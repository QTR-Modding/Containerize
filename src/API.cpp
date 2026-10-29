#include "API.h"
#include "Chest.h"
#include "Manager.h"

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
        [](RE::FormID target) { return Manager::GetSingleton()->GetChest(target); }
    };
    return &api;
}
