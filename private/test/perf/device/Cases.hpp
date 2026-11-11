#pragma once

#include <ostream>
#include <string>
#include <vector>

// Cases run serially on the benchmark thread and emit JSON objects, one per
// line. Each case owns its arguments, measurement boundaries and metric names.
//
struct BenchmarkCase
{
    const char* m_name;
    const char* m_notes;
    void (*m_run)(const std::vector<std::string>& arguments, std::ostream& output);
};

const std::vector<BenchmarkCase>& BenchmarkCases();
