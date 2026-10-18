// Game-specific patches.
#pragma once

namespace game_patches
{
    enum class PatchId
    {
        Fps60Unlock,
        AspectRatio16_9
    };

    bool IsEnabled(PatchId patch);
    void ApplyEnabledPatches();
} // namespace game_patches
