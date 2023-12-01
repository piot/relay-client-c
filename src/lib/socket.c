/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/relay-client-c
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#include <datagram-transport/types.h>
#include <flood/out_stream.h>
#include <relay-client/socket.h>


int relaySocketSendPacket(DatagramTransport transportToRelayServer, RelaySerializeUserSessionId userSessionId,
                          RelaySerializeConnectionId connectionId, const uint8_t* octets, size_t octetCount)
{
    uint8_t buf[DATAGRAM_TRANSPORT_MAX_SIZE];
    FldOutStream outStream;
    fldOutStreamInit(&outStream, buf, DATAGRAM_TRANSPORT_MAX_SIZE);

    RelaySerializeServerPacketFromClientToServer packetHeader;
    packetHeader.connectionId = connectionId;
    packetHeader.packetOctetCount = (uint16_t) octetCount;

    relaySerializeClientOutPacketToServerHeader(&outStream, userSessionId, packetHeader);
    fldOutStreamWriteOctets(&outStream, octets, octetCount);

    return transportToRelayServer.send(transportToRelayServer.self, outStream.octets, outStream.pos);
}
