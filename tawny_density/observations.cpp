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
#include "utils.hpp"              // for HttpResponse, CurlHttpClient, IHttpClient

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

using utils::HttpResponse;
using utils::IHttpClient;
using utils::CurlHttpClient;

namespace observations {

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
//    client: the HTTP client to use for making the request
//    url: the API url
// Returns:
//   response from the API call
string httpGet(IHttpClient& client, const string& url) {
    try {
        HttpResponse resp = client.get(url);

        if (resp.status < 200 || resp.status >= 300) {
            return "";
        }

        return resp.body;
    }
    catch (...) {
        return "";
    }
}

// Fetches observation points from iNaturalist
//
// Args:
//    client: the HTTP client to use for making the request
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
    IHttpClient& client,
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

        const string body = httpGet(client, url.str());

        if (body.empty()) break;
        
        json j;
        try {
            j = json::parse(body);
        } catch(...) {
            break;
        }

        if (total_results < 0 && j.contains("total_results")) {
            total_results = j["total_results"];
        }

        if (!j.contains("results") || !j["results"].is_array()) break;

        for (auto& item : j["results"]) {
            // Skip if no geojson or coordinates
            if (!item.contains("geojson")) continue;
            if (!item["geojson"].contains("coordinates")) continue;
            // Skip if coordinates are not an array of size 2
            auto coords = item["geojson"]["coordinates"];
            if (!coords.is_array() || coords.size() != 2) continue;
            // get the coordinates and add to output
            ObsPoint p;
            p.lon = coords[0].get<double>();
            p.lat = coords[1].get<double>();
            out.push_back(p);
        }

        // If we've fetched all results, break
        if (static_cast<int>(out.size()) >= total_results) break;

        ++page;
        sleep_for(milliseconds(1100));  // politeness delay
    }
    return out;
}
}  // namespace observations
