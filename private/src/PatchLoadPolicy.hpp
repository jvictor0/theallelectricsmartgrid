#pragma once

struct PatchLoadPolicy
{
    static bool ShouldRestoreFaders(bool wrldBldrOpen, bool reloadPatch)
    {
        return !wrldBldrOpen && !reloadPatch;
    }
};
