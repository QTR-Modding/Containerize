#pragma once
#include <REX/REX/Singleton.h>

class MainManager final : public REX::Singleton<MainManager> {
    std::set<RefID> queued_chests;
    RefID open_chest = 0;
    std::string closed_menu;
    RE::TESObjectREFRPtr containermenu_owner;

    [[nodiscard]] bool ActivateChest(RE::TESObjectREFR* chest) const;
    void OpenChestFromMenu(RE::TESObjectREFR* chest);
    void Gateway(int result, const RE::ObjectRefHandle& target);
    [[nodiscard]] RE::TESObjectREFR* GetContainerChest(const RE::TESObjectREFR* target) const;

public:
    void OnActivateContainer(RE::TESObjectREFR* target, int action, int delay = 0);
    void OnLongPressEquip(const RE::TESBoundObject* fake, int delay = 0);
    void CloseMenu();
    void OnChestEnter(RE::TESObjectREFR* chest);
    void OnChestExit(RE::TESObjectREFR* chest);
    bool IsInChestMenu() const { return open_chest != 0; }
    bool IsChestMenuQueued() const { return !queued_chests.empty(); }
    bool IsInUse(RefID chest) const;
    bool IsChest(RefID chest) const;
    bool IsContainer(const RE::TESObjectREFR* target) const;
    void ClearOpenChest(RefID chest);
    void ForgetChest(RefID chest);
    void Reset();
};
