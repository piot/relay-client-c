/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <datagram-transport/types.h>
#include <flood/out_stream.h>
#include <inttypes.h>
#include <relay-client/connector.h>

static int sendConnectRequest(RelayConnector* self, FldOutStream* outStream)
{
    RelaySerializeConnectRequestFromClientToServer data;
    data.connectToUserId = self->connectToUserId;
    data.appId = self->applicationId;
    data.channelId = self->channelId;
    data.requestId = ++self->requestId;

    self->waitTime = 10;

    CLOG_C_DEBUG(&self->log, "sending connect request to userId %" PRIX64, data.connectToUserId)

    return relaySerializeClientOutRequestConnect(outStream, self->userSessionId, &data);
}

static int relayConnectorSendHandshakePacket(RelayConnector* self)
{
    FldOutStream outStream;
    fldOutStreamInit(&outStream, self->tempBuffer, DATAGRAM_TRANSPORT_MAX_SIZE);

    int result = 0;
    switch (self->state) {
        case RelayConnectorStateConnecting:
            result = sendConnectRequest(self, &outStream);
            break;
        case RelayConnectorStateIdle:
        case RelayConnectorStateConnected:
            break;
    }

    if (result < 0) {
        return result;
    }

    if (outStream.pos == 0) {
        return 0;
    }

    CLOG_C_DEBUG(&self->log, "sending handshake %zd", outStream.pos)

    return datagramTransportSend(&self->transportToRelayServer, outStream.octets, outStream.pos);
}

static int relayConnectorUpdateOut(RelayConnector* self, MonotonicTimeMs now)
{
    (void) now;

    if (self->waitTime > 0) {
        self->waitTime--;
        return 0;
    }

    self->waitTime = 5;

    return relayConnectorSendHandshakePacket(self);
}

int relayConnectorUpdate(RelayConnector* self, MonotonicTimeMs now)
{
    return relayConnectorUpdateOut(self, now);
}

static int transportSend(void* _self, const uint8_t* data, size_t size)
{
    RelayConnector* self = (RelayConnector*) _self;

    CLOG_C_DEBUG(&self->log, "sending to relay: octetCount:%zu", size)

    return relaySocketSendPacket(self->transportToRelayServer, self->userSessionId, self->connectionId, data, size);
}

static ssize_t relayConnectorReceivePacket(RelayConnector* self, uint8_t* octets, size_t maxOctetCount)
{
    size_t count = discoidBufferReadAvailable(&self->inBuffer);
    if (count < sizeof(uint16_t)) {
        return 0;
    }

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

static ssize_t transportReceive(void* _self, uint8_t* data, size_t size)
{
    RelayConnector* self = (RelayConnector*) _self;

    ssize_t octetCount = relayConnectorReceivePacket(self, data, size);
    if (octetCount < 0) {
        CLOG_C_SOFT_ERROR(&self->log, "could not read in packet from relay")
    }
    if (octetCount > 0) {
        CLOG_C_DEBUG(&self->log, "got packet from relay: octetCount:%zd", octetCount)
    }

    return octetCount;
}

int relayConnectorPushPacket(RelayConnector* self, const uint8_t* data, size_t octetCountInPacket)
{
    if (discoidBufferWriteAvailable(&self->inBuffer) < octetCountInPacket + sizeof(uint16_t)) {
        CLOG_C_NOTICE(&self->log, "dropping packets since in buffer is full")
        return -1;
    }

    discoidBufferWrite(&self->inBuffer, (uint8_t*) &octetCountInPacket, sizeof(uint16_t));
    discoidBufferWrite(&self->inBuffer, data, octetCountInPacket);

    return 0;
}

int relayConnectorInit(RelayConnector* self, struct ImprintAllocator* memory, Clog log)
{
    self->log = log;
    self->state = RelayConnectorStateIdle;
    self->connectorTransport.self = self;
    self->connectorTransport.send = transportSend;
    self->connectorTransport.receive = transportReceive;
    discoidBufferInit(&self->inBuffer, memory, 32 * 1024);

    return 0;
}

void relayConnectorReInit(RelayConnector* self, DatagramTransport* transportToRelayServer, RelaySerializeUserId userId,
                          RelaySerializeApplicationId applicationId, RelaySerializeChannelId channelId)
{
    self->transportToRelayServer = *transportToRelayServer;
    self->state = RelayConnectorStateConnecting;
    self->connectToUserId = userId;
    self->applicationId = applicationId;
    self->channelId = channelId;

    discoidBufferReset(&self->inBuffer);
}
