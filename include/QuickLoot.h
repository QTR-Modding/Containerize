#pragma once
#include "QuickLootAPI.h"

namespace QuickLootIntegration {
    void OnModifyItemData(QuickLoot::API::ModifyItemDataEvent* event);
    void Install();
    bool Open(RE::TESObjectREFR* target, RE::TESObjectREFR* chest);
    bool Update(RE::TESObjectREFR* target);
    void Close(bool play_animation = true);
    void Reset();
    [[nodiscard]] bool IsInUse(RefID chest);
}
