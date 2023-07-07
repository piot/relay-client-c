/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <datagram-transport/types.h>
#include <flood/in_stream.h>
#include <flood/out_stream.h>
#include <relay-client/listener.h>

void relayListenerReInit(RelayListener* self, const RelayListenerSetup* setup)
{
    self->transportToRelayServer = setup->transportToRelayServer;
    self->userSessionId = setup->authenticatedUserSessionId;
    CLOG_ASSERT(self->userSessionId != 0, "User session id can not be zero")
    self->applicationId = setup->applicationId;
    self->channelId = setup->channelId;
    self->state = RelayListenerStateConnecting;
    self->waitTime = 0;
}

static int relayListenerReceivePacket(RelayListener* self, uint8_t* outConnectionIndex, uint8_t* octets,
                                      size_t maxOctetCount)
{
    size_t count = discoidBufferReadAvailable(&self->inBuffer);
    if (count < 1) {
        return 0;
    }

    discoidBufferRead(&self->inBuffer, outConnectionIndex, sizeof(uint8_t));

    RelaySerializeConnectionId connectionId;
    discoidBufferRead(&self->inBuffer, (uint8_t*) &connectionId, sizeof(connectionId));

    uint16_t followingOctets;
    discoidBufferRead(&self->inBuffer, (uint8_t*) &followingOctets, sizeof(followingOctets));

    if (maxOctetCount < followingOctets) {
        CLOG_C_SOFT_ERROR(&self->log, "can not read incoming packet from circular buffer")
        discoidBufferSkip(&self->inBuffer, followingOctets);
        return -2;
    }

    discoidBufferRead(&self->inBuffer, octets, followingOctets);

    return followingOctets;
}

ssize_t relayListenerFindFreeConnectionIndex(RelayListener* self)
{
    for (size_t i = 0; i < RELAY_CLIENT_MAX_LISTENER_CONNECTIONS_COUNT; ++i) {
        if (self->connections[i].connectionId == 0) {
            return (ssize_t) i;
        }
    }

    return -1;
}

RelayConnection* relayListenerFindConnection(RelayListener* self, RelaySerializeConnectionId connectionId)
{
    for (size_t i = 0; i < RELAY_CLIENT_MAX_LISTENER_CONNECTIONS_COUNT; ++i) {
        if (self->connections[i].connectionId == connectionId) {
            return &self->connections[i];
        }
    }

    return 0;
}

static int sendListenRequest(RelayListener* self, FldOutStream* outStream)
{
    RelaySerializeListenRequestFromClientToServer data;
    data.channelId = self->channelId;
    data.appId = self->applicationId;
    data.requestId = ++self->requestId;
    return relaySerializeClientOutRequestListen(outStream, self->userSessionId, &data);
}

static int relayListenerSendHandshakePacket(RelayListener* self)
{
    FldOutStream outStream;
    fldOutStreamInit(&outStream, self->tempBuffer, DATAGRAM_TRANSPORT_MAX_SIZE);

    int result = 0;
    switch (self->state) {
        case RelayListenerStateConnecting:
            result = sendListenRequest(self, &outStream);
            break;
        case RelayListenerStateIdle:
        case RelayListenerStateConnected:
            break;
    }

    if (result < 0) {
        return result;
    }

    if (outStream.pos == 0) {
        return 0;
    }

    return datagramTransportSend(&self->transportToRelayServer, outStream.octets, outStream.pos);
}

static int relayListenerUpdateOut(RelayListener* self, MonotonicTimeMs now)
{
    (void) now;

    if (self->waitTime > 0) {
        self->waitTime--;
        return 0;
    }

    self->waitTime = 5;

    return relayListenerSendHandshakePacket(self);
}

int relayListenerUpdate(RelayListener* self, MonotonicTimeMs now)
{
    return relayListenerUpdateOut(self, now);
}

static int multiTransportSend(void* _self, int connectionIndex, const uint8_t* data, size_t size)
{
    RelayListener* self = (RelayListener*) _self;

    CLOG_C_DEBUG(&self->log, "sending to relay: connection:%d octetCount:%zu", connectionIndex, size)

    if (connectionIndex < 0 || connectionIndex >= RELAY_CLIENT_MAX_LISTENER_CONNECTIONS_COUNT) {
        CLOG_C_ERROR(&self->log, "illegal index %d", connectionIndex)
    }

    RelayConnection* connection = &self->connections[connectionIndex];

    return relaySocketSendPacket(self->transportToRelayServer, self->userSessionId, connection->connectionId, data,
                                 size);
}

static ssize_t multiTransportReceive(void* _self, int* receivedFromConnectionIndex, uint8_t* data, size_t size)
{
    RelayListener* self = (RelayListener*) _self;

    uint8_t receivedFromConnectionOctetIndex;
    int octetCount = relayListenerReceivePacket(self, &receivedFromConnectionOctetIndex, data, size);
    if (octetCount < 0) {
        CLOG_C_SOFT_ERROR(&self->log, "could not read in packet from relay")
    }
    if (octetCount > 0) {
        CLOG_C_DEBUG(&self->log, "got packet from relay: connection:%d octetCount:%d", *receivedFromConnectionIndex,
                     octetCount)
    }

    *receivedFromConnectionIndex = receivedFromConnectionOctetIndex;

    return octetCount;
}

int relayListenerInit(RelayListener* self, struct ImprintAllocator* memory, const char* prefix, Clog log)
{
    self->log = log;
    tc_strncpy(self->prefix, 32, prefix, tc_strlen(prefix));
    self->log.constantPrefix = self->prefix;

    self->state = RelayListenerStateIdle;
    discoidBufferInit(&self->inBuffer, memory, 32 * 1024);

    self->multiTransport.self = self;
    self->multiTransport.sendTo = multiTransportSend;
    self->multiTransport.receiveFrom = multiTransportReceive;

    for (size_t i = 0; i < RELAY_CLIENT_MAX_LISTENER_CONNECTIONS_COUNT; ++i) {
        self->connections[i].connectionId = 0;
    }

    self->waitTime = 0;

    return 0;
}

void relayListenerDestroy(RelayListener* self)
{
    (void) self;
}

void relayListenerDisconnect(RelayListener* self)
{
    (void) self;
}

ssize_t relayListenerPushPacket(RelayListener* self, size_t relayConnectionIndex, const uint8_t* data,
                                size_t octetCountInPacket)
{
    if (discoidBufferWriteAvailable(&self->inBuffer) < octetCountInPacket + sizeof(RelaySerializeConnectionId) + 2) {
        CLOG_C_NOTICE(&self->log, "dropping packets since in buffer is full")
        return 0;
    }

    if (relayConnectionIndex >= RELAY_CLIENT_MAX_LISTENER_CONNECTIONS_COUNT) {
        CLOG_ERROR("illegal index %zd", relayConnectionIndex)
        // return -4;
    }

    RelayConnection* connection = &self->connections[relayConnectionIndex];

    uint8_t connectionIndexOctet = (uint8_t) relayConnectionIndex;
    discoidBufferWrite(&self->inBuffer, (uint8_t*) &connectionIndexOctet, sizeof(connectionIndexOctet));
    discoidBufferWrite(&self->inBuffer, (uint8_t*) &connection->connectionId, sizeof(RelaySerializeConnectionId));
    discoidBufferWrite(&self->inBuffer, (uint8_t*) &octetCountInPacket, 2);
    discoidBufferWrite(&self->inBuffer, data, octetCountInPacket);

    return (ssize_t) octetCountInPacket;
}
