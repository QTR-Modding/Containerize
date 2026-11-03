#include "QuickLoot.h"
#include "QuickLootAPI.h"
#include "MainManager.h"
#include "Manager.h"
#include "SkyPrompt.h"

namespace QuickLootIntegration {
    namespace {
        using API = QuickLoot::API::QuickLootAPI;

        struct StorageView {
            RE::ObjectRefHandle target;
            RE::ObjectRefHandle chest;
        };
        StorageView view;
        bool installed = false;

        void OnTake(QuickLoot::API::TakeItemEvent* event) {
            if (event->container != view.chest) return;
            const auto chest = event->container;
            SKSE::GetTaskInterface()->AddTask([chest] {
                if (view.chest == chest) {
                    if (const auto storage = chest.get()) (void)MainManager::GetSingleton()->Refresh(storage->GetFormID());
                }
            });
        }

        void OnInput(QuickLoot::API::InputActionEvent* event) {
            if (event->action != QuickLoot::API::QuickLootAction::kTransfer || event->container != view.chest) return;
            event->result = QuickLoot::API::HandleResult::kStop;
            const auto target = view.target;
            SKSE::GetTaskInterface()->AddTask([target] {
                if (view.target != target) return;
                Close();
                const auto reference = target.get();
                const auto manager = MainManager::GetSingleton();
                if (reference && manager->IsContainer(reference.get()) && !manager->IsTokenMode(reference.get())) {
                    manager->OnActivateContainer(reference.get(), 0);
                }
            });
        }
    }

    void Install() {
        installed = API::Init(SKSE::PluginDeclaration::GetSingleton()->GetName().data());
        if (!installed) return;
        API::RegisterTakeItemHandler(OnTake);
        API::RegisterInputActionHandler(OnInput);
        logger::info("QuickLoot integration enabled.");
    }

    void Close() {
        if (!view.chest) return;
        const auto chest = view.chest.get();
        view = {};
        if (chest && Manager::GetSingleton()->IsChest(chest->GetFormID())) {
            chest->extraList.RemoveByType(RE::ExtraDataType::kTextDisplayData);
            if (!Manager::GetSingleton()->OnChestExit(chest.get())) {
                logger::error("Failed to close QuickLoot storage {:08X}", chest->GetFormID());
            }
        }
        API::ClearForcedContainer();
    }

    void Reset() {
        view = {};
        if (installed) API::ClearForcedContainer();
    }

    bool IsInUse(const RefID chest) {
        const auto storage = view.chest.get();
        return storage && storage->GetFormID() == chest;
    }

    void Update(RE::TESObjectREFR* target) {
        if (!installed) return;
        const auto manager = MainManager::GetSingleton();
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (Manager::GetSingleton()->isUninstalled.load() || !target ||
            RE::UI::GetSingleton()->GameIsPaused() || SkyPrompt::IsAnyMenuOpen() || player->IsGrabbing() ||
            manager->IsInChestMenu() || manager->IsChestMenuQueued() ||
            !manager->IsContainer(target) || manager->IsTokenMode(target) ||
            target->IsLocked() || target->IsActivationBlocked()) {
            Close();
            return;
        }
        if (view.target == target->GetHandle() && view.chest.get()) return;
        Close();
        const auto target_id = target->GetFormID();
        if (!manager->Initialize(target_id, 0, nullptr)) return;
        const auto chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(manager->GetChest(target_id));
        if (!chest || manager->IsInUse(chest->GetFormID())) return;
        if (Manager::GetSingleton()->IsChest(chest->GetFormID()) &&
            !Manager::GetSingleton()->OnChestEnter(chest)) return;
        chest->SetDisplayName(target->GetDisplayFullName(), true);
        view = {target->GetHandle(), chest->GetHandle()};
        API::ForceCurrentContainer(view.chest);
    }
}
