/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef RELAY_CLIENT_CONNECTOR_H
#define RELAY_CLIENT_CONNECTOR_H

#include <clog/clog.h>
#include <datagram-transport/multi.h>
#include <datagram-transport/transport.h>
#include <datagram-transport/types.h>
#include <discoid/circular_buffer.h>
#include <monotonic-time/monotonic_time.h>
#include <relay-client/socket.h>
#include <relay-serialize/client_out.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct ImprintAllocator;

struct FldOutStream;

typedef enum RelayConnectorState {
    RelayConnectorStateIdle,
    RelayConnectorStateConnecting,
    RelayConnectorStateConnected,
} RelayConnectorState;

#define RELAY_CLIENT_MAX_LOCAL_USERS_COUNT (8)

struct ImprintAllocator;

typedef struct RelayConnector {
    RelayConnectorState state;
    Clog log;
    RelaySerializeConnectionId connectionId;
    RelaySerializeUserSessionId userSessionId;

    RelaySerializeUserId connectToUserId;
    RelaySerializeApplicationId applicationId;
    RelaySerializeChannelId channelId;

    DatagramTransport transportToRelayServer;
    DatagramTransport connectorTransport;
    DiscoidBuffer inBuffer;
    RelaySerializeRequestId requestId;

    size_t waitTime;
    uint8_t tempBuffer[DATAGRAM_TRANSPORT_MAX_SIZE];
} RelayConnector;

int relayConnectorInit(RelayConnector* self, struct ImprintAllocator* memory, Clog log);
void relayConnectorReset(RelayConnector* self);
void relayConnectorReInit(RelayConnector* self, DatagramTransport* transportToRelayServer,
                          RelaySerializeUserSessionId userSessionId, RelaySerializeUserId userId,
                          RelaySerializeApplicationId applicationId, RelaySerializeChannelId channelId);
void relayConnectorDestroy(RelayConnector* self);
void relayConnectorDisconnect(RelayConnector* self);
int relayConnectorUpdate(RelayConnector* self, MonotonicTimeMs now);
int relayConnectorPushPacket(RelayConnector* self, const uint8_t* data, size_t octetCountInPacket);

#endif
