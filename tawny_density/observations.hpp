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
#ifndef TAWNY_DENSITY_OBSERVATIONS_H_
#define TAWNY_DENSITY_OBSERVATIONS_H_

#include <stddef.h>  // for size_t
#include <string>    // for string
#include <vector>    // for vector

using std::vector;
using std::string;

namespace observations {

const char USER_AGENT[] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
    "AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/58.0.3029.110 Safari/537.36";

const char URL_BASE[] = "https://api.inaturalist.org/v1/observations";

const int PER_PAGE = 200;  // v1: max 200 per page

// Structure for observation lat/lon
// as per iNaturalist observation
struct ObsPoint { double lon{}, lat{}; };

static size_t curlWriteToString(void* contents, size_t size, size_t nmemb, void* userData);
string urlEncode(const string& url);
string httpGet(const string& url);

vector<ObsPoint> fetchINatPoints(
    const std::string& taxonName,
    const string& d1, const std::string& d2,
    double swlat, double swlng, double nelat, double nelng);

} // namespace observations

#endif  // TAWNY_DENSITY_OBSERVATIONS_H_
