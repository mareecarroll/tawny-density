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

// main.cpp

// 1. Project headers
#include "main.h"

// 2. C system headers
#include <cstring>   // cpplint thinks this is C system
#include <cassert>
#include <cstddef>

// 3. C++ system headers
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

// 4. Other library headers
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::min;
using std::max;
using std::move;
using std::fixed;
using std::isnan;
using std::this_thread::sleep_for;
using std::runtime_error;
using std::optional;
using std::ostringstream;
using std::ifstream;
using std::ofstream;
using std::unordered_map;
using std::exception;
using std::chrono::steady_clock;
using std::chrono::milliseconds;
using json = nlohmann::json;

// const char USER_AGENT[] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
//     "AppleWebKit/537.36 (KHTML, like Gecko) "
//     "Chrome/58.0.3029.110 Safari/537.36";

// // -------------------------------------
// // structures for holding suburb polygon
// // -------------------------------------

// // lat/lon point
// struct Point { double lon{}, lat{}; };

// // polygon ring
// struct Ring {
//     // Closed or open ring of lon/lat points
//     vector<Point> points;
// };

// // polygon shape
// struct Polygon {
//     // rings[0] = outer; rings[1..] = holes
//     vector<Ring> rings;
//     // bounding box for fast reject
//     double minLon{}, minLat{}, maxLon{}, maxLat{};
// };

// // suburb from geojson, suburb name, polygons, bounding box
// struct Suburb {
//     string name;
//     vector<Polygon> polys;
//     // bounding box
//     double minLon{}, minLat{}, maxLon{}, maxLat{};
// };

// // Structure for tawny frogmouth sighting lat/lon
// // as per iNaturalist observation
// struct ObsPoint { double lon{}, lat{}; };

// input args for main entry point
struct Args {
    string geojsonPath;
    optional<string> outCsv;
};

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
    CURL* curl = curl_easy_init();
    if (!curl) throw runtime_error("curl_easy_init failed");
    string buffer;
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
    return buffer;
}

// Parses arguments from main entry point
//
// Args:
//    argc: number of arguments given
//    argv: provided arguments
//    out: pointer to the Args structure to set state on
bool parseArgs(int argc, char** argv, Args* out) {
    for (int i = 1; i < argc; ++i) {
        string a(argv[i]);
        if (a == "--geojson" && i + 1 < argc) {
            (*out).geojsonPath = argv[++i];
        } else if (a == "--out" && i + 1 < argc) {
            (*out).outCsv = argv[++i];
        } else if (a == "--help" || a == "-h") {
            return false;
        }
    }
    return !(*out).geojsonPath.empty();
}

// CLI usage message output as console error message
//
// Args:
//     exe: executable's name
void usage(const char* exe) {
    cerr << "Usage:\n"
    << "  " << exe
    << " --geojson /path/to/melbourne_suburbs.geojson [--out counts.csv]\n";
}

// Compute axis-aligned bounding box for given polygon ring
//
// Args:
//    ring: reference to a polygon ring
//    minLon: minimum longitude component of the ring's bounding box computed here
//    minLat: minimum latitude component of the ring's bounding box computed here
//    maxLon: maximum longitude component of the ring's bounding box computed here
//    maxLat: maximum latitude component of the ring's bounding box computed here
void ringBounds(const Ring& ring, double* minLon, double* minLat, double* maxLon, double* maxLat) {
    *minLon =  1e300; *minLat =  1e300;
    *maxLon = -1e300; *maxLat = -1e300;
    for (const auto& point : ring.points) {
        *minLon = min(*minLon, point.lon);
        *minLat = min(*minLat, point.lat);
        *maxLon = max(*maxLon, point.lon);
        *maxLat = max(*maxLat, point.lat);
    }
}

// Ray casting for point in ring (excluding boundary ambiguity -> treat boundary as inside)
bool pointInRing(const Ring& ring, const Point& q) {
    bool inside = false;
    const auto& points = ring.points;
    const size_t n = points.size();
    if (n < 3) return false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const Point& a = points[j];
        const Point& b = points[i];
        const bool intersect = ((a.lat > q.lat) != (b.lat > q.lat)) &&
            (q.lon < (b.lon - a.lon) * (q.lat - a.lat) / (b.lat - a.lat + 1e-20) + a.lon);
        if (intersect) inside = !inside;
    }
    return inside;
}

