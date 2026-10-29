#ifndef CHANNEL_SERVICE_H
#define CHANNEL_SERVICE_H

#include <drogon/drogon.h>

#include <cstdint>
#include <functional>
#include <string>

// Holds the validated channel values that will be inserted into PostgreSQL.
struct ChannelData
{
    uint32_t channelId;
    std::string channelName;
    uint32_t channelType;
    uint32_t status;
};

// Represents the callback used to send an HTTP response.
using HttpResponseCallback =
    std::function<void(const drogon::HttpResponsePtr &)>;

// Inserts channel data into PostgreSQL without blocking the request thread.
void insertChannelInfoAsync(
    ChannelData data,
    HttpResponseCallback callback);

#endif // CHANNEL_SERVICE_H
