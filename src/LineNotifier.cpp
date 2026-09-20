#include "LineNotifier.h"

#include <curl/curl.h>
#include <sstream>
#include <iostream>

namespace
{
    std::string JsonEscape(const std::string& text)
    {
        std::ostringstream out;

        for (unsigned char c : text)
        {
            switch (c)
            {
                case '\"': out << "\\\""; break;
                case '\\': out << "\\\\"; break;
                case '\b': out << "\\b";  break;
                case '\f': out << "\\f";  break;
                case '\n': out << "\\n";  break;
                case '\r': out << "\\r";  break;
                case '\t': out << "\\t";  break;

                default:
                    if (c < 0x20)
                    {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out << buf;
                    }
                    else
                    {
                        out << c;
                    }
                    break;
            }
        }

        return out.str();
    }

    size_t DiscardResponse(void* contents, size_t size, size_t nmemb, void* userp)
    {
        auto* body = static_cast<std::string*>(userp);
        body->append(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }
}

bool LineNotifier::Send(
    const std::string& accessToken,
    const std::string& userId,
    const std::string& message)
{
    CURL* curl = curl_easy_init();

    if(!curl)
        return false;

    struct curl_slist* headers = nullptr;

    headers = curl_slist_append(
        headers,
        ("Authorization: Bearer " + accessToken).c_str());

    headers = curl_slist_append(
        headers,
        "Content-Type: application/json");

    std::string json =
        "{"
        "\"to\":\"" + JsonEscape(userId) + "\","
        "\"messages\":["
        "{"
        "\"type\":\"text\","
        "\"text\":\"" + JsonEscape(message) + "\""
        "}"
        "]"
        "}";

    std::string responseBody;

    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        "https://api.line.me/v2/bot/message/push");

    curl_easy_setopt(
        curl,
        CURLOPT_HTTPHEADER,
        headers);

    curl_easy_setopt(
        curl,
        CURLOPT_POSTFIELDS,
        json.c_str());

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        DiscardResponse);

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEDATA,
        &responseBody);

    CURLcode res =
        curl_easy_perform(curl);

    long httpStatus = 0;

    curl_easy_getinfo(
        curl,
        CURLINFO_RESPONSE_CODE,
        &httpStatus);

    curl_slist_free_all(headers);

    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        std::cerr
            << "LINE: curl transport error: "
            << curl_easy_strerror(res)
            << "\n";

        return false;
    }

    if (httpStatus != 200)
    {
        std::cerr
            << "LINE: push rejected, HTTP "
            << httpStatus
            << ": "
            << responseBody
            << "\n";

        return false;
    }

    return true;
}
