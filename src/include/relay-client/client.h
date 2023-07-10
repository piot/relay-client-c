/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef RELAY_CLIENT_CLIENT_H
#define RELAY_CLIENT_CLIENT_H

#include "connector.h"
#include <relay-client/listener.h>

#define RELAY_CLIENT_LISTENER_CAPACITY (4)
#define RELAY_CLIENT_CONNECTION_CAPACITY (8)

typedef struct RelayClient {
    RelayListener listeners[RELAY_CLIENT_LISTENER_CAPACITY];
    RelayConnector connectors[RELAY_CLIENT_CONNECTION_CAPACITY];
    DatagramTransport transportToRelayServer;
    RelaySerializeUserSessionId userSessionId;
    Clog log;
    uint8_t receiveBuf[DATAGRAM_TRANSPORT_MAX_SIZE];
} RelayClient;

int relayClientInit(RelayClient* self, RelaySerializeUserSessionId authenticatedUserSessionId,
                    DatagramTransport transportToRelayServer, struct ImprintAllocator* memory, const char* prefix,
                    Clog log);

RelayListener* relayClientStartListen(RelayClient* self, RelaySerializeApplicationId applicationId,
                                      RelaySerializeChannelId channelId);
RelayConnector* relayClientStartConnect(RelayClient* self, RelaySerializeUserId userId,
                                        RelaySerializeApplicationId applicationId, RelaySerializeChannelId channelId);

int relayClientUpdate(RelayClient* self, MonotonicTimeMs now);

#endif
