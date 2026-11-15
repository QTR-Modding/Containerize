# Containerize preset guide

## Your first preset

Save this as `SKSE/Plugins/Containerize/presets/Basket.yml` in your mod. Every `.yml` file starts with `containers:`.

```yaml
containers:
  - FormEditorID: 0xABD30~Skyrim.esm
    weight_limit: 10
```

This gives the matching base-game basket storage for up to 10 units of weight. Restart Skyrim, look at the basket, and use Containerize's **Open** prompt. Add one entry under `containers` for each item you want to configure. `FormEditorID` accepts an Editor ID, a Form ID such as `0xABD30~Skyrim.esm`, or the name of a form group from `SKSE/Plugins/Containerize/formGroups`.

## Weight and starting contents

Add these fields to the basket entry to make half the contents' weight count and put a gold coin inside when the storage is first created.

```yaml
    cloud_storage: 0.5
    initial_items:
      - FormEditorID: 0xF~Skyrim.esm
        count: 1
```

Omit `weight_limit` for unlimited capacity. `cloud_storage: true` removes all contents weight and `false` counts all of it. A number from 0 to 1 sets the fraction of weight to ignore. Omit `cloud_storage` to use your INI setting.

## World storage

Add `world_storage: true` to the basket entry to make the basket hold items only while placed. When you pick it up, you get the original basket and its stored contents separately in your inventory. Configured furniture uses world storage automatically.

## QuickLoot IE

With QuickLoot IE installed, Containerize's **Open** prompt on a placed object shows its loot panel. Use **Search** in the panel to reach the full container menu. Set `quickloot: false` on a preset entry to have **Open** go straight to the full menu for that item.

## SkyPrompt

Add this to the basket entry to show the prompts next to the basket, change **Open** to **Open basket**, and hide **Rename**.

```yaml
    skyprompt:
      attach: true
      labels:
        open: "Open basket"
        rename: ""
```

1. `attach` defaults to `false`. It changes where prompts appear in the world. Inventory prompts stay in their usual place.
2. To choose a different theme, set `theme: mytheme` under `skyprompt`. SkyPrompt loads `SKSE/Plugins/SkyPrompt/themes/mytheme.json`. Omit `theme` for the default.
3. Set `open`, `rename`, `weight`, or `value` under `labels` to change that prompt. An empty string (`""`) hides the prompt. Omitted labels use the default translated text.

## Storage for other mods

Set `token_mode: true` on an item whose storage another mod manages. Containerize hides its prompts and keeps its contents out of the player's crafting menus. You can still pick up, equip, and remove the item as usual.

See the [Containerize API](https://github.com/QTR-Modding/Containerize/blob/main/docs/ChestRentalAPI.md) for renting storage and the [linked storage guide](https://github.com/QTR-Modding/Containerize/blob/main/docs/LinkedStorageAPI.md) for working with storage attached to an item or furniture.
