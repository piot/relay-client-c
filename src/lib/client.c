/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <datagram-transport/types.h>
#include <flood/in_stream.h>
#include <inttypes.h>
#include <relay-client/client.h>
#include <relay-client/listener.h>
#include <relay-serialize/client_in.h>
#include <relay-serialize/client_out.h>
#include <relay-serialize/debug.h>
#include <relay-serialize/serialize.h>

static int relayClientFindListenerAndConnection(RelayClient* self, RelaySerializeConnectionId connectionId,
                                                RelayListener** outListener, size_t* outConnectionIndex)

{
    for (size_t i = 0; i < RELAY_CLIENT_LISTENER_CAPACITY; ++i) {
        RelayListener* listener = &self->listeners[i];
        for (size_t connectionIndex = 0; connectionIndex < RELAY_CLIENT_MAX_LISTENER_CONNECTIONS_COUNT;
             ++connectionIndex) {
            RelayConnection* connection = &listener->connections[connectionIndex];
            if (connection->connectionId == connectionId) {
                *outConnectionIndex = connectionIndex;
                *outListener = listener;
                return 1;
            }
        }
    }

    *outConnectionIndex = 0;
    *outListener = 0;

    return -1;
}

/*
static int relayClientFindListenerAndConnectionEx(RelayClient* self, RelayListener listenerId,
                                                  RelaySerializeConnectionId connectionId, RelayListener** outListener,
                                                  RelayConnection** outConnection)

{
    (void) listenerId;
    for (size_t i = 0; i < RELAY_CLIENT_LISTENER_CAPACITY; ++i) {
        RelayListener* listener = &self->listeners[i];
        for (size_t connectionIndex = 0; connectionIndex < RELAY_CLIENT_MAX_LISTENER_CONNECTIONS_COUNT;
             ++connectionIndex) {
            RelayConnection* connection = &listener->connections[connectionIndex];
            if (connection->connectionId == connectionId) {
                *outConnection = connection;
                *outListener = listener;
                return 1;
            }
        }
    }

    *outConnection = 0;
    *outListener = 0;

    return -1;
}
*/

static RelayConnector* relayClientFindConnector(RelayClient* self, RelaySerializeConnectionId connectionId)

{
    for (size_t connectorIndex = 0; connectorIndex < RELAY_CLIENT_CONNECTION_CAPACITY; ++connectorIndex) {
        RelayConnector* connector = &self->connectors[connectorIndex];
        if (connector->connectionId == connectionId) {
            return connector;
        }
    }

    return 0;
}

static int onIncomingPacket(RelayClient* self, FldInStream* inStream)
{
    RelaySerializeServerPacketFromServerToClient packetFromServerToClient;

    int packetHeaderErr = relaySerializeClientInPacketFromServer(inStream, &packetFromServerToClient);
    if (packetHeaderErr < 0) {
        return packetHeaderErr;
    }

    RelayListener* listener;
    size_t connectionIndex;

    int lookupErr = relayClientFindListenerAndConnection(self, packetFromServerToClient.connectionId, &listener,
                                                         &connectionIndex);
    if (lookupErr >= 0) {
        ssize_t octetsWritten = relayListenerPushPacket(listener, connectionIndex, inStream->p,
                                                        packetFromServerToClient.packetOctetCount);
        if (octetsWritten < 0) {
            return (int) octetsWritten;
        }

        return 0;
    }

    RelayConnector* connector = relayClientFindConnector(self, packetFromServerToClient.connectionId);
    if (connector == 0) {
        return -2;
    }

    ssize_t octetsWritten = relayConnectorPushPacket(connector, inStream->p, packetFromServerToClient.packetOctetCount);

    if (octetsWritten < 0) {
        return (int) octetsWritten;
    }

    return 0;
}

static RelayListener* relayClientFindListener(RelayClient* self, RelaySerializeListenerId listenerId)
{
    for (size_t i = 0; i < RELAY_CLIENT_LISTENER_CAPACITY; ++i) {
        if (self->listeners[i].listenerId == listenerId) {
            return &self->listeners[i];
        }
    }

    return 0;
}

static RelayListener* relayClientFindFreeListener(RelayClient* self)
{
    for (size_t i = 0; i < RELAY_CLIENT_LISTENER_CAPACITY; ++i) {
        if (self->listeners[i].state == RelayListenerStateIdle) {
            return &self->listeners[i];
        }
    }

    return 0;
}

