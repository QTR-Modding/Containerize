#include "MainManager.h"
#include "Manager.h"
#include "WorldStorage.h"
#include "Chest.h"
#include "Animations.h"
#include "Papyrus.h"
#include "QuickLoot.h"
#include "Events.h"
#include "CLibUtilsQTR/Tasker.hpp"

bool MainManager::ActivateChest(RE::TESObjectREFR* chest) const {
    if (!chest) {
        logger::error("ActivateChest: Chest reference is null.");
        return false;
    }
    QuickLootIntegration::Close();
    if (Manager::GetSingleton()->isUninstalled.load()) return false;
    if (WorldStorage::GetSingleton()->GetTarget(chest->GetFormID())) {
        return Menu::ActivateChest(chest, chest->GetDisplayFullName());
    }
    return Menu::ActivateChest(chest, Manager::GetSingleton()->GetChestName(chest));
}

void MainManager::OpenChestFromMenu(RE::TESObjectREFR* a_chest) {
    if (Manager::GetSingleton()->isUninstalled.load()) return;
    if (!closed_menu.empty()) {
        if (!RE::UI::GetSingleton()->IsMenuOpen(closed_menu)) {
            if (!ActivateChest(a_chest)) {
                logger::error("ActivateChest failed for chest: {:x}", a_chest->GetFormID());
            }
        } else {
            clib_utilsQTR::Tasker::GetSingleton()->PushTask(
                [this,a_chest] {
                    SKSE::GetTaskInterface()->AddUITask([this,a_chest] { OpenChestFromMenu(a_chest); });
                }, 500
                );
        }
    }
}

void MainManager::Gateway(const int result, const RE::ObjectRefHandle& a_current_container, const bool use_quickloot) {
    if (result != 0 && result != 1) {
        logger::error("Unexpected callback integer!");
        return;
    }
    const auto a_chest_handle = a_current_container.get();
    const auto a_chest = a_chest_handle ? GetContainerChest(a_chest_handle.get()) : nullptr;
    if (!a_chest) {
        logger::warn("Current container is null.");
    } else if (result) {
        if (std::ranges::contains(WorldStorage::form_types, a_chest_handle->GetBaseObject()->GetFormType())) {
            RenameCallback(a_chest_handle.get());
        } else {
            RenameCallback(Manager::GetSingleton()->GetFakeBound(a_chest_handle.get()));
        }
    } else {
        if (use_quickloot) queued_chests.erase(a_chest->GetFormID());
        const bool quickloot_opened = use_quickloot && QuickLootIntegration::Open(a_chest_handle.get(), a_chest);
        if (use_quickloot && !quickloot_opened) queued_chests.insert(a_chest->GetFormID());
        if (!quickloot_opened && !ActivateChest(a_chest)) {
            if (!WorldStorage::GetSingleton()->GetTarget(a_chest->GetFormID())) Manager::GetSingleton()->OnOpenFailed();
            open_chest = 0;
            queued_chests.clear();
            Animations::SendAnimEvent(3, nullptr);
            logger::warn("Chest not found.");
        }
    }
}

void MainManager::OnLongPressEquip(const RE::TESBoundObject* a_fake, const int delay) {
    auto chest = Manager::GetSingleton()->GetFakeContainerChest(a_fake);
    if (!chest) {
        logger::error("OnLongPressEquip: chest null");
        return;
    }
    queued_chests.insert(chest->GetFormID());
    if (delay > 0) {
        clib_utilsQTR::Tasker::GetSingleton()->PushTask([this,chest] {
            SKSE::GetTaskInterface()->AddUITask([this,chest] { OpenChestFromMenu(chest); });
        }, delay);
    } else {
        SKSE::GetTaskInterface()->AddUITask([this,chest] { OpenChestFromMenu(chest); });
    }
}

void MainManager::OnActivateContainer(RE::TESObjectREFR* a_container, const int msgbox_action, const int a_delay, const bool use_quickloot) {
    if (Manager::GetSingleton()->isUninstalled.load()) return;
    const auto base = a_container->GetBaseObject();
    if (std::ranges::contains(WorldStorage::form_types, base->GetFormType())) {
        const auto world_storage = WorldStorage::GetSingleton();
        const auto source = world_storage->GetSource(base->GetFormID());
        if (!source || source->token_mode || !world_storage->Initialize(a_container, nullptr)) return;
    } else if (!Manager::GetSingleton()->HandleRegistration(a_container)) {
        return;
    }
    if (msgbox_action == 0) {
        if (const auto chest = GetContainerChest(a_container)) queued_chests.insert(chest->GetFormID());
    }
    auto a_handle = a_container->GetHandle();
    if (a_delay > 0) {
        clib_utilsQTR::Tasker::GetSingleton()->PushTask([this,msgbox_action,a_handle,use_quickloot] {
            Gateway(msgbox_action, a_handle, use_quickloot);
        }, a_delay);
    } else {
        return Gateway(msgbox_action, a_handle, use_quickloot);
    }
}

