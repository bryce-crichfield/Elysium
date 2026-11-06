#include <doctest/doctest.h>
#include <functional>
#include <string>
#include "Core/ServiceLocator.h"
#include "Core/Shader.h"
#include "Services/Web/WebTaskService.h"

using namespace Elysium;

TEST_CASE("ToGlslEs swaps the desktop version line for GLSL ES 3.00 with precisions") {
    const std::string desktop = "#version 330\nin vec2 uv;\nout vec4 finalColor;\nvoid main() {}\n";
    const std::string es = ToGlslEs(desktop);

    CHECK(es.rfind("#version 300 es\n", 0) == 0);
    CHECK(es.find("precision highp float;") != std::string::npos);
    CHECK(es.find("#version 330") == std::string::npos);
    CHECK(es.find("in vec2 uv;\nout vec4 finalColor;\nvoid main() {}\n") != std::string::npos);
}

TEST_CASE("ToGlslEs adds a version to source that has none") {
    const std::string es = ToGlslEs("in vec2 uv;\nvoid main() {}\n");

    CHECK(es.rfind("#version 300 es\n", 0) == 0);
    CHECK(es.find("in vec2 uv;") != std::string::npos);
}

TEST_CASE("ToGlslEs drops a version line that isn't first") {
    const std::string es = ToGlslEs("// header\n#version 330 core\nvoid main() {}\n");

    CHECK(es.rfind("#version 300 es\n", 0) == 0);
    CHECK(es.find("330") == std::string::npos);
    CHECK(es.find("// header\n") != std::string::npos);
}

TEST_CASE("WebTaskService runs submitted work on Update, on the calling thread") {
    ServiceLocator locator;
    WebTaskService tasks(locator);
    tasks.Initialize();

    int result = 0;
    tasks.Submit<int>([] { return 42; }).Then([&](const int& value) { result = value; });

    CHECK_FALSE(tasks.IsIdle());
    CHECK(result == 0);

    tasks.Update(0.0f);

    CHECK(result == 42);
    CHECK(tasks.IsIdle());
}

TEST_CASE("WebTaskService runs work submitted from a continuation on a later Update") {
    ServiceLocator locator;
    WebTaskService tasks(locator);
    tasks.Initialize();

    std::string order;
    tasks.Submit<int>([] { return 1; }).Then([&](const int&) {
        order += "first ";
        tasks.Submit<int>([] { return 2; }).Then([&](const int&) { order += "second"; });
    });

    tasks.Update(0.0f);
    CHECK(order == "first ");
    CHECK_FALSE(tasks.IsIdle());

    tasks.Update(0.0f);
    CHECK(order == "first second");
    CHECK(tasks.IsIdle());
}
