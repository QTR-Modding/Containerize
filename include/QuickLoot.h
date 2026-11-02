#pragma once

namespace QuickLootIntegration {
    void Install();
    void Update(RE::TESObjectREFR* target);
    void Close();
    void Reset();
    [[nodiscard]] bool IsInUse(RefID chest);
}
