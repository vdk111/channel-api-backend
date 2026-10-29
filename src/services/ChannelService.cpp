#include "services/ChannelService.h"

#include <drogon/drogon.h>

#include <exception>
#include <memory>
#include <string>
#include <utility>

namespace
{
    // Creates an HTTP response containing JSON.
    drogon::HttpResponsePtr makeServiceResponse(
        drogon::HttpStatusCode status,
        const std::string &body)
    {
        auto response = drogon::HttpResponse::newHttpResponse();
        response->setStatusCode(status);
        response->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        response->setBody(body);
        return response;
    }

    // Handles a completed database query.
    void handleInsertSuccess(
        const drogon::orm::Result &result,
        const std::shared_ptr<HttpResponseCallback> &callback,
        uint32_t channelId)
    {
        // With ON CONFLICT DO NOTHING, an empty result means
        // that the channel ID already exists.
        if (result.empty())
        {
            LOG_WARN << "POST /createChannelInfo"
                     << " - database insert skipped: duplicate channel_id"
                     << " - channel_id: " << channelId;

            (*callback)(makeServiceResponse(
                drogon::k409Conflict,
                R"({"success":false,"error":"DUPLICATE_CHANNEL_ID","field":"channel_id","message":"Channel ID already exists"})"));

            return;
        }

        // A row was returned, so PostgreSQL inserted the channel.
        LOG_INFO << "POST /createChannelInfo"
                 << " - database insertion successful"
                 << " - channel_id: " << channelId;

        (*callback)(makeServiceResponse(
            drogon::k201Created,
            R"({"success":true,"message":"Channel information created successfully"})"));
    }

    // Handles a failed database query.
    void handleInsertError(
        const std::exception_ptr &error,
        const std::shared_ptr<HttpResponseCallback> &callback,
        uint32_t channelId)
    {
        // Log the database exception using Drogon's built-in logger.
        try
        {
            if (error)
            {
                std::rethrow_exception(error);
            }

            LOG_ERROR << "POST /createChannelInfo"
                      << " - database insertion failed"
                      << " - channel_id: " << channelId
                      << " - no exception details were supplied";
        }
        catch (const std::exception &exception)
        {
            LOG_ERROR << "POST /createChannelInfo"
                      << " - database insertion failed"
                      << " - channel_id: " << channelId
                      << " - reason: " << exception.what();
        }
        catch (...)
        {
            LOG_ERROR << "POST /createChannelInfo"
                      << " - database insertion failed"
                      << " - channel_id: " << channelId
                      << " - unknown database error";
        }

        // Preserve the API's database-error response.
        (*callback)(makeServiceResponse(
            drogon::k500InternalServerError,
            R"({"success":false,"error":"DATABASE_ERROR","message":"Database operation failed"})"));
    }
}

// Inserts channel data asynchronously into PostgreSQL.
void insertChannelInfoAsync(
    ChannelData data,
    HttpResponseCallback callback)
{
    // Use SQL parameters to prevent user input from becoming SQL code.
    const std::string sql =
        "INSERT INTO channel_info "
        "(channel_id, channel_name, channel_type, status) "
        "VALUES ($1, $2, $3, $4) "
        "ON CONFLICT (channel_id) DO NOTHING "
        "RETURNING channel_id";

    // Keep the ID for logging in the asynchronous callbacks.
    const uint32_t channelId = data.channelId;

    // Share the HTTP callback with the asynchronous handlers.
    auto sharedCallback =
        std::make_shared<HttpResponseCallback>(std::move(callback));

    // Submit the query without blocking the request-handling thread.
    drogon::app().getDbClient()->execSqlAsync(
        sql,

        // Called when PostgreSQL successfully executes the query.
        [sharedCallback, channelId](
            const drogon::orm::Result &result)
        {
            handleInsertSuccess(
                result,
                sharedCallback,
                channelId);
        },

        // Called when PostgreSQL reports a query error.
        [sharedCallback, channelId](
            const std::exception_ptr &error)
        {
            handleInsertError(
                error,
                sharedCallback,
                channelId);
        },

        // Bind values to SQL placeholders $1 through $4.
        data.channelId,
        data.channelName,
        data.channelType,
        data.status);
}
