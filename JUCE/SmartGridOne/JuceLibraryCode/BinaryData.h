/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   mod_badge_LFO_png;
    const int            mod_badge_LFO_pngSize = 1089;

    extern const char*   mod_badge_ADSR_png;
    const int            mod_badge_ADSR_pngSize = 1173;

    extern const char*   mod_badge_Sheaf_png;
    const int            mod_badge_Sheaf_pngSize = 1960;

    extern const char*   mod_badge_SmoothRandom_png;
    const int            mod_badge_SmoothRandom_pngSize = 3412;

    extern const char*   mod_badge_Noise_png;
    const int            mod_badge_Noise_pngSize = 2842;

    extern const char*   mod_badge_Quadrature_png;
    const int            mod_badge_Quadrature_pngSize = 1239;

    extern const char*   mod_badge_Spread_png;
    const int            mod_badge_Spread_pngSize = 940;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 7;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
