# Containerize-linked storage

The functions below are part of the same `ContainerizeAPI::Interface` as [ChestManager rentals](ChestRentalAPI.md). Call them on the game thread after a successful `kPostLoadGame` or `kNewGame` message.

These calls manage storage linked to an item or furniture by Containerize. Your mod does not rent that storage or take ownership of it.

## Initializing linked storage

To initialize a configured placed item or furniture without opening its menu, pass its reference FormID:

```cpp
#include "API.h"

RE::FormID InitializeStorage(RE::FormID reference) {
    const auto api = ContainerizeAPI::RequestAPI();
    return api ? api->Initialize(reference, 0, nullptr) : 0;
}
```

The result is the fake item FormID, which you can pass to GetChest, Rename or DeRegister. The fake item is Containerize's copy of the original item, associated with that one storage chest. The object's base form must already be configured in a Containerize preset. Initial items and weight/value use normal Containerize registration.

For one item in an inventory, pass its original base FormID and the inventory owner's reference FormID:

```cpp
const RE::FormID fake = api->Initialize(originalItem, inventoryOwner, nullptr);
```

Each call converts one original inventory item. Null instance uses normal registration's default instance selection. To select a specific customized copy, pass its current `RE::ExtraDataList*` from that owner's inventory as the third argument. An instance belonging to another item or owner is rejected. The selected item's extra data and equipped state are preserved.

Passing an already linked reference or fake item returns its existing fake ID without adding initial items again. Missing, unconfigured or unavailable targets return zero. Initialization never opens a menu or plays an opening animation.

## Token items

Set `token_mode: true` in a container preset when another mod should manage its storage:

```yaml
containers:
- FormEditorID: MyStorageToken
  token_mode: true
```

The item can be taken, equipped or removed normally. Containerize hides its Open prompt and prevents opening the storage or using its contents as crafting ingredients. Initialize, Rename, Refresh and DeRegister work normally. Omitted or false keeps normal container behavior.

## Finding linked storage

```cpp
const RE::FormID chest = api->GetChest(target);
if (!chest) return;  // No existing linked storage.
```

Pass a placed item's or furniture's reference FormID, or the fake item FormID of an already containerized item in an inventory. The fake item is the copy Containerize creates and displays in place of the original item. The result is the reference FormID of that target's existing storage chest.

GetChest is a lookup only. It returns zero for an unregistered target, an original base FormID, missing or deleted storage, or after Containerize uninstall. Passing an actor or an ordinary container does not retrieve storage belonging to an item inside it. The call does not create storage or register the target.

Linked storage belongs to Containerize. Use normal engine inventory operations for your own items, then call Refresh after changing the contents. Leave Containerize's internal original item untouched, and do not return this chest through another client's ReturnChest or Uninstall calls.

## Refreshing Containerize-managed storage

Containerize-managed storage is a chest connected to an item or furniture through Containerize's own registration. After changing its contents, refresh that storage once for the completed operation:

```cpp
const bool refreshed = api->Refresh(chest);
```

Pass the storage chest's reference FormID. Refresh updates the associated containerized item's weight/value, then its enclosing managed containers from the inside out. It invalidates affected inventory weight caches and calls the engine's weight calculation before returning.

If the requested storage or an enclosing container is open, its item weight/value update waits for Containerize's normal close routine, after the internal original item is restored. The open chest's actual inventory weight is still recalculated.

This does not register storage, find a furniture's linked chest or change its contents. A bare chest rented by an external client has no associated containerized item and returns false. Refresh also returns false after Containerize uninstall, for invalid storage, or if a required ancestor cannot be refreshed. Inner containers may already have been updated before an ancestor failure; the operation does not roll back those updates.

## Renaming a linked item

```cpp
const bool renamed = api->Rename(fake, "Travel supplies");
```

Pass the fake item FormID returned by Initialize and a nonempty UTF-8 name. The name is saved and retained through refresh and load. Placed objects also receive the display name. Invalid fake IDs, null names and empty names return false.

## Removing the link

```cpp
const bool removed = api->DeRegister(fake);  // A linked placed reference also works.
```

For an inventory item, this replaces the fake with the original and moves the stored contents into its owning inventory. For a placed object, the original stays in the world and the contents go to the player. If the owning inventory no longer exists, the player receives the contents. Nested containerized items keep their own storage and move with their contents intact.

The storage chest is returned to ChestManager. Do not retain or use that chest reference afterward. Returning contents can put a containing container over its configured capacity; this return is not treated as a normal deposit.

The call returns false without changes while that storage is open, scheduled to open/reopen, or lending items for crafting. Close/finish that operation before deregistering it. Invalid targets and Containerize uninstall also return false.

True means the link was removed and the chest return accepted. If an item transfer fails, Containerize shows its existing in-game warning and leftover contents are discarded through ChestManager's disposal. Unexpected failures after work starts are not rolled back.

