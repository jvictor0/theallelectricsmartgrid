#include "SgrecReplay.hpp"
#include <exception>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 5 && argc != 6)
    {
        std::cerr << "Usage: perf_sgrec PREPARED.json CALLBACK_FRAMES SECONDS|all OUTPUT.jsonl [FINAL_PATCH.json]\n";
        return 1;
    }

    try
    {
        RunSgrecReplay(argv[1], argv[2], argv[3], argv[4], argc == 6 ? argv[5] : nullptr, std::cerr);
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
