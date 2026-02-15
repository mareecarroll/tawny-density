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
#include <doctest/doctest.h>
#include <string>
#include "../tawny_density/suburb.hpp"

using std::string;

using suburb::Point;
using suburb::Ring;
using suburb::Polygon;
using suburb::Suburb;

using suburb::ringBounds;
using suburb::pointInRing;
using suburb::pointInPolygon;
using suburb::pointInSuburb;

// -----------------------------------------------------------------------------
// Tests for ringBounds
// -----------------------------------------------------------------------------

TEST_CASE("ringBounds computes correct bounds for a simple ring") {
    Ring ring{
        { {1.0, 2.0}, {3.0, -1.0}, {2.5, 4.0} }
    };

    double minLon, minLat, maxLon, maxLat;
    ringBounds(ring, &minLon, &minLat, &maxLon, &maxLat);

    CHECK(minLon == doctest::Approx(1.0));
    CHECK(maxLon == doctest::Approx(3.0));
    CHECK(minLat == doctest::Approx(-1.0));
    CHECK(maxLat == doctest::Approx(4.0));
}

TEST_CASE("ringBounds handles negative coordinates") {
    Ring ring{
        { {-10.0, -20.0}, {-5.0, -25.0}, {-7.0, -22.0} }
    };

    double minLon, minLat, maxLon, maxLat;
    ringBounds(ring, &minLon, &minLat, &maxLon, &maxLat);

    CHECK(minLon == doctest::Approx(-10.0));
    CHECK(maxLon == doctest::Approx(-5.0));
    CHECK(minLat == doctest::Approx(-25.0));
    CHECK(maxLat == doctest::Approx(-20.0));
}

TEST_CASE("ringBounds handles a single-point ring") {
    Ring ring{
        { {42.0, -17.0} }
    };

    double minLon, minLat, maxLon, maxLat;
    ringBounds(ring, &minLon, &minLat, &maxLon, &maxLat);

    CHECK(minLon == doctest::Approx(42.0));
    CHECK(maxLon == doctest::Approx(42.0));
    CHECK(minLat == doctest::Approx(-17.0));
    CHECK(maxLat == doctest::Approx(-17.0));
}

TEST_CASE("ringBounds handles identical points") {
    Ring ring{
        { {5.0, 5.0}, {5.0, 5.0}, {5.0, 5.0} }
    };

    double minLon, minLat, maxLon, maxLat;
    ringBounds(ring, &minLon, &minLat, &maxLon, &maxLat);

    CHECK(minLon == doctest::Approx(5.0));
    CHECK(maxLon == doctest::Approx(5.0));
    CHECK(minLat == doctest::Approx(5.0));
    CHECK(maxLat == doctest::Approx(5.0));
}

TEST_CASE("ringBounds handles an empty ring gracefully") {
    Ring ring{};

    double minLon, minLat, maxLon, maxLat;
    ringBounds(ring, &minLon, &minLat, &maxLon, &maxLat);

    // With no points, the function leaves the initial sentinel values.
    CHECK(minLon == doctest::Approx(1e300));
    CHECK(minLat == doctest::Approx(1e300));
    CHECK(maxLon == doctest::Approx(-1e300));
    CHECK(maxLat == doctest::Approx(-1e300));
}

// -----------------------------------------------------------------------------
// Tests for pointInRing
// -----------------------------------------------------------------------------

TEST_CASE("pointInRing: point inside a simple square") {
    Ring ring{
        { {0, 0}, {10, 0}, {10, 10}, {0, 10} }
    };

    Point p{5, 5};
    CHECK(pointInRing(ring, p) == true);
}

TEST_CASE("pointInRing: point outside a simple square") {
    Ring ring{
        { {0, 0}, {10, 0}, {10, 10}, {0, 10} }
    };

    Point p{20, 20};
    CHECK(pointInRing(ring, p) == false);
}

// TEST_CASE("pointInRing: point on an edge is treated as inside") {
//     Ring ring{
//         { {0, 0}, {10, 0}, {10, 10}, {0, 10} }
//     };

//     Point p{5, 0};   // lies exactly on bottom edge
//     CHECK(pointInRing(ring, p) == true);
// }

// TEST_CASE("pointInRing: point on a vertex is treated as inside") {
//     Ring ring{
//         { {0, 0}, {10, 0}, {10, 10}, {0, 10} }
//     };

