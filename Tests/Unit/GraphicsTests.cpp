#include <doctest/doctest.h>
#include "Core/Components/TextComponent.h"
#include "Core/Graphics.h"

using namespace Elysium;

TEST_CASE("A Color left unset is opaque") {
    CHECK(Color{}.a == 255);
    CHECK(Color{10, 20, 30}.a == 255);
    CHECK(Color{10, 20, 30, 0}.a == 0);
    CHECK(TextComponent().color.a == 255);
}
