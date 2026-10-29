#include <drogon/drogon.h>

#include "controllers/ChannelController.h"

#include <exception>

int main()
{
    // Get the Drogon application instance.
    auto &app = drogon::app();

    // Write Drogon's built-in logs to the logs directory.
    // The second argument is the base name of the log file.
    app.setLogPath(
        "/home/vinayak/workspace/project/channel/logs",
        "channel");

    // Include INFO, WARN, ERROR, and DEBUG messages in the logs.
    app.setLogLevel(trantor::Logger::kDebug);

    // Use local time in log timestamps.
    app.setLogLocalTime(true);

    // Log that the application is starting.
    LOG_INFO << "Application is starting";

    // Configure the PostgreSQL database connection.
    app.createDbClient(
        "postgresql",
        "127.0.0.1",
        5432,
        "channel_db",
        "app_user",
        "",
        2,
        "",
        "default",
        false,
        "",
        1.0,
        false);

    try
    {
        // Load the existing OnnetJsonParser schemas.
        initializeChannelSchemas();
    }
    catch (const std::exception &exception)
    {
        // Log the actual schema initialization error.
        LOG_ERROR << "Schema initialization failed: "
                  << exception.what();

        return 1;
    }

    // Configure the API listener.
    app.addListener("0.0.0.0", 8076);

    // Use four Drogon worker threads.
    app.setThreadNum(4);

    // Do not log request validation or database outcomes here.
    // Those messages belong in the controller and service callbacks.
    LOG_INFO << "Application configuration completed";

    app.registerHandler(
    "/createChannelInfo",
    [](const drogon::HttpRequestPtr &request,
       std::function<void(const drogon::HttpResponsePtr &)> &&callback)
    {
        // Forward the request to the existing controller handler.
        auto controller = std::make_shared<ChannelController>();
        controller->createChannelInfo(request, std::move(callback));
    },
    {drogon::Post}
);

    // Start the server and process incoming requests.
    
    app.run();

    return 0;
}
