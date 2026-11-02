#include "API.h"
#include "Chest.h"
#include "Manager.h"
#include "WorldStorage.h"

extern "C" __declspec(dllexport) const ContainerizeAPI::Interface* Containerize_RequestAPI(std::uint32_t version) {
    if (version != ContainerizeAPI::interface_version) return nullptr;
    static const ContainerizeAPI::Interface api{
        [](const char* name) { return ChestManager::GetSingleton()->RegisterClient(name); },
        [](ContainerizeAPI::ClientID client) -> RE::FormID {
            if (client == ContainerizeAPI::containerize_client) return 0;
            const auto chest = ChestManager::GetSingleton()->RentChest(client);
            return chest ? chest->GetFormID() : 0;
        },
        [](ContainerizeAPI::ClientID client, RE::FormID chest) {
            return client != ContainerizeAPI::containerize_client &&
                   ChestManager::GetSingleton()->ReturnChest(client, RE::TESForm::LookupByID<RE::TESObjectREFR>(chest));
        },
        [](ContainerizeAPI::ClientID client) { return ChestManager::GetSingleton()->Uninstall(client); },
        [](RE::FormID chest) { return Manager::GetSingleton()->Refresh(chest); },
        [](RE::FormID target) {
            if (const auto chest = WorldStorage::GetSingleton()->GetChest(target)) return chest;
            return Manager::GetSingleton()->GetChest(target);
        },
        [](RE::FormID target, RE::FormID owner, RE::ExtraDataList* instance) -> RE::FormID {
            const auto reference = owner ? nullptr : RE::TESForm::LookupByID<RE::TESObjectREFR>(target);
            const auto base = reference ? reference->GetBaseObject() : nullptr;
            if (base && std::ranges::contains(WorldStorage::form_types, base->GetFormType())) {
                return WorldStorage::GetSingleton()->Initialize(reference, instance);
            }
            return Manager::GetSingleton()->Initialize(target, owner, instance);
        },
        [](RE::FormID fakeItem, const char* name) { return Manager::GetSingleton()->Rename(fakeItem, name); },
        [](RE::FormID target) {
            const auto world_storage = WorldStorage::GetSingleton();
            if (world_storage->GetChest(target)) return world_storage->DeRegister(target);
            return Manager::GetSingleton()->DeRegister(target);
        }
    };
    return &api;
}
