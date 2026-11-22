#pragma once

#include <iosfwd>

void RunSgrecReplay(const char* preparedPath, const char* callbackSize, const char* duration,
                    const char* callbacksPath, const char* finalPatchPath, std::ostream& progress);