//     Point p{0, 0};
//     CHECK(pointInRing(ring, p) == true);
// }

TEST_CASE("pointInRing: concave polygon, point in concavity is outside") {
    // A simple concave shape (a "C" shape)
    Ring ring{
        { {0, 0}, {10, 0}, {10, 10}, {6, 10}, {6, 4}, {4, 4}, {4, 10}, {0, 10} }
    };

    Point inside{5, 5};   // inside the concave "bite"
    Point outside{8, 5};  // inside the outer box but outside the polygon

    CHECK(pointInRing(ring, inside) == false);
    CHECK(pointInRing(ring, outside) == true);
}

TEST_CASE("pointInRing: ring with fewer than 3 points is always false") {
    Ring ring1{ { {0, 0}, {1, 1} } };
    Ring ring2{ { {0, 0} } };
    Ring ring3{ };

    CHECK(pointInRing(ring1, {0, 0}) == false);
    CHECK(pointInRing(ring2, {0, 0}) == false);
    CHECK(pointInRing(ring3, {0, 0}) == false);
}

// -----------------------------------------------------------------------------
// Tests for pointInPolygon
// -----------------------------------------------------------------------------

TEST_CASE("pointInPolygon: point inside simple square") {
    Polygon poly;
    poly.minLon = 0; poly.minLat = 0;
    poly.maxLon = 10; poly.maxLat = 10;
    poly.rings = {
        Ring{ { {0, 0}, {10, 0}, {10, 10}, {0, 10} } }
    };

    CHECK(pointInPolygon(poly, {5, 5}) == true);
}

TEST_CASE("pointInPolygon: point outside simple square") {
    Polygon poly;
    poly.minLon = 0; poly.minLat = 0;
    poly.maxLon = 10; poly.maxLat = 10;
    poly.rings = {
        Ring{ { {0, 0}, {10, 0}, {10, 10}, {0, 10} } }
    };

    CHECK(pointInPolygon(poly, {20, 20}) == false);
}

TEST_CASE("pointInPolygon: bounding box fast reject") {
    Polygon poly;
    poly.minLon = 0; poly.minLat = 0;
    poly.maxLon = 10; poly.maxLat = 10;
    poly.rings = {
        Ring{ { {0, 0}, {10, 0}, {10, 10}, {0, 10} } }
    };

    // Outside bbox but would be inside if bbox were ignored
    CHECK(pointInPolygon(poly, {5, 20}) == false);
}

// TEST_CASE("pointInPolygon: point on boundary is inside") {
//     Polygon poly;
//     poly.minLon = 0; poly.minLat = 0;
//     poly.maxLon = 10; poly.maxLat = 10;
//     poly.rings = {
//         Ring{ { {0, 0}, {10, 0}, {10, 10}, {0, 10} } }
//     };

//     CHECK(pointInPolygon(poly, {5, 0}) == true);   // on bottom edge
//     CHECK(pointInPolygon(poly, {0, 0}) == true);   // on vertex
// }

TEST_CASE("pointInPolygon: point inside hole returns false") {
    Polygon poly;
    poly.minLon = 0; poly.minLat = 0;
    poly.maxLon = 10; poly.maxLat = 10;

    Ring outer{ { {0, 0}, {10, 0}, {10, 10}, {0, 10} } };
    Ring hole{  { {3, 3}, {7, 3}, {7, 7}, {3, 7} } };

    poly.rings = { outer, hole };

    CHECK(pointInPolygon(poly, {5, 5}) == false);  // inside hole
    CHECK(pointInPolygon(poly, {1, 1}) == true);   // inside outer, not in hole
}

TEST_CASE("pointInPolygon: concave outer ring") {
    // A simple concave polygon (a "C" shape)
    Polygon poly;
    poly.minLon = 0; poly.minLat = 0;
    poly.maxLon = 10; poly.maxLat = 10;

    poly.rings = {
        Ring{ {
            {0, 0}, {10, 0}, {10, 10}, {6, 10},
            {6, 4}, {4, 4}, {4, 10}, {0, 10}
        } }
    };

    CHECK(pointInPolygon(poly, {8, 5}) == true);   // inside
    CHECK(pointInPolygon(poly, {5, 5}) == false);  // in concavity
}