// Returns true if point is inside polygon
bool pointInPolygon(const Polygon& poly, const Point& point) {
    // Fast bounding box reject
    if (point.lon < poly.minLon ||
        point.lon > poly.maxLon ||
        point.lat < poly.minLat ||
        point.lat > poly.maxLat)
        return false;
    if (poly.rings.empty()) return false;
    // Inside outer?
    if (!pointInRing(poly.rings.front(), point)) return false;
    // Not inside any hole
    for (size_t i = 1; i < poly.rings.size(); ++i) {
        if (pointInRing(poly.rings[i], point)) return false;
    }
    return true;
}

// Returns true if point is inside suburb bounding box
//
// Args:
//     suburb: the suburb including bounding box
//     point: the point lat/lon to check inside suburb bounding box
// Returns:
//     true if point sits inside bounding box
bool pointInSuburb(const Suburb& suburb, const Point& point) {
    // Suburb-level bounding box
    if (point.lon < suburb.minLon ||
        point.lon > suburb.maxLon ||
        point.lat < suburb.minLat ||
        point.lat > suburb.maxLat)
        return false;

    return std::any_of(
        suburb.polys.begin(),
        suburb.polys.end(),
        [&](const auto& poly) {
            return pointInPolygon(&poly, &point);
        });
}

// Generic detector for suburb name in json
//
// Args:
//     props: json object of properties
// Returns:
//     suburb name pulled out of properties, or some likely
//         substitute candidate
string detectNameField(const json& props) {
    // Try common field names in AUS locality datasets
    static const vector<std::string> candidates = {
        "NAME", "Name", "name",
        "LOCALITY_NAME", "LOCALITY", "LOC_NAME",
        "vic_loca_2", "vic_loca_1", "vic_loca_",
        "SUBURB_NAME", "SuburbName", "suburb"
    };

    // try to find suburb name property
    auto it1 = std::find_if(
        candidates.begin(),
        candidates.end(),
        [&](const auto& k) {
            return props.contains(k) && props[k].is_string();
        });
    if (it1 != candidates.end()) return *it1;


    // fallback to first string property
    auto it2 = std::find_if(
        props.begin(),
        props.end(),
        [](const auto& kv) {
            return kv.value().is_string();
        });
    if (it2 != props.end()) return it2.key();

    // didn't find suitable candidate for suburb name
    return {};
}

