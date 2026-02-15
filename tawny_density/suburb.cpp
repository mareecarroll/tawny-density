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
#include "suburb.hpp"
#include <algorithm>                                // for max, min, find_if
#include <cstddef>                                  // for size_t
#include <fstream>                                  // for basic_ifstream
#include <map>                                      // for operator!=, opera...
#include <nlohmann/detail/iterators/iter_impl.hpp>  // for iter_impl
#include <nlohmann/json.hpp>                        // for basic_json, opera...
#include <optional>                                 // for optional
#include <stdexcept>                                // for runtime_error
#include <string>                                   // for basic_string, string
#include <unordered_map>                            // for unordered_map
#include <utility>                                  // for move
#include <vector>                                   // for vector

using std::string;
using std::vector;
using std::min;
using std::max;
using std::runtime_error;
using std::optional;
using std::ifstream;
using std::unordered_map;
using json = nlohmann::json;

using suburb::Point;
using suburb::Polygon;
using suburb::Ring;
using suburb::Suburb;

namespace suburb {
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

// bool onSegment(Point p, Point p1, Point p2) {
//     // Check if point p is collinear with p1 and p2
//     double cross_product = (p.lat - p1.lat) * (p2.lon - p1.lon) - (p.lon - p1.lon) * (p2.lat - p1.lat);
//     if (std::abs(cross_product) > 1e-9) return false; // Not collinear

//     // Check if point p lies between p1 and p2 on the collinear line
//     return (p.lon >= std::min(p1.lon, p2.lon) && p.lon <= std::max(p1.lon, p2.lon) &&
//             p.lat >= std::min(p1.lat, p2.lat) && p.lat <= std::max(p1.lat, p2.lat));
// }

// Ray casting for point in ring (excluding boundary ambiguity -> treat boundary as inside)
// bool pointInRing(const Ring& ring, const Point& q) {
//     bool inside = false;
//     const auto& points = ring.points;
//     const size_t n = points.size();
//     if (n < 3) return false;
//     for (size_t i = 0, j = n - 1; i < n; j = i++) {
//         const Point& a = points[j];
//         const Point& b = points[i];
//         const bool intersect = ((a.lat > q.lat) != (b.lat > q.lat)) &&
//             (q.lon < (b.lon - a.lon) * (q.lat - a.lat) / (b.lat - a.lat + 1e-20) + a.lon);
//         if (intersect) inside = !inside;
//     }
//     return inside;
// }

// Function to check if a point is inside a polygon using
// the ray-casting algorithm
// see https://www.geeksforgeeks.org/cpp/point-in-polygon-in-cpp/
//    #c-program-to-check-point-in-polygon-using-raycasting-algorithm
bool pointInRing(const Ring& ring, const Point& point) {
    // Number of vertices in the polygon
    int n = ring.points.size();
    // Count of intersections
    int count = 0;

    // Iterate through each edge of the polygon
    for (int i = 0; i < n; i++) {
        Point p1 = ring.points[i];
        // Ensure the last point connects to the first point
        Point p2 = ring.points[(i + 1) % n];

        // Check if the point's y-coordinate/lat is within the
        // edge's y-range and if the point is to the left of
        // the edge
        if ((point.lat > min(p1.lat, p2.lat)) &&
            (point.lat <= max(p1.lat, p2.lat)) &&
            (point.lon <= max(p1.lon, p2.lon))) {
            // Calculate the x-coordinate/lat of the
            // intersection of the edge with a horizontal
            // line through the point
            double xIntersect = (point.lat - p1.lat) * (p2.lon - p1.lon)
                / (p2.lat - p1.lat) + p1.lon;
            // If the edge is vertical or the point's
            // x-coordinate is less than or equal to the
            // intersection x-coordinate, increment count
            if (p1.lon == p2.lon || point.lon <= xIntersect) {
                count++;
            }
        }
    }
    // If the number of intersections is odd, the point is
    // inside the polygon
    return count % 2 == 1;
}


// Returns true if point is inside polygon
//
// Args:
//     poly: the polygon including bounding box
//     point: the point lat/lon to check inside polygon
// Returns:
//     true if point sits inside polygon
bool pointInPolygon(const Polygon& poly, const Point& point) {
    // Fast bounding box reject
    if (point.lon < poly.minLon + 1e-12 ||
        point.lon > poly.maxLon + 1e-12||
        point.lat < poly.minLat + 1e-12||
        point.lat > poly.maxLat + 1e-12)
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
    if (point.lon < suburb.minLon + 1e-12 ||
        point.lon > suburb.maxLon + 1e-12 ||
        point.lat < suburb.minLat + 1e-12 ||
        point.lat > suburb.maxLat + 1e-12)
        return false;

    return std::any_of(
        suburb.polys.begin(),
        suburb.polys.end(),
        [&](const auto& poly) {
            return pointInPolygon(poly, point);
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
            return kv.is_string();
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
                poly.rings.push_back(std::move(ring));
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

            suburb.polys.push_back(std::move(poly));
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

        suburbs.push_back(std::move(suburb));
    }

    if (suburbs.empty()) throw runtime_error("No suburb polygons loaded from GeoJSON");
    return suburbs;
}
}  // namespace suburb
