/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <relay-client/debug.h>
#include <relay-client/listener.h>

#if defined CONFIGURATION_DEBUG

static const char* stateToString(RelayListenerState state)
{
    switch (state) {
        case RelayListenerStateIdle:
            return "idle";
        case RelayListenerStateConnecting:
            return "connecting";
        case RelayListenerStateConnected:
            return "connected";
    }

    return "unknown";
}

#endif

void relayListenerDebugOutput(const RelayListener* self)
{
    (void) self;
    CLOG_C_INFO(&self->log, "state: %s", stateToString(self->state))
}
