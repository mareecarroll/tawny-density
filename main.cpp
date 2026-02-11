// main.cpp
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
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

#include <curl/curl.h>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

// -------------------------------------
// structures for holding suburb polygon
// -------------------------------------
// lat/lon point
struct Point { double lon{}, lat{}; };
// polygon ring
struct Ring {
    // Closed or open ring of lon/lat points
    vector<Point> pts;
};
// polygon shape
struct Polygon {
    // rings[0] = outer; rings[1..] = holes
    vector<Ring> rings;
    // AABB for fast reject
    double minLon{}, minLat{}, maxLon{}, maxLat{};
};
// suburb from geojson, suburb name, polygons, bounding box
struct Suburb {
    string name;
    vector<Polygon> polys;
    // aabb across all polygons
    double minLon{}, minLat{}, maxLon{}, maxLat{};
};

// input args for main entry point 
struct Args {
    string geojsonPath;
    optional<string> outCsv;
};

/**
 * Returns total 
 */
static size_t curlWriteToString(void* contents, size_t size, size_t nmemb, void* userp) {
    const size_t total = size * nmemb;
    static_cast<string*>(userp)->append(static_cast<char*>(contents), total);
    return total;
}

/**
 * Returns encoded url for inaturalist API calls
 */
string urlEncode(const string& s) {
    char* out = curl_easy_escape(nullptr, s.c_str(), (int)s.size());
    if (!out) return s;
    string encoded(out);
    curl_free(out);
    return encoded;
}
/**
 * HTTP GET for inaturalist API calls
 */
string httpGet(const string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) throw runtime_error("curl_easy_init failed");
    string buffer;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "MareeCarroll-TawnyFrogmouth/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        ostringstream oss;
        oss << "CURL error: " << curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        throw runtime_error(oss.str());
    }
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    if (code < 200 || code >= 300) {
        ostringstream oss;
        oss << "HTTP " << code << " for URL: " << url;
        throw runtime_error(oss.str());
    }
    return buffer;
}
/**
 * parses arguments from main entry point
 */
bool parseArgs(int argc, char** argv, Args& out) {
    for (int i = 1; i < argc; ++i) {
        string a(argv[i]);
        if (a == "--geojson" && i + 1 < argc) {
            out.geojsonPath = argv[++i];
        } else if (a == "--out" && i + 1 < argc) {
            out.outCsv = argv[++i];
        } else if (a == "--help" || a == "-h") {
            return false;
        }
    }
    return !out.geojsonPath.empty();
}

/**
 * CLI usage info
 */
void usage(const char* exe) {
    cerr << "Usage:\n"
              << "  " << exe << " --geojson /path/to/melbourne_suburbs.geojson [--out counts.csv]\n";
}

/* 
 * Compute AABB for a ring
 */
void ringBounds(const Ring& r, double& minLon, double& minLat, double& maxLon, double& maxLat) {
    minLon =  1e300; minLat =  1e300;
    maxLon = -1e300; maxLat = -1e300;
    for (const auto& p : r.pts) {
        minLon = min(minLon, p.lon);
        minLat = min(minLat, p.lat);
        maxLon = max(maxLon, p.lon);
        maxLat = max(maxLat, p.lat);
    }
}

/*
 * Ray casting for point in ring (excluding boundary ambiguity -> treat boundary as inside)
 */
bool pointInRing(const Ring& ring, const Point& q) {
    bool inside = false;
    const auto& pts = ring.pts;
    const size_t n = pts.size();
    if (n < 3) return false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const Point& a = pts[j];
        const Point& b = pts[i];
        const bool intersect = ((a.lat > q.lat) != (b.lat > q.lat)) &&
                               (q.lon < (b.lon - a.lon) * (q.lat - a.lat) / (b.lat - a.lat + 1e-20) + a.lon);
        if (intersect) inside = !inside;
    }
    return inside;
}

/*
 * Returns true if point is inside polygon
 */
bool pointInPolygon(const Polygon& poly, const Point& q) {
    // Fast AABB reject
    if (q.lon < poly.minLon || q.lon > poly.maxLon || q.lat < poly.minLat || q.lat > poly.maxLat) return false;
    if (poly.rings.empty()) return false;
    // Inside outer?
    if (!pointInRing(poly.rings.front(), q)) return false;
    // Not inside any hole
    for (size_t i = 1; i < poly.rings.size(); ++i) {
        if (pointInRing(poly.rings[i], q)) return false;
    }
    return true;
}

/*
 * Returns true if point is inside suburb
 */
bool pointInSuburb(const Suburb& s, const Point& q) {
    // Suburb-level AABB
    if (q.lon < s.minLon || q.lon > s.maxLon || q.lat < s.minLat || q.lat > s.maxLat) return false;
    for (const auto& poly : s.polys) {
        if (pointInPolygon(poly, q)) return true;
    }
    return false;
}

/**
 * Generic detector for suburb name in json
 */
string detectNameField(const json& props) {
    // Try common field names in AUS locality datasets
    static const vector<std::string> candidates = {
        "NAME", "Name", "name",
        "LOCALITY_NAME", "LOCALITY", "LOC_NAME",
        "vic_loca_2", "vic_loca_1", "vic_loca_",
        "SUBURB_NAME", "SuburbName", "suburb"
    };
    for (const auto& k : candidates) {
        if (props.contains(k) && props[k].is_string()) return k;
    }
    // Fallback: first string property
    for (auto it = props.begin(); it != props.end(); ++it) {
        if (it.value().is_string()) return it.key();
    }
    return {};
}

/**
 * Loads suburbs geojson into vector of Suburb structues.
 */
