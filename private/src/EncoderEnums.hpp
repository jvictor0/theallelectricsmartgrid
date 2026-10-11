#pragma once

namespace SmartGrid
{
enum class EncoderType
{
    BaseParam,
    ModulatorAmount,
    GestureParam
};
}

namespace SmartGridOne
{
enum class EncoderBankMode : int
{
    VoiceWater = 0,
    VoiceFire = 1,
    VoiceEarth = 2,
    Quad = 3,
    Global = 4,
    NumModes = 5,
    Voice = VoiceWater
};
}