// Loads suburbs geojson into vector of Suburb structues.
//
// Args:
//    path: A const reference to the suburbs geojson file path
//    outMinLon: the minimum longitude component of the bounding box to be calculated here
//    outMinLat: the minimum latitude component of the boudning box to be calculated here
//    outMaxLon: the maximum longitude component of the bounding box to be calculated here
//    outMaxLat: the maximum latitude component of the boudning box to be calculated here
// Returns:
//    The vector of suburb polygons gathered from the geojson
vector<Suburb> loadSuburbsGeoJSON(
    const string& path, double* outMinLon, double* outMinLat,
    double* outMaxLon, double* outMaxLat) {
    ifstream in(path);
    if (!in) throw runtime_error("Failed to open GeoJSON: " + path);
    json gj; in >> gj;

    if (!gj.contains("features") || !gj["features"].is_array())
        throw runtime_error("Invalid GeoJSON (no features array)");

    vector<Suburb> suburbs;
    *outMinLon =  1e300; *outMinLat =  1e300;
    *outMaxLon = -1e300; *outMaxLat = -1e300;

    // for each feature, create suburb and add to suburbs vector
    for (const auto& feat : gj["features"]) {
        if (!feat.contains("geometry") || feat["geometry"].is_null()) continue;
        const auto& geom = feat["geometry"];
        const auto type = geom.value("type", "");
        const auto& props = feat.value("properties", json::object());

        const string nameField = detectNameField(props);
        string name = nameField.empty() ? "UNKNOWN" : props.value(nameField, std::string("UNKNOWN"));

        // initialise suburb with name, bouding box
        Suburb suburb;
        suburb.name = name;
        suburb.minLon =  1e300;
        suburb.minLat =  1e300;
        suburb.maxLon = -1e300;
        suburb.maxLat = -1e300;

        // lambda addPolygon
        // Args:
        //     coords: the coordinates section of geojson
        auto addPolygon = [&](const json& coords) {
            // coords: [ [ [lon,lat], ... ], [hole...], ... ]
            Polygon poly;
            for (const auto& ringCoords : coords) {
                Ring ring;
                for (const auto& p : ringCoords) {
                    Point pt{ p.at(0).get<double>(), p.at(1).get<double>() };
                    ring.points.push_back(pt);
                }
                // ensure closed ring for numeric stability
                if (!ring.points.empty() && (
                        ring.points.front().lon != ring.points.back().lon ||
                        ring.points.front().lat != ring.points.back().lat)) {
                    ring.points.push_back(ring.points.front());
                }
                poly.rings.push_back(move(ring));
            }
            // compute poly axis-aligned bounding box
            double minLon, minLat, maxLon, maxLat;
            ringBounds(poly.rings.front(), &minLon, &minLat, &maxLon, &maxLat);
            for (size_t i = 1; i < poly.rings.size(); ++i) {
                double rminLon, rminLat, rmaxLon, rmaxLat;
                ringBounds(poly.rings[i], &rminLon, &rminLat, &rmaxLon, &rmaxLat);
                minLon = min(minLon, rminLon);
                minLat = min(minLat, rminLat);
                maxLon = max(maxLon, rmaxLon);
                maxLat = max(maxLat, rmaxLat);
            }
            poly.minLon = minLon; poly.minLat = minLat; poly.maxLon = maxLon; poly.maxLat = maxLat;

            // update suburb axis-aligned bounding box
            suburb.minLon = min(suburb.minLon, minLon);
            suburb.minLat = min(suburb.minLat, minLat);
            suburb.maxLon = max(suburb.maxLon, maxLon);
            suburb.maxLat = max(suburb.maxLat, maxLat);

            suburb.polys.push_back(move(poly));
        };

        if (type == "Polygon") {
            addPolygon(geom["coordinates"]);
        } else if (type == "MultiPolygon") {
            for (const auto& polyCoords : geom["coordinates"]) addPolygon(polyCoords);
        } else {
            // Ignore non-area features
            continue;
        }

        // Update global bounding box
        *outMinLon = min(*outMinLon, suburb.minLon);
        *outMinLat = min(*outMinLat, suburb.minLat);
        *outMaxLon = max(*outMaxLon, suburb.maxLon);
        *outMaxLat = max(*outMaxLat, suburb.maxLat);

        suburbs.push_back(move(suburb));
    }

    if (suburbs.empty()) throw runtime_error("No suburb polygons loaded from GeoJSON");
    return suburbs;
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
    const std::string& taxonName,
    const string& d1, const std::string& d2,
    double swlat, double swlng, double nelat, double nelng) {

    // vector of points to be returned
    vector<ObsPoint> out;

    const string url_base = "https://api.inaturalist.org/v1/observations";
    const int perPage = 200;  // v1: max 200 per page

    // We'll aim for georeferenced observations; v1 supports geo=true
    // (fallback: we'll still filter out those without coordinates if any slip through)
    int page = 1;
    int total_results = -1;

    while (true) {
        ostringstream url;
        url << url_base
            << "?taxon_name=" << urlEncode(taxonName)
            << "&d1=" << d1
            << "&d2=" << d2
            << "&swlat=" << fixed << swlat
            << "&swlng=" << fixed << swlng
            << "&nelat=" << fixed << nelat
            << "&nelng=" << fixed << nelng
            << "&geo=true"
            << "&order_by=observed_on"
            << "&per_page=" << perPage
            << "&page=" << page;

        const string body = httpGet(url.str());
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
            if (isnan(lat) || std::isnan(lon)) {
                // alternative fields
                if (r.contains("latitude") && r.contains("longitude") &&
                    !r["latitude"].is_null() && !r["longitude"].is_null()) {
                    lat = r["latitude"].get<double>();
                    lon = r["longitude"].get<double>();
                }
            }
            if (!isnan(lat) && !std::isnan(lon)) {
                out.push_back(ObsPoint{lon, lat});
            }
        }

        // crude stop conditions
        if (static_cast<int>(results.size()) < perPage) break;
        if (total_results >= 0 && (page * perPage) >= total_results) break;
        if (page >= 100) {
            cerr << "[warn] Reached page 100; iNaturalist may require auth for higher pages. "
                    "Stopping to avoid being blocked.\n";
            break;
        }
        ++page;
        sleep_for(std::chrono::milliseconds(1100));  // politeness delay
    }
    return out;
}