vector<Suburb> loadSuburbsGeoJSON(const std::string& path, double& outMinLon, double& outMinLat,
                                       double& outMaxLon, double& outMaxLat)
{
    ifstream in(path);
    if (!in) throw runtime_error("Failed to open GeoJSON: " + path);
    json gj; in >> gj;

    if (!gj.contains("features") || !gj["features"].is_array())
        throw runtime_error("Invalid GeoJSON (no features array)");

    vector<Suburb> suburbs;
    outMinLon =  1e300; outMinLat =  1e300;
    outMaxLon = -1e300; outMaxLat = -1e300;

    for (const auto& feat : gj["features"]) {
        if (!feat.contains("geometry") || feat["geometry"].is_null()) continue;
        const auto& geom = feat["geometry"];
        const auto type = geom.value("type", "");
        const auto& props = feat.value("properties", json::object());

        const string nameField = detectNameField(props);
        string name = nameField.empty() ? "UNKNOWN" : props.value(nameField, std::string("UNKNOWN"));

        Suburb sub; sub.name = name;
        sub.minLon =  1e300; sub.minLat =  1e300;
        sub.maxLon = -1e300; sub.maxLat = -1e300;

        auto addPolygon = & {
            // coords: [ [ [lon,lat], ... ], [hole...], ... ]
            Polygon poly;
            for (const auto& ringCoords : coords) {
                Ring ring;
                for (const auto& p : ringCoords) {
                    Point pt{ p.at(0).get<double>(), p.at(1).get<double>() };
                    ring.pts.push_back(pt);
                }
                // ensure closed ring for numeric stability (optional)
                if (!ring.pts.empty() && (ring.pts.front().lon != ring.pts.back().lon ||
                                          ring.pts.front().lat != ring.pts.back().lat)) {
                    ring.pts.push_back(ring.pts.front());
                }
                poly.rings.push_back(move(ring));
            }
            // compute poly AABB
            double minLon, minLat, maxLon, maxLat;
            ringBounds(poly.rings.front(), minLon, minLat, maxLon, maxLat);
            for (size_t i = 1; i < poly.rings.size(); ++i) {
                double rminLon, rminLat, rmaxLon, rmaxLat;
                ringBounds(poly.rings[i], rminLon, rminLat, rmaxLon, rmaxLat);
                minLon = min(minLon, rminLon);
                minLat = min(minLat, rminLat);
                maxLon = max(maxLon, rmaxLon);
                maxLat = max(maxLat, rmaxLat);
            }
            poly.minLon = minLon; poly.minLat = minLat; poly.maxLon = maxLon; poly.maxLat = maxLat;

            // update suburb AABB
            sub.minLon = min(sub.minLon, minLon);
            sub.minLat = min(sub.minLat, minLat);
            sub.maxLon = max(sub.maxLon, maxLon);
            sub.maxLat = max(sub.maxLat, maxLat);

            sub.polys.push_back(move(poly));
        };

        if (type == "Polygon") {
            addPolygon(geom["coordinates"]);
        } else if (type == "MultiPolygon") {
            for (const auto& polyCoords : geom["coordinates"]) addPolygon(polyCoords);
        } else {
            // Ignore non-area features
            continue;
        }

        // Update global AABB
        outMinLon = min(outMinLon, sub.minLon);
        outMinLat = min(outMinLat, sub.minLat);
        outMaxLon = max(outMaxLon, sub.maxLon);
        outMaxLat = max(outMaxLat, sub.maxLat);

        suburbs.push_back(move(sub));
    }

    if (suburbs.empty()) throw runtime_error("No suburb polygons loaded from GeoJSON");
    return suburbs;
}
/**
 * Structure for tawny frogmouth sighting lat/lon
 */
struct ObsPoint { double lon{}, lat{}; };

vector<ObsPoint> fetchINatPoints(const std::string& taxonName,
                                      const string& d1, const std::string& d2,
                                      double swlat, double swlng, double nelat, double nelng)
{
    vector<ObsPoint> out;

    const string base = "https://api.inaturalist.org/v1/observations";
    const int perPage = 200; // v1: max 200 per page
    // We'll aim for georeferenced observations; v1 supports geo=true
    // (fallback: we'll still filter out those without coordinates if any slip through)
    int page = 1;
    int total_results = -1;

    while (true) {
        ostringstream url;
        url << base
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
                    !r["latitude"].is_null() && !r["longitude"].is_null())
                {
                    lat = r["latitude"].get<double>();
                    lon = r["longitude"].get<double>();
                }
            }
            if (!isnan(lat) && !std::isnan(lon)) {
                out.push_back(ObsPoint{lon, lat});
            }
        }

        // crude stop conditions
        if ((int)results.size() < perPage) break;
        if (total_results >= 0 && (page * perPage) >= total_results) break;
        if (page >= 100) {
            cerr << "[warn] Reached page 100; iNaturalist may require auth for higher pages. "
                         "Stopping to avoid being blocked.\n";
            break;
        }
        ++page;
        this_thread::sleep_for(std::chrono::milliseconds(1100)); // politeness delay
    }
    return out;
}

/**
 * Entry point
 */
int main(int argc, char** argv) {
    Args args;
    if (!parseArgs(argc, argv, args)) {
        usage(argv[0]);
        return 1;
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);

    try {
        // 1) Load suburbs
        double minLon, minLat, maxLon, maxLat;
        auto suburbs = loadSuburbsGeoJSON(args.geojsonPath, minLon, minLat, maxLon, maxLat);

        // 2) Fetch iNaturalist sightings for Spring 2025
        //    Australia (meteorological): Spring = Sep-Nov -> 2025-09-01 .. 2025-11-30
        const string d1 = "2025-09-01";
        const string d2 = "2025-11-30";
        const string taxon = "Podargus strigoides"; // Tawny Frogmouth

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

        // Basic spatial filter: try suburb AABBs then precise PIP
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
