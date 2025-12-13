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
        if (const auto a_chestID = a_container->GetFormID(); M->IsChest(a_chestID)) {
            if (const auto a_loc = M->GetContainerLocation(a_chestID); a_loc > 0) {
                if (!GetSingleton()->SendPrompt(a_loc)) {
                    logger::error("QuickLootHelper: Prompt sending failed.");
                }
            } else {
                logger::error("QuickLootHelper: Container location invalid.");
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
                    QuickLoot::API::QuickLootAPI::DisableLootMenu();
                    break;
                default:
                    GetSingleton()->SetLastOpenState(kOpen);
                    QuickLoot::API::QuickLootAPI::EnableLootMenu();
                    break;
            }
            break;
        default:
            break;
    }
}

bool QuickLootHelper::SendPrompt(const RefID refid) const {
    logger::info("QuickLootHelper::SendPrompt called with refid: {:x}", refid);
    EventSink::RemovePrompts();
    RemovePrompt();
    ql_prompt.refid = refid;
    return SkyPromptAPI::SendPrompt(this,SkyPrompt::g_clientID);
}

void QuickLootHelper::RemovePrompt() const {
    SkyPromptAPI::RemovePrompt(this, SkyPrompt::g_clientID);
}