TEST_CASE("pointInPolygon: polygon with no rings is always false") {
    Polygon poly;
    poly.minLon = 0; poly.minLat = 0;
    poly.maxLon = 10; poly.maxLat = 10;
    poly.rings = {};

    CHECK(pointInPolygon(poly, {5, 5}) == false);
}

TEST_CASE("pointInPolygon: outer ring with fewer than 3 points is false") {
    Polygon poly;
    poly.minLon = 0; poly.minLat = 0;
    poly.maxLon = 10; poly.maxLat = 10;

    poly.rings = {
        Ring{ { {0, 0}, {10, 0} } }   // invalid ring
    };

    CHECK(pointInPolygon(poly, {5, 5}) == false);
}

// -----------------------------------------------------------------------------
// Tests for pointInSuburb
// -----------------------------------------------------------------------------

// TEST_CASE("pointInSuburb: inside suburb bbox and inside polygon") {
//     Suburb s;
//     s.name = "Testville";
//     s.minLon = 0; s.minLat = 0;
//     s.maxLon = 10; s.maxLat = 10;

//     Polygon poly;
//     poly.minLon = 0; poly.minLat = 0;
//     poly.maxLon = 10; poly.maxLat = 10;
//     poly.rings = {
//         Ring{ { {0,0}, {10,0}, {10,10}, {0,10} } }
//     };

//     s.polys = { poly };

//     CHECK(pointInSuburb(s, {5,5}) == true);
// }

// TEST_CASE("pointInSuburb: outside suburb bounding box -> fast reject") {
//     Suburb s;
//     s.name = "Testville";
//     s.minLon = 0; s.minLat = 0;
//     s.maxLon = 10; s.maxLat = 10;

//     Polygon poly;
//     poly.minLon = 0; poly.minLat = 0;
//     poly.maxLon = 10; poly.maxLat = 10;
//     poly.rings = {
//         Ring{ { {0,0}, {10,0}, {10,10}, {0,10} } }
//     };

//     s.polys = { poly };

//     CHECK(pointInSuburb(s, {20,20}) == false);
//     CHECK(pointInSuburb(s, {-5,5}) == false);
//     CHECK(pointInSuburb(s, {5,-5}) == false);
// }

// TEST_CASE("pointInSuburb: inside bbox but outside polygon") {
//     Suburb s;
//     s.name = "Testville";
//     s.minLon = 0; s.minLat = 0;
//     s.maxLon = 10; s.maxLat = 10;

//     Polygon poly;
//     poly.minLon = 2; poly.minLat = 2;
//     poly.maxLon = 8; poly.maxLat = 8;
//     poly.rings = {
//         Ring{ { {2,2}, {8,2}, {8,8}, {2,8} } }
//     };

//     s.polys = { poly };

//     CHECK(pointInSuburb(s, {1,1}) == false);  // inside suburb bbox, but outside polygon
//     CHECK(pointInSuburb(s, {9,9}) == false);
// }

// TEST_CASE("pointInSuburb: boundary points are inside") {
//     Suburb s;
//     s.name = "Testville";
//     s.minLon = 0; s.minLat = 0;
//     s.maxLon = 10; s.maxLat = 10;

//     Polygon poly;
//     poly.minLon = 0; poly.minLat = 0;
//     poly.maxLon = 10; poly.maxLat = 10;
//     poly.rings = {
//         Ring{ { {0,0}, {10,0}, {10,10}, {0,10} } }
//     };

//     s.polys = { poly };

//     INFO("Suburb bbox: " << s.minLon << ", " << s.minLat << " to "
//         << s.maxLon << ", " << s.maxLat);
//     INFO("Polygon bbox: " << poly.minLon << ", " << poly.minLat << " to "
//         << poly.maxLon << ", " << poly.maxLat);


//     CHECK(pointInSuburb(s, {0,0}) == true);
//     CHECK(pointInSuburb(s, {10,10}) == true);
//     CHECK(pointInSuburb(s, {5,0}) == true);
// }

// TEST_CASE("pointInSuburb: multiple polygons") {
//     Suburb s;
//     s.name = "TwinPolys";
//     s.minLon = 0; s.minLat = 0;
//     s.maxLon = 20; s.maxLat = 20;

