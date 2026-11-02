#pragma once
#include <REX/REX/Singleton.h>

class MainManager final : public REX::Singleton<MainManager> {
    std::set<RefID> queued_chests;
    std::set<std::pair<RefID, FormID>> bypass_CanBeAdded;
    std::unordered_map<RefID, std::vector<std::pair<FormID, Count>>> transfer_cache;
    RefID open_chest = 0;
    std::string closed_menu;
    RE::TESObjectREFRPtr containermenu_owner;

    [[nodiscard]] bool ActivateChest(RE::TESObjectREFR* chest) const;
    void OpenChestFromMenu(RE::TESObjectREFR* chest);
    void Gateway(int result, const RE::ObjectRefHandle& target);
    void TransferOnUse(RefID chest);
    [[nodiscard]] RE::TESObjectREFR* GetContainerChest(const RE::TESObjectREFR* target) const;

public:
    [[nodiscard]] RefID GetChest(FormID target) const;
    [[nodiscard]] std::string GetWeightText(RE::TESObjectREFR* target) const;
    [[nodiscard]] std::string GetValueText(RE::TESObjectREFR* target) const;
    [[nodiscard]] bool IsTokenMode(const RE::TESObjectREFR* target) const;
    [[nodiscard]] bool CanRename() const;
    [[nodiscard]] bool Rename(FormID target, const char* name);
    static void RenameCallback(RE::TESForm* target);
    [[nodiscard]] FormID Initialize(FormID target, RefID owner, RE::ExtraDataList* instance);
    [[nodiscard]] bool DeRegister(FormID target);
    [[nodiscard]] Count CanBeAdded(const RE::TESBoundObject* item, Count count, RE::TESObjectREFR* chest);
    void SetCapacityBypass(const std::pair<RefID, FormID>& target, bool enabled);
    void OnActivateContainer(RE::TESObjectREFR* target, int action, int delay = 0);
    void OnLongPressEquip(const RE::TESBoundObject* fake, int delay = 0);
    void CloseMenu();
    void HandleCraftingEnter(RE::TESObjectREFR* furniture);
    void HandleCraftingExit();
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
