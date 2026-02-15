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
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <string>
#include "../tawny_density/observations.hpp"
#include "fake_http_client.hpp"

using std::string;

using utils::HttpResponse;
using utils::IHttpClient;

using observations::urlEncode;
using observations::httpGet;
using observations::fetchINatPoints;

// We wrap curl_easy_escape so we can mock it in tests.
extern "C" {
    #include <curl/curl.h>
}

// Pointer to the escape function (allows overriding in tests)
static char* (*curlEscapeWrapper)(const char*, int) = nullptr;

// Default wrapper that calls libcurl
static char* curlEscapeDefault(const char* s, int len) {
    return curl_easy_escape(nullptr, s, len);
}

// -----------------------------------------------------------------------------
// Tests for urlEncode
// -----------------------------------------------------------------------------

TEST_CASE("urlEncode encodes spaces") {
    curlEscapeWrapper = curlEscapeDefault;
    CHECK_EQ(urlEncode("hello world"), "hello%20world");
}

TEST_CASE("urlEncode encodes reserved characters") {
    curlEscapeWrapper = curlEscapeDefault;
    CHECK_EQ(urlEncode("a&b=c"), "a%26b%3Dc");
}

TEST_CASE("urlEncode leaves safe characters unchanged") {
    curlEscapeWrapper = curlEscapeDefault;
    CHECK_EQ(urlEncode("abc123"), "abc123");
}

TEST_CASE("urlEncode handles empty string") {
    curlEscapeWrapper = curlEscapeDefault;
    CHECK_EQ(urlEncode(""), "");
}

TEST_CASE("urlEncode encodes UTF-8 bytes") {
    curlEscapeWrapper = curlEscapeDefault;
    // "✓" is UTF‑8: E2 9C 93
    CHECK_EQ(urlEncode("✓"), "%E2%9C%93");
}

TEST_CASE("urlEncode returns original string when curl_easy_escape fails") {
    // Fake function that simulates failure
    auto fakeFail = [](const char*, int) -> char* {
        return nullptr;
    };

    curlEscapeWrapper = fakeFail;

    CHECK_EQ(urlEncode("abc"), "abc");

    // Restore default for safety
    curlEscapeWrapper = curlEscapeDefault;
}

// -----------------------------------------------------------------------------
// Tests for httpGet
// -----------------------------------------------------------------------------

TEST_CASE("httpGet returns body on success") {
    FakeHttpClient fake;
    fake.next = {200, "OK"};

    CHECK_EQ(httpGet(fake, "http://example.com"), "OK");
}

TEST_CASE("httpGet returns empty string on HTTP error") {
    FakeHttpClient fake;
    fake.next = {500, "Server error"};

    CHECK_EQ(httpGet(fake, "http://example.com"), "");
}

TEST_CASE("httpGet returns empty string on exception") {
    struct ThrowingClient : IHttpClient {
        HttpResponse get(const std::string&) override {
            throw std::runtime_error("boom");
        }
    } bad;

    CHECK_EQ(httpGet(bad, "http://example.com"), "");
}

// -----------------------------------------------------------------------------
// Tests for fetchINatPoints
// -----------------------------------------------------------------------------

TEST_CASE("fetchINatPoints parses a simple iNaturalist response") {
    FakeHttpClient fake;

    // Minimal valid iNaturalist-style JSON
    fake.next = {
        200,
        R"({
            "total_results": 2,
            "results": [
                {
                    "geojson": { "coordinates": [144.9631, -37.8136] },
                    "observed_on": "2024-01-01"
                },
                {
                    "geojson": { "coordinates": [145.0000, -37.8200] },
                    "observed_on": "2024-01-02"
                }
            ]
        })"
    };

    auto points = fetchINatPoints(
        fake,
        "Aves",
        "2024-01-01", "2024-01-31",
        -38.0, 144.0,
        -37.0, 146.0);

    CHECK_EQ(points.size(), 2);

    CHECK_EQ(points[0].lon, doctest::Approx(144.9631));
    CHECK_EQ(points[0].lat, doctest::Approx(-37.8136));

    CHECK_EQ(points[1].lon, doctest::Approx(145.0000));
    CHECK_EQ(points[1].lat, doctest::Approx(-37.8200));
}

TEST_CASE("fetchINatPoints returns empty vector on HTTP error") {
    FakeHttpClient fake;
    fake.next = {500, "Server error"};

    auto points = fetchINatPoints(
        fake,
        "Aves",
        "2024-01-01", "2024-01-31",
        -38.0, 144.0,
        -37.0, 146.0);

    CHECK(points.empty());
}

TEST_CASE("fetchINatPoints returns empty vector on malformed JSON") {
    FakeHttpClient fake;
    fake.next = {200, "not json at all"};

    auto points = fetchINatPoints(
        fake,
        "Aves",
        "2024-01-01", "2024-01-31",
        -38.0, 144.0,
        -37.0, 146.0);

    CHECK(points.empty());
}
