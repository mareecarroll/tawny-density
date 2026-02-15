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
#ifndef TAWNY_DENSITY_SUBURB_HPP_
#define TAWNY_DENSITY_SUBURB_HPP_

#include <nlohmann/json_fwd.hpp>  // for json
#include <string>                 // for string, basic_string
#include <vector>                 // for vector

using std::string;
using std::vector;
using json = nlohmann::json;

namespace suburb {
// -------------------------------------
// structures for holding suburb polygon
// -------------------------------------

// lat/lon point
struct Point { double lon{}, lat{}; };

// polygon ring
struct Ring {
    // Closed or open ring of lon/lat points
    vector<Point> points;
};

// polygon shape
struct Polygon {
    // rings[0] = outer; rings[1..] = holes
    vector<Ring> rings;
    // bounding box for fast reject
    double minLon{}, minLat{}, maxLon{}, maxLat{};
};

// suburb from geojson, suburb name, polygons, bounding box
struct Suburb {
    string name;
    vector<Polygon> polys;
    // bounding box
    double minLon{}, minLat{}, maxLon{}, maxLat{};
};

void ringBounds(const Ring& ring, double* minLon, double* minLat, double* maxLon, double* maxLat);
bool pointInRing(const Ring& ring, const Point& point);
bool pointInPolygon(const Polygon& poly, const Point& point);
bool pointInSuburb(const Suburb& suburb, const Point& point);
string detectNameField(const json& props);
vector<Suburb> loadSuburbsGeoJSON(
    const string& path, double* outMinLon, double* outMinLat,
    double* outMaxLon, double* outMaxLat);

}  // namespace suburb

#endif  // TAWNY_DENSITY_SUBURB_HPP_
