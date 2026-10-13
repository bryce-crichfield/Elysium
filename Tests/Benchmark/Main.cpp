#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include "Benchmarks.h"

int main() {
    RunWorldBenchmarks();
    RunScriptBenchmarks();
    return 0;
}
