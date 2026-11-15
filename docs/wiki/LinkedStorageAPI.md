# Containerize-linked storage

## Initializing linked storage

Suppose your mod needs to place ingredients into the storage that a player opens from a cooking spit. Configure the spit in a [Containerize preset](https://github.com/QTR-Modding/Containerize/wiki), then pass its reference FormID to `Initialize` and look up its chest with `GetChest`. A reference FormID identifies that one placed spit.

```cpp
#include "API.h"

RE::FormID InitializeAndGetChest(RE::FormID placedSpit) {
    const auto api = ContainerizeAPI::RequestAPI();
    if (!api) {
        return 0;
    }

    const auto linkedForm = api->Initialize(placedSpit, 0, nullptr);
    return linkedForm ? api->GetChest(linkedForm) : 0;
}
```

Run this on the game thread once SKSE reports a successful game load (`kPostLoadGame`) or a new game (`kNewGame`). A nonzero result is the chest's FormID. To add ingredients, get the chest with `RE::TESForm::LookupByID<RE::TESObjectREFR>(chestID)` and use Skyrim's inventory functions. Call [Refresh](#refreshing-containerize-managed-storage) with the chest ID when you're done. Containerize manages this chest. Use [chest rentals](https://github.com/QTR-Modding/Containerize/wiki/ChestRentalAPI) when your mod needs its own inventory.

For furniture and items configured with `world_storage: true`, `Initialize` returns the placed reference FormID. For ordinary carryable containers, it returns the form ID of the item copy that Containerize creates. The API calls this copy the **fake item**. Either result identifies the linked object for `GetChest`, `Rename`, and `DeRegister`.

You can also give an item storage while it's already in someone's inventory. Use a preset for carryable storage, then pass the original item's base FormID and the reference FormID of the player, NPC, or chest that holds it.

```cpp
const auto linkedForm = api->Initialize(originalItem, inventoryOwner, nullptr);
```

This replaces one copy of the item and preserves its equipped state and extra data, such as its enchantment. Use `nullptr` to let Containerize choose the copy. To choose a specific copy, pass its `RE::ExtraDataList*` from that inventory as the third argument.

Call `Initialize` again with an already linked reference or fake item to get its existing linked ID. Starting contents are added once. Zero means initialization failed, for example because the object is missing or lacks a preset. The call leaves the container menu closed.

## Container prompts

See [QuickLoot IE](https://github.com/QTR-Modding/Containerize/wiki#quickloot-ie) and [SkyPrompt](https://github.com/QTR-Modding/Containerize/wiki#skyprompt) for preset examples.

## Token items

See [Storage for other mods](https://github.com/QTR-Modding/Containerize/wiki#storage-for-other-mods) to manage storage through the API while keeping it out of the player's menus.

## Finding linked storage

```cpp
const auto chest = api->GetChest(linkedForm);
```

Pass the placed reference FormID or the fake item FormID. `GetChest` returns the reference FormID of the existing storage chest, or zero when it finds no link.

Keep the original item copy that Containerize stores in the chest intact. Use [DeRegister](#removing-the-link) to remove the link and return its contents.

### Finding the linked item or reference

```cpp
const auto linkedForm = api->GetLinkedForm(chest);
```

This reverses the lookup. It returns the fake item FormID for a carryable container, the reference FormID for world storage, or zero for a chest without a link.

## Finding all linked chests for a base form

```cpp
std::vector<RE::FormID> chests(api->GetChests(baseForm, {}));
api->GetChests(baseForm, chests);
```

Include `<vector>` for this example. Pass the original item or furniture record's FormID. Call once with an empty buffer to count its chests, then again to fill your buffer with their reference FormIDs. The return value is the total number of matches, even when the buffer holds fewer.

## Finding the original item

```cpp
const auto original = api->GetRealID(chest);
```

For a carryable container, this returns the base FormID of the item Containerize copied. A chest without a registered carryable item, such as world storage, returns zero.

## Refreshing Containerize-managed storage

```cpp
const bool refreshed = api->Refresh(chest);
```

Pass the storage chest's reference FormID after adding or removing items. Refresh recalculates the chest's weight. For carryable containers, it also updates the item's weight and value and works outward through any enclosing Containerize storage. An open container's item weight and value update when its menu closes.

False means the chest is unavailable, belongs to an external rental, or a required refresh failed. Updates already made stay applied.

## Renaming a linked item

```cpp
const bool renamed = api->Rename(linkedForm, "Travel supplies");
```

Pass the linked ID and a nonempty UTF-8 name. For a carryable container, use its fake item ID. For world storage, use the placed reference ID. Save the game to keep the new name. Skyrim can reject a rename when a quest controls the object's display name.

## Removing the link

```cpp
const bool removed = api->DeRegister(linkedForm);
```

For an item in an inventory, this restores the original and moves its contents into that inventory. For an object in the world, the object stays in place and the player receives the contents. The player also receives the contents when the original inventory is unavailable. Nested containers keep their own storage. Returning items can put the destination over its capacity.

Close the storage and finish any crafting that is using its contents before calling. The call also returns false when the storage is queued to open. After it succeeds, stop using the chest reference.

True means the link was removed and the chest return was accepted. If an item cannot be moved, Containerize warns the player and discards the leftover contents. Transfers that already happened stay applied.
