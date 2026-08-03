# Pathing packs

Put TacO / BlishHUD **`.taco`** files in this folder. The addon loads all of them.

## Curated packs (auto-updated)

On first Pathing load (and when you click **Update curated**), the addon downloads:

| Pack | Source |
|------|--------|
| `tw_ALL_IN_ONE.taco` | [Tekkit's Workshop](https://www.tekkitsworkshop.net/) CDN |
| `LadyElyssa.taco` | [LadyElyssaTacoTrails](https://github.com/LadyElyssa/LadyElyssaTacoTrails) GitHub Releases |
| `LadyElyssaAP.taco` | [LadyElyssaAchievementGuides](https://github.com/LadyElyssa/LadyElyssaAchievementGuides) GitHub Releases |

Credits: **Tekkit's Workshop** (All-In-One — used with permission) · **Lady Elyssa**
([wiki](https://wiki.guildwars2.com/wiki/User:Lady_Elyssa)).

Stamp files (`*.taco.ver`) track the release / size so updates only re-download when needed.
**Your own `.taco` files are never deleted.**

## Custom packs

1. Copy any `.taco` pack into this folder (same format as TacO / BlishHUD / Taimi).
2. In-game, open **Pathing** and click **Reload packs** (or **Update curated** to refresh downloads too).
3. Enable the categories you want (everything starts unchecked each launch).

Installed path:

```
<Guild Wars 2>/addons/GW2-InGame-Helper/pathing/
```

Taimi / Blish / TacO are **not** required. Fallback discovery still looks for
packs under Minimap Resizer / those installs if our folder is empty.
