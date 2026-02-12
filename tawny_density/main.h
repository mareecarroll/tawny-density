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
#ifndef TAWNY_DENSITY_MAIN_H_
#define TAWNY_DENSITY_MAIN_H_

#include <string>
#include <vector>

using std::string;
using std::vector;

// Forward declarations or shared types
const char USER_AGENT[] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
    "AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/58.0.3029.110 Safari/537.36";

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

// Structure for tawny frogmouth sighting lat/lon
// as per iNaturalist observation
struct ObsPoint { double lon{}, lat{}; };

#endif  // TAWNY_DENSITY_MAIN_H_