void MainManager::CloseMenu() {
    containermenu_owner.reset();
    if (!Menu::GetContainerMenuOwner(containermenu_owner)) {
        containermenu_owner.reset();
    }
    closed_menu = Menu::CloseMenu();
}

void MainManager::OnChestEnter(RE::TESObjectREFR* chest) {
    const auto chest_id = chest->GetFormID();
    queued_chests.erase(chest_id);
    open_chest = chest_id;
    const auto manager = Manager::GetSingleton();
    if (manager->IsChest(chest_id) && !manager->OnChestEnter(chest)) open_chest = 0;
}

void MainManager::OnChestExit(RE::TESObjectREFR* chest) {
    const auto chest_id = chest->GetFormID();
    const auto manager = Manager::GetSingleton();
    const bool was_open = open_chest == chest_id;
    const bool next_menu_is_chest = containermenu_owner && IsChest(containermenu_owner->GetFormID());
    const bool inventory_container = manager->IsChest(chest_id);
    if (inventory_container && !manager->OnChestExit(chest)) return;
    if (was_open) {
        open_chest = 0;
        if (Settings::other_settings.at(Settings::otherstuffKeys[2]) && queued_chests.empty()) {
            if (closed_menu == RE::ContainerMenu::MENU_NAME) {
                if (containermenu_owner) {
                    SKSE::GetTaskInterface()->AddUITask(
                        [this] {
                            if (containermenu_owner && IsChest(containermenu_owner->GetFormID())) {
                                if (!ActivateChest(containermenu_owner.get())) {
                                    logger::error("ActivateChest failed for containermenu_owner: {:x}",
                                                  containermenu_owner->GetFormID());
                                }
                            } else if (containermenu_owner) {
                                RE::ContainerMenu::OpenMenu(containermenu_owner.get(),
                                                            RE::ContainerMenu::ContainerMode::kLoot);
                            }
                            containermenu_owner.reset();
                        });
                } else logger::error("containermenu_owner is null in OnChestExit");
            } else {
                Menu::OpenMenu(closed_menu);
            }
            closed_menu = "";
        }
    }
    if (!next_menu_is_chest) {
        if (inventory_container) manager->PlayCloseAnimation(chest);
        else WorldStorage::GetSingleton()->PlayCloseAnimation(chest);
    }
}

void MainManager::TransferOnUse(const RefID a_chestID) {
    const auto manager = Manager::GetSingleton();
    if (manager->IsTokenMode(a_chestID)) return;
    if (const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(a_chestID)) {
        const auto player_ref = RE::PlayerCharacter::GetSingleton();
        const auto a_inv = chest->GetInventoryCounts();
        const auto a_realID = manager->GetRealID(a_chestID);
        std::vector<std::pair<FormID, Count>> a_cache;
        for (const auto& [item, count] : a_inv) {
            auto a_formid = item->GetFormID();
            auto a_count = count;
            if (manager->IsFakeContainer(a_formid)) {
                TransferOnUse(GetChest(a_formid));
                continue;
            }
            if (a_realID == a_formid) {
                a_count -= 1;
            }
            if (a_count > 0) {
                a_cache.emplace_back(a_formid, a_count);
                chest->RemoveItem(item, a_count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, player_ref);
            }
        }
        transfer_cache[a_chestID] = std::move(a_cache);
    }
}

void MainManager::HandleCraftingEnter(RE::TESObjectREFR* furniture) {
    const auto world_storage = WorldStorage::GetSingleton();
    const auto source = world_storage->GetSource(furniture->GetBaseObject()->GetFormID());
    if (source && source->transfer_on_use && !source->token_mode) {
        if (const auto chest = world_storage->GetChest(furniture->GetFormID())) TransferOnUse(chest);
    }
}

void MainManager::HandleCraftingExit() {
    const auto player_ref = RE::PlayerCharacter::GetSingleton();
    for (auto& [a_chestID,a_cache] : transfer_cache) {
        if (!a_cache.empty()) {
            if (const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(a_chestID)) {
                for (const auto& [item_formid, count] : a_cache) {
                    if (const auto item = RE::TESForm::LookupByID<RE::TESBoundObject>(item_formid)) {
                        player_ref->RemoveItem(item, count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, chest);
                    }
                }
            }
        }
    }
    transfer_cache.clear();

    Manager::GetSingleton()->UpdateExtrasAfterCrafting();
}

