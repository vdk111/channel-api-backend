
#ifndef CHANNEL_SERVICE_TYPES_H
#define CHANNEL_SERVICE_TYPES_H

// Include Drogon HTTP request and response types.
#include <drogon/drogon.h>

// Include fixed-width integer types such as uint32_t.
#include <cstdint>

// Include the function wrapper used for callbacks.
#include <functional>

// Store validated channel information.
struct ChannelData
{
    // Unique channel ID.
    uint32_t channelId;

    // Human-readable channel name.
    std::string channelName;

    // Channel type, currently 0 or 1.
    uint32_t channelType;

    // Channel status, currently 0 or 1.
    uint32_t status;
};

// Define the callback used to return an HTTP response.
using HttpResponseCallback =
    std::function<void(const drogon::HttpResponsePtr &)>;

#endif // CHANNEL_SERVICE_TYPES_H

