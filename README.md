# Tawny Density

Determines the Victorian suburb with the most Tawny Frogmouth sightings in Spring 2025.

## Process

1. Calls the iNaturalist v1 Observations API for Tawny Frogmouth (Podargus strigoides) observations between 2025‑09‑01 and 2025‑11‑30 (Australian spring)
2. Restricts results to the bounding box of the suburbs provided in Victorian suburbs GeoJSON file
3. Assigns each georeferenced observation to a suburb by point‑in‑polygon (Polygon & MultiPolygon, with hole‑aware ray casting)
4. Reports the suburb with the most sightings (and writes a full CSV of suburb Tawny counts).


## API & data notes

- [iNaturalist](inaturalist.org) v1 observations endpoint supports filtering by date range (d1, d2), by taxon name (taxon_name), and by bounding box (swlat, swlng, nelat, nelng). Page size up to 200 per request is supported; pagination can be required for larger result sets.
- iNaturalist’s v1 endpoint limits to 200 results per page and commonly enforces a practical cap of ~10,000 results for a single parameter set; for very large queries you’d page with id_above/id_below. For this Melbourne‑spring‑2025 slice that cap should not be reached, but the program still paginates defensively.
- The “VIC Suburb/Locality Boundaries – Geoscape Administrative Boundaries” dataset provides Victoria-wide localities under CC BY 4.0, available as GeoJSON/WFS. You can either pre‑filter it to Greater Melbourne or pass a Melbourne‑only GeoJSON. See [data.gov.au]

## Requirements

```shell
# Fedora
sudo dnf install -y g++ cmake libcurl-devel

git clone https://github.com/nlohmann/json.git
cd json
mkdir build && cd build
cmake -DJSON_BuildTests=OFF ..
sudo make install
```

## Install pre-commit hooks

```shell
pre-commit install
```

## Running pre-commit

```shell
pre-commit run --files tests/* tawny_density/*
```

## Tests and coverage

Uses doctest (tests/doctest/doctest.h included)

```shell
mkdir -p build && cd build
cmake -DENABLE_COVERAGE=ON ..
cmake --build .
ctest # minimal CI style
gcovr -r .. --html-details coverage.html
gcovr -r .. # terminal
```

Alternatively to run doctest directly, full, verbose and human-friendly.

```script
./tawny_density_tests
```

## Release Build with CMake

```shell
mkdir -p build && cd build
cmake ..
cmake --build . --config Release
```

## Run

```shell
./build/tawny_density --geojson suburb-10-vic.geojson --out counts.csv
```

```shell
Suburbs loaded: 2973
Querying iNaturalist within bbox [-39.1366,140.962] to [-33.9815,149.976] for Podargus strigoides from 2025-09-01 to 2025-11-30 ...
Observations fetched (with coordinates): 1033
Assigned observations: 949
Top suburb (Spring 2025): BLACKBURN — 289 sightings
Wrote counts CSV to counts.csv
```

## Implementation details & assumptions

- If you want to lock to a numeric taxon_id, you can first hit GET /v1/taxa?q=Podargus%20strigoides and pass taxon_id instead. (That’s supported by the same API family.) [inaturalist.org]
- API paging & rate: Pages up to 200 results each; the loop pauses ~1.1s between requests. This follows community best practice to stay below ~1 request/second and avoids the unauthenticated page>100 threshold. [observablehq.com], [inaturalist.org]
- Suburbs GeoJSON: The program uses the provided file as the ground truth and queries iNat with the union bbox of those polygons. To target “suburbs of Melbourne,” pass a Melbourne‑only GeoJSON (e.g., a filtered subset of the Geoscape VIC Localities). [data.gov.au]
- Geometry: Handles Polygon and MultiPolygon, supports holes, and uses AABB‑first filtering for speed. Coordinates are interpreted as [lon, lat] (WGS84) per GeoJSON spec.
- Tie‑breaking: If two suburbs have the same max count, the first encountered wins. If you want a deterministic tie resolution (e.g., alphabetical), sort before selecting.
