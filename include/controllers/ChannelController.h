#ifndef CHANNEL_CONTROLLER_H                         // Prevent repeated inclusion.
#define CHANNEL_CONTROLLER_H                         // Define the header guard.

#include <drogon/HttpController.h>                   // Include Drogon controller support.

class ChannelController                            // Declare the channel API controller.
    : public drogon::HttpController<ChannelController> // Register it as a Drogon controller.
{
public:                                               // Expose the controller methods.
    METHOD_LIST_BEGIN                                 // Begin declaring HTTP routes.

    ADD_METHOD_TO(                                    // Register the POST endpoint.
        ChannelController::createChannelInfo,         // Handler function.
        "/createChannelInfo",                         // API path.
        drogon::Post                                  // Accepted HTTP method.
    );

    ADD_METHOD_TO(                                    // Register GET for method rejection.
        ChannelController::createChannelInfo,         // Reuse the same handler.
        "/createChannelInfo",                         // Use the same API path.
        drogon::Get                                   // HTTP GET method.
    );

    ADD_METHOD_TO(                                    // Register PUT for method rejection.
        ChannelController::createChannelInfo,         // Reuse the same handler.
        "/createChannelInfo",                         // Use the same API path.
        drogon::Put                                   // HTTP PUT method.
    );

    ADD_METHOD_TO(                                    // Register DELETE for method rejection.
        ChannelController::createChannelInfo,         // Reuse the same handler.
        "/createChannelInfo",                         // Use the same API path.
        drogon::Delete                                // HTTP DELETE method.
    );

    ADD_METHOD_TO(                                    // Register PATCH for method rejection.
        ChannelController::createChannelInfo,         // Reuse the same handler.
        "/createChannelInfo",                         // Use the same API path.
        drogon::Patch                                 // HTTP PATCH method.
    );

    ADD_METHOD_TO(                                    // Register HEAD for method rejection.
        ChannelController::createChannelInfo,         // Reuse the same handler.
        "/createChannelInfo",                         // Use the same API path.
        drogon::Head                                  // HTTP HEAD method.
    );

    ADD_METHOD_TO(                                    // Register OPTIONS for method rejection.
        ChannelController::createChannelInfo,         // Reuse the same handler.
        "/createChannelInfo",                         // Use the same API path.
        drogon::Options                               // HTTP OPTIONS method.
    );

    METHOD_LIST_END                                   // Finish declaring HTTP routes.

    void createChannelInfo(                           // Declare the request handler.
        const drogon::HttpRequestPtr &request,        // Incoming HTTP request.
        std::function<void(const drogon::HttpResponsePtr &)> &&callback // Response callback.
    );                                                // End the declaration.
};                                                    // End the controller class.

#endif                                                // End the header guard.






 
 
 
 
 
 
 
 
 
 
 
 
 
 
 /*   #pragma once

    #ifndef CHANNEL_CONTROLLER_H
    #define CHANNEL_CONTROLLER_H

    #include <drogon/HttpController.h>
    #include <functional>

    class ChannelController
        : public drogon::HttpController<ChannelController>
    {
    public:
        METHOD_LIST_BEGIN

        // Register the POST endpoint.
        ADD_METHOD_TO(
            ChannelController::createChannelInfo,
            "/createChannelInfo",
            drogon::Post
        );

        METHOD_LIST_END

        // Declare the request handler.
        void createChannelInfo(
            const drogon::HttpRequestPtr &request,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback
        );
    };

    #endif

*/