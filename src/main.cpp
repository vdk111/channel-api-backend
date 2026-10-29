
#include <drogon/drogon.h>
#include <exception>

int main()
{
    try
    {
        // Configure the log output file and logging level.
        // The path is relative to the directory where you start the program.
        drogon::app().setLogPath(
            "/home/vinayak/workspace/project/channel_backend/logs"
        );

        // Include informational messages, warnings, and errors.
        drogon::app().setLogLevel(trantor::Logger::kInfo);

        // Configure the PostgreSQL connection.
        drogon::app().createDbClient(
            "postgresql",       //Database type
            "127.0.0.1",        //Database server address
            5432,               //PostgreSQL port
            "channel_db",       //Database name
            "app_user",         //Database username
            "",                 //empty password
            2,                  //Database connection pool size
            "default"           //Name used to retrieve this connection pool later
        );

        // Listen on port 8077.
        drogon::app().addListener("0.0.0.0", 8077);

        // Configure worker threads.
        drogon::app().setThreadNum(2);

        // Write the startup message to the logger.
        LOG_INFO << "Channel backend starting on port 8077";

        // Start the server.
        drogon::app().run();

        // Log when the server stops normally.
        LOG_INFO << "Channel backend stopped";
    }
    catch (const std::exception &error)
    {
        // Log startup failures.
        LOG_ERROR << "Channel backend failed: " << error.what();
        return 1;
    }

    return 0;
}






















/*
#include <drogon/drogon.h>  // Provides the Drogon web framework.
#include <iostream>          // Provides std::cout and std::cerr.
#include <exception>         // Provides std::exception.

int main()
{
    try
    {
        // Create a PostgreSQL connection pool.
        // Arguments: database type, host, port, database, username,
        // password, connection count, client name, and other options.
        //
        // Password is empty because this project is configured for
        // passwordless local PostgreSQL authentication.
        drogon::app().createDbClient(
            "postgresql",    // Database type.
            "127.0.0.1",     // PostgreSQL host.
            5432,            // PostgreSQL port.
            "channel_db",    // Database name.
            "app_user",      // PostgreSQL username.
            "",              // No password.
            2,               // Number of database connections.
            "default"        // Database client name.
        );

        // Listen on port 8077 on all network interfaces.
        drogon::app().addListener("0.0.0.0", 8077);

        // Configure two server threads.
        drogon::app().setThreadNum(2);

        // Print a startup message.
        std::cout << "Channel backend starting on port 8077..."
                  << std::endl;

        // Start the server.
        drogon::app().run();
    }
    catch (const std::exception &error)
    {
        // Display startup errors.
        std::cerr << "Failed to start channel backend: "
                  << error.what() << std::endl;

        return 1;
    }

    return 0;
}

*/