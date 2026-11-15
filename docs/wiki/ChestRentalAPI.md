# Containerize API

## ChestManager: chest rentals

Rent a chest when your SKSE plugin needs an inventory of its own. Include Containerize's [`API.h`](https://github.com/QTR-Modding/Containerize/blob/main/include/API.h). Run the example on the game thread once SKSE reports a successful game load (`kPostLoadGame`) or a new game (`kNewGame`). It rents an empty chest and returns it.

```cpp
#include "API.h"

bool TryChestRental() {
    const auto api = ContainerizeAPI::RequestAPI();
    if (!api) {
        return false;
    }

    const auto client = api->RegisterClient("MyMod.Storage");
    if (client == ContainerizeAPI::invalid_client) {
        return false;
    }

    const auto chest = api->RentChest(client);
    if (!chest) {
        return false;
    }

    return api->ReturnChest(client, chest);
}
```

Replace `MyMod.Storage` with a unique, case-sensitive name for your mod. `RegisterClient` returns the number Containerize uses to track your rentals. `RentChest` returns the FormID of the chest. To put items in it, use `RE::TESForm::LookupByID<RE::TESObjectREFR>(chest)` to get the chest reference and pass it to Skyrim's inventory functions before returning the chest.

### Registration and saves

Register the same name after each load or new game. Keep the name unchanged across mod updates. The same save retains its client ID, and a different save can assign a different one.

Containerize saves the registered names and rented chests. To keep using a chest across loads, store its reference FormID and resolve it through SKSE's serialization interface when loading. Rent again only when you need a new chest.

Registration returns zero for an empty or null name. Containerize reserves client ID zero for its own use. A rental can fail and return zero, for example when chest initialization has yet to finish.

### Returning storage

Move anything you want to keep out of the chest first. `ReturnChest(client, chest)` accepts chests rented by that client. Once it returns true, stop using the chest reference. Any remaining contents are discarded. Cleanup can finish after the call returns and resumes after loading a save.

### Client uninstall

`Uninstall(client)` returns every chest rented by that client. Move out the items you want to keep and stop using those chests first. A true result means every return was accepted. If a return fails, the call returns false and the chests already returned stay returned. The client name stays registered and can rent again later.

## Containerize: linked item/furniture storage

For storage that players open through an item or furniture, use the [linked storage guide](https://github.com/QTR-Modding/Containerize/wiki/LinkedStorageAPI).

### Finding linked storage

Use [GetChest](https://github.com/QTR-Modding/Containerize/wiki/LinkedStorageAPI#finding-linked-storage) to find existing storage for an item or placed object.

### Refreshing Containerize-managed storage

Use [Refresh](https://github.com/QTR-Modding/Containerize/wiki/LinkedStorageAPI#refreshing-containerize-managed-storage) after changing the contents.

## Interface availability

`RequestAPI()` can retrieve the interface as soon as Containerize is loaded. A null result means Containerize or the requested interface is unavailable. Consumers need the Windows and CommonLib headers.