bool MainManager::IsInUse(const RefID chest) const {
    if (const auto loan = transfer_cache.find(chest); loan != transfer_cache.end() && !loan->second.empty()) return true;
    return QuickLootIntegration::IsInUse(chest) || open_chest == chest || queued_chests.contains(chest) ||
        (containermenu_owner && containermenu_owner->GetFormID() == chest);
}

void MainManager::ClearOpenChest(const RefID chest) {
    if (open_chest == chest) open_chest = 0;
}

void MainManager::ForgetChest(const RefID chest) {
    if (QuickLootIntegration::IsInUse(chest)) QuickLootIntegration::Close();
    transfer_cache.erase(chest);
    ClearOpenChest(chest);
    queued_chests.erase(chest);
    if (containermenu_owner && containermenu_owner->GetFormID() == chest) {
        containermenu_owner.reset();
        closed_menu.clear();
    }
}

void MainManager::HandleFormDelete(const RefID target) {
    if (QuickLootIntegration::IsInUse(target) || QuickLootIntegration::IsInUse(GetChest(target))) {
        QuickLootIntegration::Close();
    }
    Manager::GetSingleton()->HandleFormDelete(target);
    WorldStorage::GetSingleton()->HandleFormDelete(target);
}

void MainManager::Reset() {
    QuickLootIntegration::Reset();
    transfer_cache.clear();
    queued_chests.clear();
    open_chest = 0;
    closed_menu.clear();
    containermenu_owner.reset();
}

std::unordered_map<FormID, Source> MainManager::GetSources() const {
    auto sources = Manager::GetSingleton()->GetSources();
    sources.merge(WorldStorage::GetSingleton()->GetSources());
    return sources;
}

std::size_t MainManager::GetChests(const FormID baseForm, std::span<RefID> chests) const {
    const auto manager = Manager::GetSingleton();
    if (manager->isUninstalled.load()) return 0;
    const auto world_storage = WorldStorage::GetSingleton();
    return world_storage->GetSource(baseForm) ? world_storage->GetChests(baseForm, chests)
                                              : manager->GetChests(baseForm, chests);
}

RefID MainManager::GetChest(const FormID target) const {
    if (const auto chest = WorldStorage::GetSingleton()->GetChest(target)) return chest;
    return Manager::GetSingleton()->GetChest(target);
}

FormID MainManager::GetTarget(const RefID chest) const {
    const auto manager = Manager::GetSingleton();
    if (manager->isUninstalled.load()) return 0;
    if (const auto fake = manager->GetFakeID(chest)) return fake;
    return WorldStorage::GetSingleton()->GetTarget(chest);
}

bool MainManager::Refresh(const RefID chest) {
    const auto world_storage = WorldStorage::GetSingleton();
    const bool refreshed = world_storage->GetTarget(chest)
                               ? world_storage->Refresh(chest)
                               : Manager::GetSingleton()->Refresh(chest);
    if (!refreshed) return false;
    if (!IsInUse(chest)) {
        if (const auto target = RE::CrosshairPickData::GetSingleton()->GetActiveTarget().get();
            target && GetChest(target->GetFormID()) == chest)
            EventSink::SendPrompts(target.get());
    }
    return true;
}

const Source* MainManager::GetSource(const RE::TESObjectREFR* target) const {
    const auto base = target->GetBaseObject();
    if (std::ranges::contains(WorldStorage::form_types, base->GetFormType())) {
        return WorldStorage::GetSingleton()->GetSource(base->GetFormID());
    }
    const auto* manager = Manager::GetSingleton();
    return manager->GetContainerSource(base->GetFormID());
}

std::string MainManager::GetWeightText(RE::TESObjectREFR* target) const {
    if (std::ranges::contains(WorldStorage::form_types, target->GetBaseObject()->GetFormType())) {
        return WorldStorage::GetSingleton()->GetWeightText(target);
    }
    return Manager::GetSingleton()->GetWeightText(target);
}

std::string MainManager::GetValueText(RE::TESObjectREFR* target) const {
    if (std::ranges::contains(WorldStorage::form_types, target->GetBaseObject()->GetFormType())) return "";
    return Manager::GetSingleton()->GetValueText(target);
}

