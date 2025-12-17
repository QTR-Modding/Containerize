#include "SkyPrompt.h"
#include "Animations.h"
#include "Events.h"
#include "Hooks.h"
#include "Manager.h"

using namespace SkyPrompt;

void MyPromptSink::ProcessEvent(const SkyPromptAPI::PromptEvent event) const {
    if (event.type) {
        return;
    }
    EventSink::RemovePrompts();

    if (const auto crosshairref = RE::CrosshairPickData::GetSingleton()->target) {
        if (const auto a_ref = crosshairref->get().get()) {
            if (const auto prompt_eventid = event.prompt.eventID;
                prompt_eventid == 0) {
                const auto duration = Animations::SetUpPlayAnimation(a_ref, true);
                Manager::GetSingleton()->OnActivateContainer(a_ref, 0, duration);
            } else if (prompt_eventid == 1) {
                Manager::GetSingleton()->OnActivateContainer(a_ref, 1);
            }
        }
    }
}

void MyPromptSink2::Start(RE::TESObjectREFR* a_ref) {
    weight_text.clear();
    value_text.clear();

    const auto manager = Manager::GetSingleton();
    const auto a_weight_text = manager->GetWeightText(a_ref);

    const auto a_value_text = manager->GetValueText(a_ref);
    if (a_weight_text.empty() || a_value_text.empty()) {
        return;
    }

    weight_text.append(Strings::weight).append(a_weight_text);
    value_text.append(Strings::value).append(a_value_text);

    weight_prompt.text = weight_text;
    value_prompt.text = value_text;

    prompts = {weight_prompt, value_prompt};
}

bool SkyPrompt::IsAnyMenuOpen() {
    const auto ui = RE::UI::GetSingleton();
    for (const auto a_name : blockedMenus) {
        if (ui->IsMenuOpen(a_name)) {
            return true;
        }
    }
    return false;
}

void MenuPromptSink::ProcessEvent(const SkyPromptAPI::PromptEvent event) const {
    #undef GetObject
    if (event.type) {
        return;
    }
    EventSink::RemoveMenuPrompts();
    if (const auto a_entry = Hooks::GetSelectedEntryInMenu()) {
        if (const auto a_fake = a_entry->GetObject();
            a_fake && Manager::GetSingleton()->IsFakeContainer(a_fake->GetFormID())) {
            if (event.prompt.eventID == 1) {
                Manager::RenameCallback(a_fake);
                return;
            }
            OpenBag(a_fake, RE::UI::GetSingleton()->IsMenuOpen(RE::InventoryMenu::MENU_NAME) && a_entry->IsWorn());
        }
    }
}

void MenuPromptSink::Show(const RE::TESBoundObject* a_fake) const {
    weight_text.clear();

    if (const auto a_ref = RE::Inventory3DManager::GetSingleton()->tempRef) {
        const auto refid = a_ref->GetFormID();
        open_prompt.refid = refid;
        rename_prompt.refid = refid;
        weight_prompt.refid = refid;

        const auto a_weight_text = Manager::GetSingleton()->GetWeightText(a_fake);
        weight_text.append(Strings::weight).append(a_weight_text);
        weight_prompt.text = weight_text;
    } else {
        open_prompt.refid = 0;
        rename_prompt.refid = 0;
        weight_prompt.refid = 0;
    }
    prompts = {open_prompt, rename_prompt, weight_prompt};

    if (!SkyPromptAPI::SendPrompt(this, g_clientID)) {
    }
}

void MenuPromptSink::Hide() const {
    SkyPromptAPI::RemovePrompt(this, g_clientID);
}

void MenuPromptSink::OpenBag(RE::TESBoundObject* a_fake, const bool is_worn) {
    using namespace ModCompatibility::Mods;

    const auto manager = Manager::GetSingleton();

    const auto a_real = manager->FakeToRealContainer(a_fake->GetFormID());

    if (is_worn) {
        Hooks::OnIsWorn(a_fake);
    }

    manager->CloseMenu();

    Manager::GetSingleton()->OnLongPressEquip(a_fake, Animations::SetUpPlayAnimation(a_real, is_worn));
}

void RegistrationPromptSink::ProcessEvent(const SkyPromptAPI::PromptEvent event) const {
    if (event.type) {
        return;
    }

    EventSink::RemoveMenuPrompts();

    if (const auto a_entry = Hooks::GetSelectedEntryInMenu()) {
        const bool is_in_inventory_menu = RE::UI::GetSingleton()->IsMenuOpen(RE::InventoryMenu::MENU_NAME);
        const bool is_worn = is_in_inventory_menu && a_entry->IsWorn();
        const auto real_id = a_entry->GetObject()->GetFormID();
        const auto owner_handle = Menu::GetOwnerInContainerMenu(real_id);
        RE::TESObjectREFRPtr a_owner;
        if (is_in_inventory_menu || !RE::LookupReferenceByHandle(owner_handle, a_owner)) {
            a_owner.reset();
        }
        if (const auto a_fake = Manager::GetSingleton()->RegisterFromMenu(a_entry, a_owner.get());
            a_fake && Manager::GetSingleton()->IsFakeContainer(a_fake->GetFormID())) {
            if (is_worn) {
                RE::ActorEquipManager::GetSingleton()->EquipObject(RE::PlayerCharacter::GetSingleton(), a_fake);
            }

            if (const auto ui = RE::UI::GetSingleton();
                ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) || ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
                RE::SendUIMessage::SendInventoryUpdateMessage(RE::PlayerCharacter::GetSingleton(), nullptr);
                if (a_owner) {
                    RE::SendUIMessage::SendInventoryUpdateMessage(a_owner.get(), nullptr);
                }
            }

            if (event.prompt.eventID == 4) {
                Manager::RenameCallback(a_fake);
                return;
            }
            MenuPromptSink::OpenBag(a_fake, is_worn);
        }
    }
}

void RegistrationPromptSink::Show(const RE::TESBoundObject* a_item) const {
    weight_text.clear();

    if (const auto a_ref = RE::Inventory3DManager::GetSingleton()->tempRef) {
        const auto refid = a_ref->GetFormID();
        open_prompt.refid = refid;
        rename_prompt.refid = refid;
        weight_prompt.refid = refid;

        const auto a_weight_text = Manager::GetSingleton()->GetWeightText(a_item);
        weight_text.append(Strings::weight).append(a_weight_text);
        weight_prompt.text = weight_text;
    } else {
        open_prompt.refid = 0;
        rename_prompt.refid = 0;
        weight_prompt.refid = 0;
    }

    prompts = {open_prompt, rename_prompt, weight_prompt};

    if (!SkyPromptAPI::SendPrompt(this, g_clientID)) {
    }
}

void RegistrationPromptSink::Hide() const {
    SkyPromptAPI::RemovePrompt(this, g_clientID);
    containermenu_owner.reset();
}