static RelayConnector* relayClientFindFreeConnector(RelayClient* self)
{
    for (size_t i = 0; i < RELAY_CLIENT_CONNECTION_CAPACITY; ++i) {
        if (self->connectors[i].state == RelayConnectorStateIdle) {
            return &self->connectors[i];
        }
    }

    return 0;
}

static RelayListener* relayClientFindListenerByAppAndChannel(RelayClient* self, RelaySerializeApplicationId appId,
                                                             RelaySerializeChannelId channelId,
                                                             RelaySerializeRequestId requestId)
{
    for (size_t i = 0; i < RELAY_CLIENT_LISTENER_CAPACITY; ++i) {
        RelayListener* listener = &self->listeners[i];
        if (listener->listenerId != 0) {
            continue;
        }
        if (listener->applicationId == appId && listener->channelId == channelId) {
            if (listener->requestId != requestId) {
                CLOG_C_NOTICE(&self->log, "not answer to same requestedID, but accepting it anyway")
            }
            return listener;
        }
    }
    return 0;
}

static RelayConnector* relayClientFindConnectorUsingRequestId(RelayClient* self,
                                                              RelaySerializeRequestId connectorRequestId)
{
    for (size_t i = 0; i < RELAY_CLIENT_CONNECTION_CAPACITY; ++i) {
        RelayConnector* connector = &self->connectors[i];
        if (connector->requestId == connectorRequestId) {
            return connector;
        }
    }

    return 0;
}

static int onConnectorResponse(RelayClient* self, FldInStream* inStream)
{
    RelaySerializeConnectResponseFromServerToClient data;
    int err = relaySerializeClientInConnectResponse(inStream, &data);
    if (err < 0) {
        return err;
    }

    RelayConnector* connector = relayClientFindConnectorUsingRequestId(self, data.requestId);
    if (connector == 0) {
        CLOG_C_SOFT_ERROR(&self->log, "strange, got connect response for an unknown connector")
        return -6;
    }

    if (connector->state == RelayConnectorStateConnecting) {
        CLOG_C_DEBUG(&self->log, "connector is connected to the relay server")
        connector->state = RelayConnectorStateConnected;
    }

    return 0;
}

static int onListenResponse(RelayClient* self, FldInStream* inStream)
{
    RelaySerializeListenResponseFromServerToListener data;
    int err = relaySerializeClientInListenResponse(inStream, &data);
    if (err < 0) {
        return err;
    }

    RelayListener* listener = relayClientFindListenerByAppAndChannel(self, data.appId, data.channelId, data.requestId);
    if (listener == 0) {
        CLOG_C_SOFT_ERROR(&self->log, "got listen response, but I have no listener waiting for that")
        return -5;
    }

    listener->listenerId = data.listenerId;
    listener->state = RelayListenerStateConnected;
    CLOG_C_DEBUG(&self->log, "listener connected to relay %" PRIX64, listener->listenerId)

    return 0;
}

static int onConnectionRequestToListener(RelayClient* self, FldInStream* inStream)
{
    RelaySerializeConnectRequestFromServerToListener data;

    int err = relaySerializeClientInConnectRequestToListener(inStream, &data);
    if (err < 0) {
        return err;
    }

    RelayListener* listener = relayClientFindListener(self, data.listenerId);
    if (listener == 0) {
        CLOG_SOFT_ERROR("suspicious, we got a connection request for a listener we don't know about %" PRIx64,
                        data.listenerId)
        return -5;
    }

    RelayConnection* connection = relayListenerFindConnection(listener, data.connectionId);
    if (connection == 0) {
        ssize_t foundIndex = relayListenerFindFreeConnectionIndex(listener);
        if (foundIndex < 0) {
            CLOG_SOFT_ERROR("out of connection capacity")
            return -4;
        }
        connection = &listener->connections[foundIndex];
        connection->connectionId = data.connectionId;
    }

    return 0;
}

