#include "MainManager.h"
#include "Manager.h"
#include "WorldStorage.h"
#include "Animations.h"
#include "CLibUtilsQTR/Tasker.hpp"

bool MainManager::ActivateChest(RE::TESObjectREFR* chest) const {
    if (!chest) {
        logger::error("ActivateChest: Chest reference is null.");
        return false;
    }
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

void MainManager::Gateway(const int result, const RE::ObjectRefHandle& a_current_container) {
    if (result != 0 && result != 1) {
        logger::error("Unexpected callback integer!");
        return;
    }
    const auto a_chest_handle = a_current_container.get();
    const auto a_chest = a_chest_handle ? GetContainerChest(a_chest_handle.get()) : nullptr;
    if (!a_chest) {
        logger::warn("Current container is null.");
    } else if (result) {
        Manager::RenameCallback(Manager::GetSingleton()->GetFakeBound(a_chest_handle.get()));
    } else if (!ActivateChest(a_chest)) {
        if (!WorldStorage::GetSingleton()->GetTarget(a_chest->GetFormID())) Manager::GetSingleton()->OnOpenFailed();
        open_chest = 0;
        queued_chests.clear();
        Animations::SendAnimEvent(3, nullptr);
        logger::warn("Chest not found.");
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

void MainManager::OnActivateContainer(RE::TESObjectREFR* a_container, const int msgbox_action, const int a_delay) {
    if (Manager::GetSingleton()->isUninstalled.load()) return;
    const auto base = a_container->GetBaseObject();
    if (std::ranges::contains(WorldStorage::form_types, base->GetFormType())) {
        const auto world_storage = WorldStorage::GetSingleton();
        const auto source = world_storage->GetSource(base->GetFormID());
        if (msgbox_action != 0 || !source || source->token_mode || !world_storage->Initialize(a_container, nullptr)) return;
    } else if (!Manager::GetSingleton()->HandleRegistration(a_container)) {
        return;
    }
    if (msgbox_action == 0) {
        if (const auto chest = GetContainerChest(a_container)) queued_chests.insert(chest->GetFormID());
    }
    auto a_handle = a_container->GetHandle();
    if (a_delay > 0) {
        clib_utilsQTR::Tasker::GetSingleton()->PushTask([this,msgbox_action,a_handle] {
            Gateway(msgbox_action, a_handle);
        }, a_delay);
    } else {
        return Gateway(msgbox_action, a_handle);
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

bool MainManager::IsInUse(const RefID chest) const {
    return open_chest == chest || queued_chests.contains(chest) ||
        (containermenu_owner && containermenu_owner->GetFormID() == chest);
}

void MainManager::ClearOpenChest(const RefID chest) {
    if (open_chest == chest) open_chest = 0;
}

void MainManager::ForgetChest(const RefID chest) {
    ClearOpenChest(chest);
    queued_chests.erase(chest);
    if (containermenu_owner && containermenu_owner->GetFormID() == chest) {
        containermenu_owner.reset();
        closed_menu.clear();
    }
}

void MainManager::Reset() {
    queued_chests.clear();
    open_chest = 0;
    closed_menu.clear();
    containermenu_owner.reset();
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
