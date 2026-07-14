#include "Core/CommandLine.h"
#include <algorithm>
#include <cctype>

namespace Elysium {

namespace {

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Splits "--Key=Value" into {"--key", "Value"}; bare "--Key" yields {"--key", ""}.
std::pair<std::string, std::string> SplitArg(const std::string& arg) {
    size_t eq = arg.find('=');
    if (eq == std::string::npos) {
        return {ToLower(arg), ""};
    }
    return {ToLower(arg.substr(0, eq)), arg.substr(eq + 1)};
}

}  // namespace

CommandLineArgs CommandLine::Parse(int argc, char** argv) {
    CommandLineArgs args;

    for (int i = 1; i < argc; ++i) {
        auto [key, value] = SplitArg(argv[i]);

        if (key == "--project" && !value.empty()) {
            args.projectPath = value;
        } else if (key == "--editor") {
            args.editor = value.empty() || ToLower(value) == "true";
        }
    }

    return args;
}

}  // namespace Elysium
