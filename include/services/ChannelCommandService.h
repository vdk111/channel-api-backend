
#ifndef CHANNEL_COMMAND_SERVICE_H
#define CHANNEL_COMMAND_SERVICE_H

// Include shared channel data and callback declarations.
#include "services/ChannelServiceTypes.h"

// Insert channel information into PostgreSQL asynchronously.
void insertChannelInfoAsync(
    // The validated channel information to insert.
    ChannelData data,

    // Callback used to send the HTTP response.
    HttpResponseCallback callback
);

#endif // CHANNEL_COMMAND_SERVICE_H

