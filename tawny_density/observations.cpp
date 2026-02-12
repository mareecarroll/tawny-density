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

#include "observations.hpp"
#include <curl/curl.h>            // for curl_easy_setopt, curl_easy_cleanup
#include <chrono>                 // for milliseconds
#include <cmath>                  // for isnan, NAN
#include <exception>              // for exception
#include <iostream>               // for basic_ostream, operator<<, basic_os...
#include <map>                    // for operator!=, operator==
#include <nlohmann/json.hpp>      // for basic_json
#include <nlohmann/json_fwd.hpp>  // for json
#include <optional>               // for optional
#include <sstream>                // for basic_ostringstream
#include <stdexcept>              // for runtime_error
#include <string>                 // for char_traits, basic_string, allocator
#include <thread>                 // for sleep_for
#include <unordered_map>          // for unordered_map
#include <vector>                 // for vector

using std::string;
using std::vector;
using std::cerr;
using std::fixed;
using std::isnan;
using std::runtime_error;
using std::exception;
using std::this_thread::sleep_for;
using std::optional;
using std::ostringstream;
using std::unordered_map;
using std::chrono::milliseconds;
using json = nlohmann::json;

using observations::ObsPoint;
using observations::URL_BASE;
using observations::USER_AGENT;
using observations::PER_PAGE;

namespace observations {
// Callback function for handling response from inaturalist API call
//
// Args:
//    contents: pointer to response contents
//    size: size of each data element
//    nmemb: number of data elements
//    userData: a pointer to user-defined data, passed via CURLOPT_WRITEDATA option
// Returns:
//   total size of the data (size of each data element * number of data elements)
static size_t curlWriteToString(void* contents, size_t size, size_t nmemb, void* userData) {
    const size_t total = size * nmemb;
    static_cast<string*>(userData)->append(static_cast<char*>(contents), total);
    return total;
}

// Returns encoded url for inaturalist API calls
//
// Args:
//     s: the url to encode
// Returns:
//    the encoded url
string urlEncode(const string& url) {
    char* out = curl_easy_escape(nullptr, url.c_str(), static_cast<int>(url.size()));
    if (!out) return url;
    string encoded(out);
    curl_free(out);
    return encoded;
}

// HTTP GET call to inaturalist API
//
// Args:
//    url: the API url
// Returns:
//   response from the API call
string httpGet(const string& url) {
    string buffer;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    try {
        CURL* curl = curl_easy_init();
        if (!curl) throw runtime_error("curl_easy_init failed");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteToString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            ostringstream oss;
            oss << "CURL error: " << curl_easy_strerror(res);
            curl_easy_cleanup(curl);
            throw runtime_error(oss.str());
        }
        int code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        curl_easy_cleanup(curl);
        if (code < 200 || code >= 300) {
            ostringstream oss;
            oss << "HTTP " << code << " for URL: " << url;
            throw runtime_error(oss.str());
        }
    } catch (const exception& e) {
        cerr << "Fatal: " << e.what() << "\n";
        curl_global_cleanup();
        return "";
    }
    curl_global_cleanup();
    return buffer;
}

// Fetches observation points from iNaturalist
//
// Args:
//    taxonName: the taxonomical name of the animal or plant species
//    d1: must be observed on or after this date
//    d2: must be observed on or before this ddate
//    swlat: south-western latitude component of bounding box
//    swlng: south-western longitude component of bounding box
//    nelat: north-eastern lattitude component of bounding box
//    nelng: north-eastern longitude component of bounding box
// Returns:
//    vector of observation points
vector<ObsPoint> fetchINatPoints(
    const string& taxonName,
    const string& d1, const string& d2,
    double swlat, double swlng, double nelat, double nelng) {

    // vector of points to be returned
    vector<ObsPoint> out;

    // We'll aim for georeferenced observations; v1 supports geo=true
    // (fallback: we'll still filter out those without coordinates if any slip through)
    int page = 1;
    int total_results = -1;

    while (true) {
        ostringstream url;
        url << URL_BASE
            << "?taxon_name=" << urlEncode(taxonName)
            << "&d1=" << d1
            << "&d2=" << d2
            << "&swlat=" << fixed << swlat
            << "&swlng=" << fixed << swlng
            << "&nelat=" << fixed << nelat
            << "&nelng=" << fixed << nelng
            << "&geo=true"
            << "&order_by=observed_on"
            << "&per_page=" << PER_PAGE
            << "&page=" << page;

        const string body = httpGet(url.str());
        if (body.size() == 0) throw runtime_error("Couldn't fetch observations");
        const auto j = json::parse(body);

        if (total_results < 0 && j.contains("total_results")) {
            total_results = j["total_results"].get<int>();
        }

        if (!j.contains("results") || !j["results"].is_array()) break;
        const auto& results = j["results"];
        if (results.empty()) break;

        for (const auto& r : results) {
            // Prefer geometry -> fall back to lat/lon fields
            double lat = NAN, lon = NAN;
            if (r.contains("geojson") && r["geojson"].contains("coordinates")) {
                const auto& coords = r["geojson"]["coordinates"];
                if (coords.is_array() && coords.size() == 2) {
                    lon = coords[0].get<double>();
                    lat = coords[1].get<double>();
                }
            }
            if (isnan(lat) || isnan(lon)) {
                // alternative fields
                if (r.contains("latitude") && r.contains("longitude") &&
                    !r["latitude"].is_null() && !r["longitude"].is_null()) {
                    lat = r["latitude"].get<double>();
                    lon = r["longitude"].get<double>();
                }
            }
            if (!isnan(lat) && !isnan(lon)) {
                out.push_back(ObsPoint{lon, lat});
            }
        }

        // crude stop conditions
        if (static_cast<int>(results.size()) < PER_PAGE) break;
        if (total_results >= 0 && (page * PER_PAGE) >= total_results) break;
        if (page >= 100) {
            cerr << "[warn] Reached page 100; iNaturalist may require auth for higher pages. "
                    "Stopping to avoid being blocked.\n";
            break;
        }
        ++page;
        sleep_for(milliseconds(1100));  // politeness delay
    }
    return out;
}
}  // namespace observations