static int relayClientFeed(RelayClient* self, const uint8_t* data, size_t len)
{
    FldInStream inStream;
    fldInStreamInit(&inStream, data, len);

    uint8_t cmd;
    fldInStreamReadUInt8(&inStream, &cmd);
    CLOG_C_VERBOSE(&self->log, "cmd: %s", relaySerializeCmdToString(cmd))
    switch (cmd) {
        case relaySerializeCmdPacketToClient:
            return onIncomingPacket(self, &inStream);
        case relaySerializeCmdConnectionRequestToClient:
            return onConnectionRequestToListener(self, &inStream);
        case relaySerializeCmdListenResponseToClient:
            return onListenResponse(self, &inStream);
        case relaySerializeCmdConnectResponseToClient:
            return onConnectorResponse(self, &inStream);
        default:
            CLOG_C_ERROR(&self->log, "relayClientFeed: unknown message %02X", cmd)
            // return -1;
    }
}

static int relayClientReceiveAllDatagramsFromRelayServer(RelayClient* self)
{
    size_t count = 0;
    for (size_t i = 0; i < 30; ++i) {
        ssize_t octetCount = datagramTransportReceive(&self->transportToRelayServer, self->receiveBuf,
                                                      DATAGRAM_TRANSPORT_MAX_SIZE);
        if (octetCount > 0) {
            relayClientFeed(self, self->receiveBuf, (size_t) octetCount);
            count++;
        } else if (octetCount < 0) {
            CLOG_C_SOFT_ERROR(&self->log, "error: %zd", octetCount)
            return (int) octetCount;
        } else {
            break;
        }
    }

    return (int) count;
}

int relayClientInit(RelayClient* self, RelaySerializeUserSessionId authenticatedUserSessionId,
                    DatagramTransport transportToRelayServer, struct ImprintAllocator* memory, const char* prefix,
                    Clog log)
{
    char temp[32];

    (void) prefix;

    for (size_t i = 0; i < RELAY_CLIENT_LISTENER_CAPACITY; ++i) {
        tc_snprintf(temp, 32, "listener-%zu", i);
        relayListenerInit(&self->listeners[i], memory, temp, log);
    }

    for (size_t i = 0; i < RELAY_CLIENT_CONNECTION_CAPACITY; ++i) {
        tc_snprintf(temp, 32, "connector-%zu", i);
        relayConnectorInit(&self->connectors[i], memory, log);
    }

    self->userSessionId = authenticatedUserSessionId;
    CLOG_ASSERT(authenticatedUserSessionId != 0, "user session id can not be zero")
    self->transportToRelayServer = transportToRelayServer;
    self->log = log;

    return 0;
}

RelayListener* relayClientStartListen(RelayClient* self, RelaySerializeApplicationId applicationId,
                                      RelaySerializeChannelId channelId)
{
    RelayListener* listener = relayClientFindFreeListener(self);
    if (listener == 0) {
        return 0;
    }

    RelayListenerSetup setup;

    setup.channelId = channelId;
    setup.applicationId = applicationId;
    setup.transportToRelayServer = self->transportToRelayServer;
    setup.authenticatedUserSessionId = self->userSessionId;
    relayListenerReInit(listener, &setup);

    return listener;
}

RelayConnector* relayClientStartConnect(RelayClient* self, RelaySerializeUserId userId,
                                        RelaySerializeApplicationId applicationId, RelaySerializeChannelId channelId)
{
    RelayConnector* connector = relayClientFindFreeConnector(self);
    if (connector == 0) {
        return 0;
    }

    relayConnectorReInit(connector, &self->transportToRelayServer, self->userSessionId, userId, applicationId,
                         channelId);

    return connector;
}

int relayClientUpdate(RelayClient* self, MonotonicTimeMs now)
{
    (void) now;

    int receiveErr = relayClientReceiveAllDatagramsFromRelayServer(self);
    if (receiveErr < 0) {
        return receiveErr;
    }

    for (size_t i = 0; i < RELAY_CLIENT_LISTENER_CAPACITY; ++i) {
        RelayListener* listener = &self->listeners[i];
        if (listener->state == RelayListenerStateConnecting) {
            relayListenerUpdate(listener, now);
        }
    }

    for (size_t connectorIndex = 0; connectorIndex < RELAY_CLIENT_CONNECTION_CAPACITY; ++connectorIndex) {
        RelayConnector* connector = &self->connectors[connectorIndex];
        if (connector->state == RelayConnectorStateConnecting) {
            relayConnectorUpdate(connector, now);
        }
    }

    return 0;
}
