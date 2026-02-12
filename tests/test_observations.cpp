#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <string>
using std::string;

// -----------------------------------------------------------------------------
// Production code declarations
// -----------------------------------------------------------------------------

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

// Function under test
string urlEncode(const string& url) {
    if (!curlEscapeWrapper)
        curlEscapeWrapper = curlEscapeDefault;

    char* out = curlEscapeWrapper(url.c_str(), static_cast<int>(url.size()));
    if (!out)
        return url;

    string encoded(out);
    curl_free(out);
    return encoded;
}

// -----------------------------------------------------------------------------
// Tests
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
