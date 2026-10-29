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

GetChest and Refresh operate on Containerize's existing linked storage. Your mod does not rent that storage or take ownership of it.

### Finding linked storage

```cpp
const RE::FormID chest = api->GetChest(target);
if (!chest) return;  // No existing linked storage.
```

Pass a placed item's or furniture's reference FormID, or the fake item FormID of an already containerized item in an inventory. The fake item is the copy Containerize creates and displays in place of the original item. The result is the reference FormID of that target's existing storage chest.

GetChest is a lookup only. It returns zero for an unregistered target, an original base FormID, missing or deleted storage, or after Containerize uninstall. Passing an actor or an ordinary container does not retrieve storage belonging to an item inside it. The call does not create storage or register the target.

Linked storage belongs to Containerize. Use normal engine inventory operations for your own items, then call Refresh after changing the contents. Leave Containerize's internal original item untouched, and do not return this chest through another client's ReturnChest or Uninstall calls.

### Refreshing Containerize-managed storage

Containerize-managed storage is a chest connected to an item or furniture through Containerize's own registration. After changing its contents, refresh that storage once for the completed operation:

```cpp
const bool refreshed = api->Refresh(chest);
```

Pass the storage chest's reference FormID. Refresh updates the associated containerized item's weight/value, then its enclosing managed containers from the inside out. It invalidates affected inventory weight caches and calls the engine's weight calculation before returning.

If the requested storage or an enclosing container is open, its item weight/value update waits for Containerize's normal close routine, after the internal original item is restored. The open chest's actual inventory weight is still recalculated.

This does not register storage, find a furniture's linked chest or change its contents. A bare chest rented by an external client has no associated containerized item and returns false. Refresh also returns false after Containerize uninstall, for invalid storage, or if a required ancestor cannot be refreshed. Inner containers may already have been updated before an ancestor failure; the operation does not roll back those updates.

## Interface availability

Call the API on the game thread after a successful `kPostLoadGame` or `kNewGame` message.

`RequestAPI()` returns null when Containerize is unavailable or the interface version is unsupported. The interface contains the function pointers shown above and remains valid while Containerize is loaded. Consumers need Windows and CommonLibVR-MIT headers; no Containerize implementation headers or import library are required.
