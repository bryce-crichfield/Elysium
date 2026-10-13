#include <doctest/doctest.h>
#include "Core/Math/Polygon.h"
#include "Core/Xml.h"

using namespace Elysium;

TEST_CASE("ColorToHex and ParseHexColor round-trip an opaque color") {
    Color color{18, 52, 86, 255};
    CHECK(ColorToHex(color) == "#123456FF");
    Color parsed = ParseHexColor(ColorToHex(color));
    CHECK(parsed.r == 18);
    CHECK(parsed.g == 52);
    CHECK(parsed.b == 86);
    CHECK(parsed.a == 255);
}

TEST_CASE("ParseHexColor reads #RRGGBB as opaque and falls back on bad input") {
    CHECK(ParseHexColor("#FF0000").a == 255);
    CHECK(ParseHexColor("red", Colors::White).r == Colors::White.r);
    CHECK(ParseHexColor("", Colors::White).a == Colors::White.a);
}

TEST_CASE("Polygon::Parse and Format round-trip, skipping malformed tokens") {
    Polygon polygon = Polygon::Parse("0,0 10,0 junk 10,5");
    REQUIRE(polygon.Size() == 3);
    CHECK(polygon[2].x == 10.0f);
    CHECK(polygon[2].y == 5.0f);
    CHECK(Polygon::Parse(polygon.Format()).Size() == 3);
}
