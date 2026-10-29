#ifndef CHANNEL_CONTROLLER_H
#define CHANNEL_CONTROLLER_H

#include <drogon/HttpController.h>
#include <functional>

// Drogon automatically registers this controller's routes.
class ChannelController : public drogon::HttpController<ChannelController>
{
public:
    METHOD_LIST_BEGIN

    // Register POST /createChannelInfo.
    METHOD_ADD(
        ChannelController::createChannelInfo,
        "/createChannelInfo",
        drogon::Post
    );

    METHOD_LIST_END

    // Handle requests sent to POST /createChannelInfo.
    void createChannelInfo(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback
    );
};

// Load and verify the OnnetJsonParser schema.
void initializeChannelSchemas();

#endif
































/*
#ifndef CHANNEL_CONTROLLER_H
#define CHANNEL_CONTROLLER_H

#include <drogon/drogon.h>

#include <functional>

// Controller for the channel API.
class ChannelController
    : public drogon::HttpController<ChannelController>
{
public:
    // Register the POST /createChannelInfo endpoint.
    METHOD_LIST_BEGIN

    METHOD_ADD(
        ChannelController::createChannelInfo,
        "/createChannelInfo",
        drogon::Post);

    METHOD_LIST_END

    // Handle requests to create channel information.
    void createChannelInfo(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};

// Load and initialize the API schemas before the server starts.
void initializeChannelSchemas();

#endif // CHANNEL_CONTROLLER_H
*/