bool MainManager::IsTokenMode(const RE::TESObjectREFR* target) const {
    const auto base = target->GetBaseObject();
    if (std::ranges::contains(WorldStorage::form_types, base->GetFormType())) {
        const auto source = WorldStorage::GetSingleton()->GetSource(base->GetFormID());
        return source && source->token_mode;
    }
    return Manager::GetSingleton()->IsTokenMode(base->GetFormID());
}

bool MainManager::CanRename() const {
    return ModCompatibility::Mods::ui_extensions_installed;
}

bool MainManager::Rename(const FormID target, const char* name) {
    if (Manager::GetSingleton()->isUninstalled.load() || !name || !*name) return false;
    const auto reference = RE::TESForm::LookupByID<RE::TESObjectREFR>(target);
    if (reference && std::ranges::contains(WorldStorage::form_types, reference->GetBaseObject()->GetFormType())) {
        return WorldStorage::GetSingleton()->Rename(reference, name);
    }
    return Manager::GetSingleton()->Rename(target, name);
}

FormID MainManager::Initialize(const FormID target, const RefID owner, RE::ExtraDataList* instance) {
    const auto reference = owner ? nullptr : RE::TESForm::LookupByID<RE::TESObjectREFR>(target);
    const auto base = reference ? reference->GetBaseObject() : nullptr;
    if (base && std::ranges::contains(WorldStorage::form_types, base->GetFormType())) {
        return WorldStorage::GetSingleton()->Initialize(reference, instance);
    }
    return Manager::GetSingleton()->Initialize(target, owner, instance);
}

bool MainManager::DeRegister(const FormID target) {
    const auto world_storage = WorldStorage::GetSingleton();
    if (world_storage->GetChest(target)) return world_storage->DeRegister(target);
    return Manager::GetSingleton()->DeRegister(target);
}

Count MainManager::CanBeAdded(const RE::TESBoundObject* item, const Count count, RE::TESObjectREFR* chest) {
    const auto chest_id = chest->GetFormID();
    if (chest->GetBaseObject() != UnownedStuff::unownedChest) return count;
    if (bypass_CanBeAdded.contains({chest_id, item->GetFormID()})) return count;
    const auto manager = Manager::GetSingleton();
    if (manager->IsChest(chest_id)) return manager->CanBeAdded(item, count, chest);
    return WorldStorage::GetSingleton()->CanBeAdded(item, count, chest);
}

void MainManager::SetCapacityBypass(const std::pair<RefID, FormID>& target, const bool enabled) {
    if (enabled) bypass_CanBeAdded.insert(target);
    else bypass_CanBeAdded.erase(target);
}

RE::TESObjectREFR* MainManager::GetContainerChest(const RE::TESObjectREFR* target) const {
    if (std::ranges::contains(WorldStorage::form_types, target->GetBaseObject()->GetFormType())) {
        return RE::TESForm::LookupByID<RE::TESObjectREFR>(WorldStorage::GetSingleton()->GetChest(target->GetFormID()));
    }
    return Manager::GetSingleton()->GetContainerChest(target);
}

bool MainManager::IsChest(const RefID chest) const {
    return Manager::GetSingleton()->IsChest(chest) || WorldStorage::GetSingleton()->GetTarget(chest);
}

bool MainManager::IsContainer(const RE::TESObjectREFR* target) const {
    if (std::ranges::contains(WorldStorage::form_types, target->GetBaseObject()->GetFormType())) {
        return !target->IsDisabled() && !target->IsDeleted() &&
            WorldStorage::GetSingleton()->GetSource(target->GetBaseObject()->GetFormID());
    }
    return Manager::GetSingleton()->IsRealContainer(target);
}

void MainManager::RenameCallback(RE::TESForm* target) {
    if (!GetSingleton()->CanRename()) return;
    const auto reference = target->As<RE::TESObjectREFR>();
    const char* container_name = reference ? reference->GetDisplayFullName() : target->GetName();
    if (Papyrus::CallFunction("UIExtensions", "SetMenuPropertyString", {}, "UITextEntryMenu", "text", container_name)) {
        RE::TESForm* dummy1 = nullptr;
        RE::TESForm* dummy2 = nullptr;
        const auto smart = reference
            ? RE::make_smart<Papyrus::RenameCallbackFunctor>(reference->GetHandle())
            : RE::make_smart<Papyrus::RenameCallbackFunctor>(target->As<RE::TESBoundObject>());
        if (!Papyrus::CallFunction("UIExtensions", "OpenMenu", smart.get(), "UITextEntryMenu", dummy1, dummy2)) {
            logger::error("Failed to open UIExtensions menu.");
        }
    } else {
        logger::error("Failed to call UIExtensions functions.");
    }
}