//     Polygon p1;
//     p1.minLon = 0; p1.minLat = 0;
//     p1.maxLon = 10; p1.maxLat = 10;
//     p1.rings = {
//         Ring{ { {0,0}, {10,0}, {10,10}, {0,10} } }
//     };

//     Polygon p2;
//     p2.minLon = 10; p2.minLat = 10;
//     p2.maxLon = 20; p2.maxLat = 20;
//     p2.rings = {
//         Ring{ { {10,10}, {20,10}, {20,20}, {10,20} } }
//     };

//     s.polys = { p1, p2 };

//     CHECK(pointInSuburb(s, {5,5}) == true);     // inside p1
//     CHECK(pointInSuburb(s, {15,15}) == true);   // inside p2
//     CHECK(pointInSuburb(s, {30,30}) == false);  // outside bbox
// }

// TEST_CASE("pointInSuburb: point inside a hole returns false") {
//     Suburb s;
//     s.name = "HoleTown";
//     s.minLon = 0; s.minLat = 0;
//     s.maxLon = 10; s.maxLat = 10;

//     Polygon poly;
//     poly.minLon = 0; poly.minLat = 0;
//     poly.maxLon = 10; poly.maxLat = 10;

//     Ring outer{ { {0,0}, {10,0}, {10,10}, {0,10} } };
//     Ring hole{  { {3,3}, {7,3}, {7,7}, {3,7} } };

//     poly.rings = { outer, hole };
//     s.polys = { poly };

//     CHECK(pointInSuburb(s, {5,5}) == false);  // inside hole
//     CHECK(pointInSuburb(s, {1,1}) == true);   // inside outer, not in hole
// }

// TEST_CASE("pointInSuburb: suburb with no polygons returns false") {
//     Suburb s;
//     s.name = "Emptyville";
//     s.minLon = 0; s.minLat = 0;
//     s.maxLon = 10; s.maxLat = 10;
//     s.polys = {};

//     CHECK(pointInSuburb(s, {5,5}) == false);
// }

// // -----------------------------------------------------------------------------
// // Tests for isPointInRing
// // -----------------------------------------------------------------------------

// TEST_CASE("isPointInRing: point inside a simple square") {
//     Ring ring{
//         { {0, 0}, {10, 0}, {10, 10}, {0, 10} }
//     };

//     Point p{5, 5};
//     CHECK(isPointInRing(ring, p) == true);
// }

// TEST_CASE("isPointInRing: point outside a simple square") {
//     Ring ring{
//         { {0, 0}, {10, 0}, {10, 10}, {0, 10} }
//     };

//     Point p{20, 20};
//     CHECK(isPointInRing(ring, p) == false);
// }

// // TEST_CASE("isPointInRing: point on an edge is treated as inside") {
// //     Ring ring{
// //         { {0, 0}, {10, 0}, {10, 10}, {0, 10} }
// //     };

// //     Point p{5, 0};   // lies exactly on bottom edge
// //     CHECK_EQ(isPointInRing(ring, p), true);
// // }

// // TEST_CASE("isPointInRing: point on a vertex is treated as inside") {
// //     Ring ring{
// //         { {0, 0}, {10, 0}, {10, 10}, {0, 10} }
// //     };

// //     Point p{0, 0};
// //     CHECK_EQ(isPointInRing(ring, p), true);
// // }

// TEST_CASE("isPointInRing: concave polygon, point in concavity is outside") {
//     // A simple concave shape (a "C" shape)
//     Ring ring{
//         { {0, 0}, {10, 0}, {10, 10}, {6, 10}, {6, 4}, {4, 4}, {4, 10}, {0, 10} }
//     };

//     Point inside{5, 5};   // inside the concave "bite"
//     Point outside{8, 5};  // inside the outer box but outside the polygon

//     CHECK(isPointInRing(ring, inside) == false);
//     CHECK(isPointInRing(ring, outside) == true);
// }

// TEST_CASE("isPointInRing: ring with fewer than 3 points is always false") {
//     Ring ring1{ { {0, 0}, {1, 1} } };
//     Ring ring2{ { {0, 0} } };
//     Ring ring3{ };

//     CHECK(isPointInRing(ring1, {0, 0}) == false);
//     CHECK(isPointInRing(ring2, {0, 0}) == false);
//     CHECK(isPointInRing(ring3, {0, 0}) == false);
// }
