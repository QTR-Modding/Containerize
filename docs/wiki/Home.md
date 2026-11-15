# Containerize preset guide

## Your first preset

Save this as `SKSE/Plugins/Containerize/presets/Basket.yml` inside your mod's folder.

```yaml
containers:
  - FormEditorID: 0xABD30~Skyrim.esm
    weight_limit: 10
```

Start Skyrim and look at a basket with this Form ID. Choose **Open** to use its storage. With QuickLoot IE, choose **Search** in the loot panel to reach the full menu and put items inside. The weight limit is 10.

Add entries under `containers` for the items you want to configure. `FormEditorID` identifies the item. Here `0xABD30` is the basket's record number, and `Skyrim.esm` is the plugin that defines it. You can also use an Editor ID (the record's editor name in xEdit) or a form group (a named list of records under `SKSE/Plugins/Containerize/formGroups`).

## Weight and starting contents

Add these lines below `weight_limit` in the basket preset. Half the contents' weight now counts toward the limit, and the basket starts with one gold coin the first time its storage is created.

```yaml
    cloud_storage: 0.5
    initial_items:
      - FormEditorID: 0xF~Skyrim.esm
        count: 1
```

Omit `weight_limit` or set it to `0` for unlimited capacity. Use `cloud_storage: false` to count the full weight. Use `true` to ignore all contents weight. A number from 0 to 1 gives the fraction to ignore. When you omit it, the preset uses `CloudStorage` from [the INI](#ini-settings).

## World storage

Add `world_storage: true` below `weight_limit` to give the basket storage only while it's placed. When you pick it up, the original basket and its contents go into your inventory as separate items. With the default mode, the stored contents stay with the item when you carry it.

Configured furniture uses world storage automatically. When you set `transfer_on_use: true` on a crafting station, its stored items are available for crafting while you use it. Unused items go back into storage when you leave the station.

## QuickLoot IE

With QuickLoot IE installed, Containerize's **Open** prompt opens its loot panel by default. **Search** in the panel opens the full container menu. Set `quickloot: false` below `weight_limit` to send **Open** directly to the full menu for that item.

## SkyPrompt

Add these lines below `weight_limit` to show the world prompts next to the basket, label **Open** as **Open basket**, and hide **Rename**.

```yaml
    skyprompt:
      attach: true
      labels:
        open: "Open basket"
        rename: ""
```

1. `attach` defaults to `false`. It controls where prompts appear when you look at the object in the world. Inventory prompts stay in their usual place.
2. Set `theme: mytheme` under `skyprompt` to use `SKSE/Plugins/SkyPrompt/themes/mytheme.json`. Leave out `theme` to use the default.
3. Under `labels`, set `open`, `rename`, `weight`, or `value` to change that prompt's text. Use `""` to hide a prompt. Labels you leave out keep the default translated text.

## INI settings

Edit `SKSE/Plugins/Containerize.ini` to change settings for the whole mod. Keep the settings under `[Other Stuff]`. Restart Skyrim after editing the file.

| Setting | Default | What it does |
| --- | --- | --- |
| `INI_changed_msg` | `true` | Warns when a container in the loaded save is missing from your current configuration. |
| `RemoveCarryBoosts` | `true` | Removes the original item's carry-weight bonus from its Containerize copy. |
| `ReturnToInitialMenu` | `true` | Returns to the previous menu after you close storage opened from that menu. |
| `CloudStorage` | `false` | Makes stored items weightless when enabled. A preset can override it with `cloud_storage`. |
| `AnimationsDelayMenuOpen` | `false` | Waits for the opening animation before showing storage. |
| `PlayAnimationsOnlyIfEquipped` | `false` | Plays opening animations only for equipped items when enabled. |

The animation settings apply when a mod supplies Containerize animations through Dynamic Animation Framework. `ObjectManipulationOverhaul` is inactive in Containerize 1.1.0. Selling a container with its contents is always enabled.

### Set up an item in the INI

The INI can also set up the same basket. Add these lines to the corresponding sections of `Containerize.ini`.

```ini
[Containers]
basket = 000ABD30

[Capacities]
basket = 10
```

Use the same key (`basket` here) under both sections. Give each item its own key and use `0` for unlimited capacity. Use the item's full Form ID or its Editor ID with powerofthree's Tweaks installed. When both files configure the same item, the YAML preset takes priority.

## Storage for other mods

Set `token_mode: true` on an item whose storage another mod manages. You can still pick up, equip, and remove the item. Containerize hides its prompts, keeps players from opening its storage, and keeps its contents out of crafting menus.

See the [chest rental guide](https://github.com/QTR-Modding/Containerize/blob/main/docs/ChestRentalAPI.md) to rent storage for your mod and the [linked storage guide](https://github.com/QTR-Modding/Containerize/blob/main/docs/LinkedStorageAPI.md) to manage storage attached to an item or furniture.
