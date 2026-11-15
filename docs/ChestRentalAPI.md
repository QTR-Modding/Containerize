# Containerize API

## ChestManager: chest rentals

Rent a chest when your SKSE plugin needs an inventory of its own. Include Containerize's `include/API.h` and call the API on the game thread after a successful `kPostLoadGame` or `kNewGame`. This example rents an empty chest and returns it.

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

Replace `MyMod.Storage` with a unique, case-sensitive name for your mod. `RegisterClient` gives that name a client ID. `RentChest` returns the reference FormID of a hidden chest. Resolve that ID to a chest reference and use Skyrim's inventory operations to store or take items.

### Registration and saves

Register again after each load or New Game. Keep the same name across updates. The same name gets the same client ID in a save. Ask again after loading, since a different save can assign a different ID.

Containerize saves the registered names and rented chests. To keep using a chest across loads, store its reference FormID and resolve it through SKSE's serialization interface when loading. Rent again only when you need a new chest.

Registration returns zero for an empty or null name. Containerize reserves client ID zero for its own use. A rental can fail and return zero, for example when chest initialization has yet to finish.

### Returning storage

Move anything you want to keep out of the chest first. `ReturnChest(client, chest)` accepts chests rented by that client. Once it returns true, stop using the chest reference. Any remaining contents are discarded. Cleanup can finish after the call returns and resumes after loading a save.

### Client uninstall

`Uninstall(client)` returns every chest rented by that client. Move out the items you want to keep and stop using those chests first. A true result means every return was accepted. If a return fails, the call returns false and the chests already returned stay returned. The client name stays registered and can rent again later.

## Containerize: linked item/furniture storage

For storage that players open through an item or furniture, use the [linked storage guide](LinkedStorageAPI.md).

### Finding linked storage

Use [GetChest](LinkedStorageAPI.md#finding-linked-storage) to find existing storage for an item or placed object.

### Refreshing Containerize-managed storage

Use [Refresh](LinkedStorageAPI.md#refreshing-containerize-managed-storage) after changing the contents.

## Interface availability

`RequestAPI()` can retrieve the interface as soon as Containerize is loaded. A null result means Containerize or the requested interface is unavailable. Consumers need the Windows and CommonLib headers.
