
#ifndef CHANNEL_QUERY_SERVICE_H
#define CHANNEL_QUERY_SERVICE_H

// Include shared channel data and callback declarations.
#include "services/ChannelServiceTypes.h"

// Retrieve one channel using Redis first and PostgreSQL as fallback.
void getChannelByIdAsync(
    // ID of the channel to retrieve.
    uint32_t channelId,

    // Callback used to send the HTTP response.
    HttpResponseCallback callback
);

#endif // CHANNEL_QUERY_SERVICE_H

