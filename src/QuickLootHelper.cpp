#include "QuickLootHelper.h"
#include "Events.h"
#include "Manager.h"

void QuickLootHelper::containerOverrideHandler(QuickLoot::API::ContainerOverrideEvent* e) {
    if (const auto a_chest = Manager::GetSingleton()->QL_GetChest(e->reference)) {
        e->overrideContainer = a_chest;
    }
}

void QuickLootHelper::openingLootMenuHandler(QuickLoot::API::OpeningLootMenuEvent* e) {
    if (const auto a_container = e->container) {
        const auto M = Manager::GetSingleton();
        if (M->IsChest(a_container->GetFormID())) {
            if (const auto crosshairref = RE::CrosshairPickData::GetSingleton()->target->get().get(); 
                crosshairref && !GetSingleton()->SendPrompt(crosshairref)) {
                logger::error("QuickLootHelper: Prompt sending failed.");
            }
            if (GetSingleton()->GetLastOpenState() == kClosed) {
                e->result = QuickLoot::API::HandleResult::kStop;
            } else {
                M->QL_Opening(a_container);
            }
        }
    }
}

void QuickLootHelper::closingLootMenuHandler(QuickLoot::API::CloseLootMenuEvent* e) {
    if (const auto a_container = e->container) {
        const auto M = Manager::GetSingleton();
        if (M->IsChest(a_container->GetFormID())) {
            M->QL_Close(a_container);
        }
    }
}

void QuickLootHelper::ProcessEvent(const SkyPromptAPI::PromptEvent event) const {
    switch (event.type) {
        case SkyPromptAPI::PromptEventType::kAccepted:
            switch (GetLastOpenState()) {
                case kOpen:
                    GetSingleton()->SetLastOpenState(kClosed);
                    ToggleQLMenu(true);
                    break;
                default:
                    GetSingleton()->SetLastOpenState(kOpen);
                    ToggleQLMenu(false);
                    break;
            }
            break;
        default:
            break;
    }
}

bool QuickLootHelper::SendPrompt(RE::TESObjectREFR* a_ref) const {
    EventSink::RemovePrompts();
    RemovePrompt();
    ql_prompt.refid = a_ref->GetFormID();
    prompts = {ql_prompt};
    const auto ps = SkyPrompt::MyPromptSink2::GetSingleton();
    ps->Start(a_ref);
    if (!SkyPromptAPI::SendPrompt(ps, SkyPrompt::g_clientID)) {
        // logger::error("Prompt failed.");
    }
    return SkyPromptAPI::SendPrompt(this,SkyPrompt::g_clientID);
}

void QuickLootHelper::ToggleQLMenu(const bool a_disabled) const { 
    if (a_disabled) {
        QuickLoot::API::QuickLootAPI::DisableLootMenu();
    } else {
        QuickLoot::API::QuickLootAPI::EnableLootMenu();
    }
    ql_menu_disabled = a_disabled;
}

RE::BSEventNotifyControl QuickLootHelper::ProcessEvent(const SKSE::CrosshairRefEvent* a_event,
                                                       RE::BSTEventSource<SKSE::CrosshairRefEvent>*) {
    if (!a_event->crosshairRef) {
        if (ql_menu_disabled) ToggleQLMenu(false);
        RemovePrompt();
    }
    return RE::BSEventNotifyControl::kContinue;
}

void QuickLootHelper::RemovePrompt() const {
    SkyPromptAPI::RemovePrompt(this, SkyPrompt::g_clientID);
}