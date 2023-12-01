/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/relay-client-c
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#ifndef RELAY_CLIENT_SOCKET_H
#define RELAY_CLIENT_SOCKET_H

#include <clog/clog.h>
#include <datagram-transport/multi.h>
#include <datagram-transport/transport.h>
#include <discoid/circular_buffer.h>
#include <monotonic-time/monotonic_time.h>
#include <relay-serialize/client_out.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

int relaySocketSendPacket(DatagramTransport transportToRelayServer, RelaySerializeUserSessionId userSessionId,
                          RelaySerializeConnectionId connectionId, const uint8_t* octets, size_t octetCount);
#endif
