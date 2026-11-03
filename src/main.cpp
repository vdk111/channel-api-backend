#include <drogon/drogon.h>

#include "controllers/ChannelController.h"

#include <exception>
#include <memory>
#include <utility>

int main()
{
    // Get the Drogon application instance.
    auto &app = drogon::app();

    // Configure the directory where application logs are stored.
    app.setLogPath(
        "/home/vinayak/workspace/project/channel_backup/logs",
        "channel_backup"
    );

    // Enable debug-level logging.
    app.setLogLevel(trantor::Logger::kDebug);

    // Use local time in log timestamps.
    app.setLogLocalTime(true);

    // Log that the application is starting.
    LOG_INFO << "Application is starting";

    // Configure the PostgreSQL database connection.
    app.createDbClient(
        "postgresql",   // Database type.
        "127.0.0.1",    // PostgreSQL server address.
        5432,           // PostgreSQL port.
        "channel_db",   // Database name.
        "app_user",     // Database username.
        "",             // Database password.
        2,              // Number of PostgreSQL connections in the pool.
        "",             // Additional configuration file.
        "default",      // Name of this database client.
        false,          // Disable fast mode.
        "",             // Character set.
        1.0,            // Connection timeout in seconds.
        false           // Disable automatic ORM mapping.
    );

    // Create a Redis client before the application starts.
    app.createRedisClient(
        "127.0.0.1",    // Redis server address.
        6379,           // Redis server port.
        "default",      // Name of the Redis client.
        "",             // Redis password (empty if authentication is disabled).
        2,              // Number of Redis connections.
        false,          // Disable fast mode.
        1.0             // Redis command timeout in seconds.
    );

    // Log successful Redis client configuration.
    LOG_INFO << "Redis client configured";

    try
    {
        // Load the OnnetJsonParser schemas.
        initializeChannelSchemas();
    }
    catch (const std::exception &exception)
    {
        // Log the reason if schema initialization fails.
        LOG_ERROR << "Schema initialization failed: "
                  << exception.what();

        // Stop application startup.
        return 1;
    }

    // Create one shared controller instance.
    auto controller = std::make_shared<ChannelController>();

    // Register the POST /createChannelInfo endpoint.
    app.registerHandler(
        "/createChannelInfo",
        [controller](
            const drogon::HttpRequestPtr &request,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback
        )
        {
            // Pass the request to the controller.
            controller->createChannelInfo(
                request,
                std::move(callback)
            );
        },
        {drogon::Post}
    );

    // Register the GET /channels?id=101 endpoint.
    app.registerHandler(
        "/channels",
        [controller](
            const drogon::HttpRequestPtr &request,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback
        )
        {
            // Pass the request to the controller.
            controller->getChannelById(
                request,
                std::move(callback)
            );
        },
        {drogon::Get}
    );

    // Register the GET /channels/101 endpoint.
    app.registerHandler(
        "/channels/{1}",
        [controller](
            const drogon::HttpRequestPtr &request,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback,
            const std::string &channelIdText
        )
        {
            // Pass the request and path ID to the controller.
            controller->getChannelByPathId(
                request,
                std::move(callback),
                channelIdText
            );
        },
        {drogon::Get}
    );

    // Listen for HTTP requests on port 8075.
    app.addListener("0.0.0.0", 8075);

    // Use four HTTP worker threads.
    app.setThreadNum(4);

    // Log the registered API routes.
    LOG_INFO << "POST route registered: /createChannelInfo";
    LOG_INFO << "GET route registered: /channels";
    LOG_INFO << "GET route registered: /channels/{1}";

    // Log that the application configuration is complete.
    LOG_INFO << "Application configuration completed";

    // Start the HTTP server.
    app.run();

    // Return success when the server stops.
    return 0;
}
