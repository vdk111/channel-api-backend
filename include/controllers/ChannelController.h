#ifndef CHANNEL_CONTROLLER_H
#define CHANNEL_CONTROLLER_H

#include <drogon/HttpController.h>

#include <functional>
#include <string>

// Normal controller class.
// Routes will be registered explicitly in main.cpp.
class ChannelController
{
public:
    // Handles POST /createChannelInfo.
    void createChannelInfo(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback
    );

    // Handles GET /channels?id=101.
    void getChannelById(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback
    );

    // Handles GET /channels/101.
    void getChannelByPathId(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback,
        const std::string &channelIdText
    );
};

// Initializes the custom JSON schema registry.
void initializeChannelSchemas();

#endif // CHANNEL_CONTROLLER_H