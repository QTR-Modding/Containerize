# Containerize API

One interface exposes ChestManager's chest rentals and Containerize's storage linked to items or furniture.

## ChestManager: chest rentals

Include `API.h` from Containerize's `include` directory. This example rents an empty chest, then returns it:

```cpp
#include "API.h"

bool UseStorage() {
    const auto api = ContainerizeAPI::RequestAPI();
    if (!api) return false;

    const auto client = api->RegisterClient("MyMod.Storage");
    if (client == ContainerizeAPI::invalid_client) return false;

    const auto chest = api->RentChest(client);
    if (!chest) return false;

    // Use the chest through normal engine inventory operations.
    // Move out everything you want to keep before returning it.
    return api->ReturnChest(client, chest);
}
```

`chest` is the reference FormID of a hidden storage chest. Renting does not create an item or link storage to furniture. Your mod owns those connections.

### Registration and saves

You can obtain the interface before loading a game; `RentChest` returns zero until chest initialization succeeds.

Register again after each game load or New Game. Names are case-sensitive and must be unique to your mod and remain unchanged across updates. The same name receives the same ClientID within that save. Different saves can assign different IDs, so do not retain a ClientID across loads or store it in your own save data.

ChestManager saves the registered names and their rentals. If your mod saves rented chest FormIDs, resolve them through SKSE's serialization interface when loading. Registering a client does not rent a chest; do not rent replacements merely because a game loaded.

ClientID zero is reserved for Containerize itself. Registration returns zero for a null or empty name, or when no more IDs are available. The public rental, return and uninstall calls do not accept client zero.

### Returning storage

`ReturnChest(client, chest)` returns true when the chest belongs to that client and its return is accepted. Invalid references and ownership mismatches return false without ending the rental.

After a successful return, stop using that chest reference. Empty chests can be rented again immediately. Chests with contents are withheld from new rentals while ChestManager removes the contents; a chest that cannot be emptied is submitted to the game's garbage collector. Cleanup can finish after the API call returns and resumes after loading a save.

Move anything you want to preserve elsewhere before returning the chest. Remaining contents are discarded, not transferred to the player.

### Client uninstall

```cpp
const bool accepted = api->Uninstall(client);
```

This ends that client's rentals through the same return and cleanup path. Other clients' rentals are untouched. True means all returns were accepted, not that asynchronous cleanup has finished. If any return fails, the call returns false; successfully returned chests remain returned.

Your mod must first finish its own item transfers and stop using the chests. Containerize does not call your code or clear your mod's state. The registered name is retained, so registering it again keeps the same ClientID in that save. Uninstall does not disable future rentals.

## Containerize: linked item/furniture storage

See [Containerize-linked storage](LinkedStorageAPI.md) for initialization, lookup, renaming, refresh and deregistration. These calls operate on storage owned by Containerize, rather than rentals owned by your mod.

### Finding linked storage

[GetChest](LinkedStorageAPI.md#finding-linked-storage) retrieves an existing linked chest without initializing storage.

### Refreshing Containerize-managed storage

[Refresh](LinkedStorageAPI.md#refreshing-containerize-managed-storage) updates the linked item's weight/value after external contents changes.

## Interface availability

Call the API on the game thread after a successful `kPostLoadGame` or `kNewGame` message.

`RequestAPI()` returns null when Containerize is unavailable or the interface version is unsupported. The interface contains the function pointers shown above and remains valid while Containerize is loaded. Consumers need Windows and CommonLibVR-MIT headers; no Containerize implementation headers or import library are required.
