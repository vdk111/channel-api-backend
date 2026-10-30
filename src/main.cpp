#include <drogon/drogon.h>

#include "controllers/ChannelController.h"

#include <exception>
#include <memory>

int main()
{
    // Get the Drogon application instance.
    auto &app = drogon::app();

    // Configure the log directory and filename prefix.
    app.setLogPath(
        "/home/vinayak/workspace/project/channel_backup/logs",
        "channel_backup"
    );

    // Enable application logging.
    app.setLogLevel(trantor::Logger::kDebug);

    // Use local timestamps.
    app.setLogLocalTime(true);

    LOG_INFO << "Application is starting";

    // Configure the PostgreSQL connection.
    app.createDbClient(
        "postgresql",   // Type of database we are connecting to (PostgreSQL)
        "127.0.0.1",    // IP address of the database server (localhost)
        5432,           // Port number on which PostgreSQL is running
        "channel_db",   // Name of the database we want to connect to
        "app_user",     // Username used to log in to PostgreSQL
        "",             // Password for the database user (currently empty)
        2,              // Number of database connections created in the connection pool
        "",             // Additional database configuration (empty value means default)
        "default",      // Database client name used to identify this connection
        false,          // Disable automatic reconnection
        "",             // Additional database configuration (empty value means default)
        1.0,            // Database connection timeout in seconds
        false           // Disable automatic connection to a database through the ORM
        
        );

    try
    {
        // Load the OnnetJsonParser schemas.
        initializeChannelSchemas();
    }
    catch (const std::exception &exception)
    {
        LOG_ERROR << "Schema initialization failed: "
                  << exception.what();

        return 1;
    }

    // Create one controller instance.
    auto controller = std::make_shared<ChannelController>();

    // Explicitly register POST /createChannelInfo.
    app.registerHandler(
        "/createChannelInfo",
        [controller](
            const drogon::HttpRequestPtr &request,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback
        )
        {
            controller->createChannelInfo(
                request,
                std::move(callback)
            );
        },
        {drogon::Post}
    );

    // Explicitly register GET /channels?id=101.
    app.registerHandler(
        "/channels",
        [controller](
            const drogon::HttpRequestPtr &request,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback
        )
        {
            controller->getChannelById(
                request,
                std::move(callback)
            );
        },
        {drogon::Get}
    );

    // Explicitly register GET /channels/101.
    app.registerHandler(
        "/channels/{1}",
        [controller](
            const drogon::HttpRequestPtr &request,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback,
            const std::string &channelIdText
        )
        {
            controller->getChannelByPathId(
                request,
                std::move(callback),
                channelIdText
            );
        },
        {drogon::Get}
    );

    // Configure the server listener.
    app.addListener("0.0.0.0", 8075);

    // Use four worker threads.
    app.setThreadNum(4);

    LOG_INFO << "POST route registered: /createChannelInfo";
    LOG_INFO << "GET route registered: /channels";
    LOG_INFO << "GET route registered: /channels/{1}";
    LOG_INFO << "Application configuration completed";

    // Start the HTTP server.
    app.run();

    return 0;
}