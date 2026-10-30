#ifndef CHANNEL_SERVICE_H
#define CHANNEL_SERVICE_H

#include <drogon/drogon.h>

#include <cstdint>
#include <functional>
#include <string>

// Holds validated channel data passed from the controller to the service.
struct ChannelData
{
    uint32_t channelId;
    std::string channelName;
    uint32_t channelType;
    uint32_t status;
};

// Callback used by the service to return an HTTP response.
using HttpResponseCallback =
    std::function<void(const drogon::HttpResponsePtr &)>;

// Inserts a channel into PostgreSQL asynchronously.
void insertChannelInfoAsync(
    ChannelData data,
    HttpResponseCallback callback
);

// Retrieves a channel from PostgreSQL asynchronously using its ID.
void getChannelByIdAsync(
    uint32_t channelId,
    HttpResponseCallback callback
);

#endif // CHANNEL_SERVICE_H