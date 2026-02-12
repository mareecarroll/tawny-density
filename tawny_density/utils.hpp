// Copyright 2026 Maree Carroll
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once
#include <string>
#include <curl/curl.h>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <cstdint>

using std::string;
using std::runtime_error;

namespace utils {

// Simple struct to hold HTTP response data
struct HttpResponse {
    uint16_t status = 0;
    string body;
};

// Interface for HTTP client (allows mocking in tests)
struct IHttpClient {
    virtual ~IHttpClient() = default;
    virtual HttpResponse get(const string& url) = 0;
};

// Concrete implementation of IHttpClient using libcurl
class CurlHttpClient : public IHttpClient {
 public:
    // Constructor initializes libcurl
    CurlHttpClient() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    // Destructor cleans up libcurl resources
    ~CurlHttpClient() override {
        curl_global_cleanup();
    }

    // Performs an HTTP GET request to the specified URL and returns the response
    //
    // Args:
    //    url: the URL to send the GET request to
    // Returns:
    //    HttpResponse containing the status code and response body
    HttpResponse get(const string& url) override {
        HttpResponse resp;
        string buffer;

        CURL* curl = curl_easy_init();
        if (!curl) throw runtime_error("curl_easy_init failed");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "tawny-density");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            curl_easy_cleanup(curl);
            throw runtime_error(curl_easy_strerror(res));
        }

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status);
        curl_easy_cleanup(curl);

        resp.body = std::move(buffer);
        return resp;
    }

 private:
    // Callback function for libcurl to write response data into a string
    //
    // Args:
    //    contents: pointer to the data received from the server
    //    size: size of each data element
    //    nmemb: number of data elements
    //    userData: pointer to user-defined data (in this case, a string to append to)
    // Returns:
    //    total size of the data (size of each data element * number of data elements)
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userData) {
        size_t total = size * nmemb;
        auto* out = static_cast<string*>(userData);
        out->append(static_cast<char*>(contents), total);
        return total;
    }
};

}  // namespace utils
