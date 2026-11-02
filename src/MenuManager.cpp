#include "MenuManager.h"
#include "Manager.h"
#include "Animations.h"
#include "CLibUtilsQTR/Tasker.hpp"

bool MenuManager::ActivateChest(RE::TESObjectREFR* chest) const {
    if (!chest) {
        logger::error("ActivateChest: Chest reference is null.");
        return false;
    }
    return Menu::ActivateChest(chest, Manager::GetSingleton()->GetChestName(chest));
}

void MenuManager::OpenChestFromMenu(RE::TESObjectREFR* a_chest) {
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

void MenuManager::Gateway(const int result, const RE::ObjectRefHandle& a_current_container) {
    if (result != 0 && result != 1) {
        logger::error("Unexpected callback integer!");
        return;
    }
    const auto a_chest_handle = a_current_container.get();
    const auto a_chest = a_chest_handle ? Manager::GetSingleton()->GetContainerChest(a_chest_handle.get()) : nullptr;
    if (!a_chest) {
        logger::warn("Current container is null.");
    } else if (result) {
        Manager::RenameCallback(Manager::GetSingleton()->GetFakeBound(a_chest_handle.get()));
    } else if (!ActivateChest(a_chest)) {
        Manager::GetSingleton()->OnOpenFailed();
        open_chest = 0;
        queued_chests.clear();
        Animations::SendAnimEvent(3, nullptr);
        logger::warn("Chest not found.");
    }
}

void MenuManager::OnLongPressEquip(const RE::TESBoundObject* a_fake, const int delay) {
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

void MenuManager::OnActivateContainer(RE::TESObjectREFR* a_container, const int msgbox_action, const int a_delay) {
    if (Manager::GetSingleton()->isUninstalled.load()) return;
    if (!Manager::GetSingleton()->HandleRegistration(a_container)) return;
    if (msgbox_action == 0) {
        if (const auto chest = Manager::GetSingleton()->GetContainerChest(a_container)) queued_chests.insert(chest->GetFormID());
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

void MenuManager::CloseMenu() {
    containermenu_owner.reset();
    if (!Menu::GetContainerMenuOwner(containermenu_owner)) {
        containermenu_owner.reset();
    }
    closed_menu = Menu::CloseMenu();
}

void MenuManager::OnChestEnter(RE::TESObjectREFR* chest) {
    const auto chest_id = chest->GetFormID();
    queued_chests.erase(chest_id);
    open_chest = chest_id;
    if (!Manager::GetSingleton()->OnChestEnter(chest)) open_chest = 0;
}

void MenuManager::OnChestExit(RE::TESObjectREFR* chest) {
    const auto chest_id = chest->GetFormID();
    const auto manager = Manager::GetSingleton();
    const bool was_open = open_chest == chest_id;
    const bool next_menu_is_chest = containermenu_owner && manager->IsChest(containermenu_owner->GetFormID());
    if (!manager->OnChestExit(chest)) return;
    if (was_open) {
        open_chest = 0;
        if (Settings::other_settings.at(Settings::otherstuffKeys[2]) && queued_chests.empty()) {
            if (closed_menu == RE::ContainerMenu::MENU_NAME) {
                if (containermenu_owner) {
                    SKSE::GetTaskInterface()->AddUITask(
                        [this] {
                            if (containermenu_owner && Manager::GetSingleton()->IsChest(containermenu_owner->GetFormID())) {
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
    if (!next_menu_is_chest) manager->PlayCloseAnimation(chest);
}

bool MenuManager::IsInUse(const RefID chest) const {
    return open_chest == chest || queued_chests.contains(chest) ||
        (containermenu_owner && containermenu_owner->GetFormID() == chest);
}

void MenuManager::ClearOpenChest(const RefID chest) {
    if (open_chest == chest) open_chest = 0;
}

void MenuManager::Reset() {
    queued_chests.clear();
    open_chest = 0;
    closed_menu.clear();
    containermenu_owner.reset();
}
