#include "curl.hpp"

#include <cstring>
#include <stdexcept>

osmview::curl_easy::curl_easy() : handle_(curl_easy_init())
{
    if (!handle_)
    {
        throw std::runtime_error("Curl: error in curl_easy_init");
    }
    curl_easy_setopt(handle(), CURLOPT_ERRORBUFFER, error_buffer);
    // this is needed for mutithreaded applications
    curl_easy_setopt(handle(), CURLOPT_NOSIGNAL, 1l);
}

void osmview::curl_easy::setup_download(const char *url,
                                        osmview::curl_easy::callback f,
                                        void *data)
{
    curl_easy_setopt(handle(), CURLOPT_TIMEOUT, 5l);
    curl_easy_setopt(handle(), CURLOPT_FAILONERROR, 1l);
    curl_easy_setopt(handle(), CURLOPT_URL, url);
    curl_easy_setopt(handle(), CURLOPT_WRITEFUNCTION, f);
    curl_easy_setopt(handle(), CURLOPT_WRITEDATA, data);
}

const char *osmview::curl_easy::error_message(CURLcode code) const
{
    if (std::strlen(error_buffer))
        return error_buffer;
    else
        return curl_easy_strerror(code);
}

osmview::curl_multi::curl_multi() : handle_(curl_multi_init())
{
    if (!handle_)
        throw std::runtime_error("Curl: error in curl_multi_init");
}

int osmview::curl_multi::perform(bool repeat)
{
    int still_running = 0;
    CURLMcode code;
    do
    {
        code = curl_multi_perform(handle(), &still_running);
    } while (repeat && code == CURLM_CALL_MULTI_PERFORM);
    if (code != CURLM_OK && code != CURLM_CALL_MULTI_PERFORM)
        throw std::runtime_error("Curl: error in curl_multi_perform");
    return still_running;
}

void osmview::curl_multi::add(osmview::curl_easy &easy)
{
    if (curl_multi_add_handle(handle(), easy.handle()) != CURLM_OK)
    {
        throw std::runtime_error("Curl: error in curl_multi_add_handle");
    }
}

void osmview::curl_multi::remove(osmview::curl_easy &easy)
{
    if (curl_multi_remove_handle(handle(), easy.handle()) != CURLM_OK)
    {
        throw std::runtime_error("Curl: error in curl_multi_remove_handle");
    }
}

CURLMsg *osmview::curl_multi::info_read()
{
    int nmessages;
    return curl_multi_info_read(handle(), &nmessages);
}

osmview::curl_global::curl_global()
{
    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK)
        throw std::runtime_error("Curl: error in curl_global_init");
}

osmview::curl_global::~curl_global()
{
    curl_global_cleanup();
}