// Entry point
int main(int argc, char** argv) {
    Args args;
    if (!parseArgs(argc, argv, &args)) {
        usage(argv[0]);
        return 1;
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);

    try {
        // 1) Load suburbs
        double minLon, minLat, maxLon, maxLat;  // collect bonding box for Victoria
        auto suburbs = loadSuburbsGeoJSON(args.geojsonPath, &minLon, &minLat, &maxLon, &maxLat);

        // 2) Fetch iNaturalist sightings for Spring 2025
        const string d1 = "2025-09-01";
        const string d2 = "2025-11-30";
        const string taxon = "Podargus strigoides";  // Tawny Frogmouth

        // iNat bounding box expects (swlat, swlng, nelat, nelng)
        const double swlat = minLat, swlng = minLon, nelat = maxLat, nelng = maxLon;

        cerr << "Suburbs loaded: " << suburbs.size() << "\n";
        cerr << "Querying iNaturalist within bbox ["
            << swlat << "," << swlng << "] to [" << nelat << "," << nelng
            << "] for " << taxon << " from " << d1 << " to " << d2 << " ...\n";

        auto obs = fetchINatPoints(taxon, d1, d2, swlat, swlng, nelat, nelng);
        cerr << "Observations fetched (with coordinates): " << obs.size() << "\n";

        // 3) Assign to suburb
        unordered_map<std::string, std::uint64_t> counts;
        counts.reserve(suburbs.size() * 2);

        // Basic spatial filter: try suburb axis-aligned bounding boxs then precise PIP
        size_t assigned = 0;
        for (const auto& op : obs) {
            // quick reject using global bbox is unnecessary here; obs already limited by bbox
            // find first matching suburb (they should not overlap meaningfully)
            for (const auto& s : suburbs) {
                if (!pointInSuburb(s, Point{op.lon, op.lat})) continue;
                counts[s.name] += 1;
                ++assigned;
                break;
            }
        }
        cerr << "Assigned observations: " << assigned << "\n";

        // 4) Find the top suburb
        string topSuburb;
        uint64_t topCount = 0;
        for (const auto& kv : counts) {
            if (kv.second > topCount) {
                topSuburb = kv.first;
                topCount = kv.second;
            }
        }

        if (topCount == 0) {
            cout << "No Tawny Frogmouth observations found in Spring 2025 for the provided suburbs.\n";
        } else {
            cout << "Top suburb (Spring 2025): " << topSuburb
                << " — " << topCount << " sightings\n";
        }

        // Optional CSV output
        if (args.outCsv) {
            ofstream out(*args.outCsv);
            if (!out) throw runtime_error("Failed to open CSV for writing: " + *args.outCsv);
            out << "suburb,count\n";
            for (const auto& kv : counts) {
                // Quote suburb in case of commas
                out << "\"" << kv.first << "\"," << kv.second << "\n";
            }
            cerr << "Wrote counts CSV to " << *args.outCsv << "\n";
        }
    } catch (const exception& e) {
        cerr << "Fatal: " << e.what() << "\n";
        curl_global_cleanup();
        return 2;
    }

    curl_global_cleanup();
    return 0;
}
