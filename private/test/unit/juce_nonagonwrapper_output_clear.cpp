#include "doctest.h"

#include <fstream>
#include <sstream>
#include <string>

DOCTEST_TEST_CASE("NonagonWrapper clears each output sample before writing owned output channels")
{
    std::ifstream file(std::string(SMARTGRID_REPO_ROOT) + "/JUCE/SmartGridOne/Source/NonagonWrapper.hpp");
    DOCTEST_REQUIRE(file.good());

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string source = ss.str();

    size_t processPos = source.find("void Process(const juce::AudioSourceChannelInfo& bufferToFill");
    DOCTEST_REQUIRE(processPos != std::string::npos);

    size_t inputReadPos = source.find("audioInputBuffer.m_input[j] =", processPos);
    size_t clearPos = source.find("bufferToFill.buffer->getWritePointer(j, bufferToFill.startSample)[i] = 0.0f;", processPos);
    size_t outputPos = source.find("QuadFloatWithStereoAndSub output = ProcessSample(audioInputBuffer);", processPos);

    DOCTEST_CHECK(inputReadPos != std::string::npos);
    DOCTEST_CHECK(clearPos != std::string::npos);
    DOCTEST_CHECK(outputPos != std::string::npos);
    DOCTEST_CHECK(inputReadPos < clearPos);
    DOCTEST_CHECK(clearPos < outputPos);
}
