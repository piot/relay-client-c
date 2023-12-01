/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/relay-client-c
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#ifndef RELAY_CLIENT_LISTENER_H
#define RELAY_CLIENT_LISTENER_H

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

typedef enum RelayListenerState {
    RelayListenerStateIdle,
    RelayListenerStateConnecting,
    RelayListenerStateConnected,
} RelayListenerState;

#define RELAY_CLIENT_MAX_LISTENER_CONNECTIONS_COUNT (32)
struct ImprintAllocator;

typedef struct RelayConnection {
    RelaySerializeConnectionId connectionId;
} RelayConnection;

typedef struct RelayListener {
    int waitTime;
    RelayListenerState state;
    RelaySerializeListenerId listenerId;

    RelaySerializeApplicationId applicationId;
    RelaySerializeChannelId channelId;
    RelaySerializeRequestId requestId;

    DatagramTransportMulti multiTransport;
    RelaySerializeUserSessionId userSessionId;
    DatagramTransport transportToRelayServer;
    DiscoidBuffer inBuffer;
    RelayConnection connections[RELAY_CLIENT_MAX_LISTENER_CONNECTIONS_COUNT];
    uint8_t tempBuffer[DATAGRAM_TRANSPORT_MAX_SIZE];
    char prefix[33];
    Clog log;
} RelayListener;

typedef struct RelayListenerSetup {
    RelaySerializeUserSessionId authenticatedUserSessionId;
    RelaySerializeApplicationId applicationId;
    RelaySerializeChannelId channelId;
    DatagramTransport transportToRelayServer;
} RelayListenerSetup;

int relayListenerInit(RelayListener* self, struct ImprintAllocator* memory, const char* prefix, Clog log);
void relayListenerReInit(RelayListener* self, const RelayListenerSetup* setup);
void relayListenerDestroy(RelayListener* self);
void relayListenerDisconnect(RelayListener* self);
int relayListenerUpdate(RelayListener* self, MonotonicTimeMs now);
ssize_t relayListenerPushPacket(RelayListener* self, size_t relayConnectionIndex, const uint8_t* data,
                                size_t octetCountInPacket);
ssize_t relayListenerFindFreeConnectionIndex(RelayListener* self);
RelayConnection* relayListenerFindConnection(RelayListener* self, RelaySerializeConnectionId connectionId);
ssize_t relayListenerSendToConnectionIndex(RelayListener* self, size_t connectionIndex, const uint8_t* data,
                                           size_t octetCount);
ssize_t relayListenerReceivePacket(RelayListener* self, uint8_t* outConnectionIndex, uint8_t* octets,
                                   size_t maxOctetCount);

#endif
