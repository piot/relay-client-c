/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/relay-client-c
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#include <relay-client/debug.h>
#include <relay-client/listener.h>

#if defined CLOG_LOG_ENABLED

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
#if defined CLOG_LOG_ENABLED
    CLOG_C_INFO(&self->log, "state: %s", stateToString(self->state))
#else
    (void) self;
#endif
}
