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

#include "main.hpp"
#include <algorithm>              // for max, min
#include <cstddef>                // for size_t
#include <cstdint>                // for uint64_t
#include <exception>              // for exception
#include <fstream>                // for basic_ostream, operator<<, basic_of...
#include <iostream>               // for cerr, cout
#include <nlohmann/json_fwd.hpp>  // for json
#include <optional>               // for optional
#include <stdexcept>              // for runtime_error
#include <string>                 // for basic_string, char_traits, allocator
#include <unordered_map>          // for unordered_map
#include <utility>                // for pair
#include <vector>                 // for vector
#include "observations.hpp"       // for ObsPoint, fetchINatPoints
#include "suburb.hpp"             // for loadSuburbsGeoJSON, pointInSuburb

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::min;
using std::max;
using std::runtime_error;
using std::optional;
using std::ofstream;
using std::unordered_map;
using std::exception;
using json = nlohmann::json;

using suburb::loadSuburbsGeoJSON;
using suburb::Point;
using observations::fetchINatPoints;

// input args for main entry point
struct Args {
    string geojsonPath;
    optional<string> outCsv;
};

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

// Entry point
int main(int argc, char** argv) {
    Args args;
    if (!parseArgs(argc, argv, &args)) {
        usage(argv[0]);
        return 1;
    }

    try {
        // 1) Load suburbs
        double minLon, minLat, maxLon, maxLat;  // collect bonding box for Victoria
        auto suburbs = loadSuburbsGeoJSON(args.geojsonPath, &minLon, &minLat, &maxLon, &maxLat);

        // 2) Fetch iNaturalist sightings for Spring 2025

        // iNat bounding box expects (swlat, swlng, nelat, nelng)
        const double swlat = minLat, swlng = minLon, nelat = maxLat, nelng = maxLon;

        cerr << "Suburbs loaded: " << suburbs.size() << "\n";
        cerr << "Querying iNaturalist within bbox ["
            << swlat << "," << swlng << "] to [" << nelat << "," << nelng
            << "] for " << TAWNY_TAXON << " from " << SPRING_2025_START_DATE
            << " to " << SPRING_2025_END_DATE << " ...\n";

        auto obs = fetchINatPoints(TAWNY_TAXON, SPRING_2025_START_DATE, SPRING_2025_END_DATE,
            swlat, swlng, nelat, nelng);
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
        return 2;
    }

    return 0;
}
