#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <string>
#include "../tawny_density/observations.hpp"


using std::string;

using observations::urlEncode;
using observations::curlWriteToString;
// using observations::httpGet;
// using observations::fetchINatPoints;

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
    CHECK(urlEncode("hello world") == "hello%20world");
}

TEST_CASE("urlEncode encodes reserved characters") {
    curlEscapeWrapper = curlEscapeDefault;
    CHECK(urlEncode("a&b=c") == "a%26b%3Dc");
}

TEST_CASE("urlEncode leaves safe characters unchanged") {
    curlEscapeWrapper = curlEscapeDefault;
    CHECK(urlEncode("abc123") == "abc123");
}

TEST_CASE("urlEncode handles empty string") {
    curlEscapeWrapper = curlEscapeDefault;
    CHECK(urlEncode("") == "");
}

TEST_CASE("urlEncode encodes UTF-8 bytes") {
    curlEscapeWrapper = curlEscapeDefault;
    // "✓" is UTF‑8: E2 9C 93
    CHECK(urlEncode("✓") == "%E2%9C%93");
}

TEST_CASE("urlEncode returns original string when curl_easy_escape fails") {
    // Fake function that simulates failure
    auto fakeFail = [](const char*, int) -> char* {
        return nullptr;
    };

    curlEscapeWrapper = fakeFail;

    CHECK(urlEncode("abc") == "abc");

    // Restore default for safety
    curlEscapeWrapper = curlEscapeDefault;
}

// -----------------------------------------------------------------------------
// Tests for curlWriteToString
// -----------------------------------------------------------------------------

TEST_CASE("curlWriteToString appends data correctly") {
    string buffer;

    const char* data = "hello world";
    size_t size = 1;
    size_t nmemb = 11; // strlen("hello world")

    size_t written = curlWriteToString((void*)data, size, nmemb, &buffer);

    CHECK(written == 11);
    CHECK(buffer == "hello world");
}

TEST_CASE("curlWriteToString handles multi-byte chunks") {
    string buffer;

    const char* data = "abc123";
    size_t size = 2;     // pretend each element is 2 bytes
    size_t nmemb = 3;    // 3 elements → 6 bytes total

    size_t written = curlWriteToString((void*)data, size, nmemb, &buffer);

    CHECK(written == 6);
    CHECK(buffer == "abc123");
}

TEST_CASE("curlWriteToString appends to existing content") {
    string buffer = "prefix:";

    const char* data = "XYZ";
    size_t size = 1;
    size_t nmemb = 3;

    size_t written = curlWriteToString((void*)data, size, nmemb, &buffer);

    CHECK(written == 3);
    CHECK(buffer == "prefix:XYZ");
}
