# ConsoleDeck — In-Game Cheat Menu Built From Your Own Code

**Documentation: <https://wiki.teufel-engineering.com/en/ConsoleDeck/documentation>**

ConsoleDeck brings **no cheats of its own**. It shows what your project can already do.

```cpp
UFUNCTION(BlueprintCallable, meta = (ConsoleDeck = "Player|God Mode"))
void ToggleGodMode();

UFUNCTION(BlueprintCallable, meta = (ConsoleDeck = "World|Spawn Wave", ConsoleDeckArgs = "Count=10:1..64"))
void SpawnWave(int32 Count);
```

That is the integration. The functions are in the menu — on a gamepad, in a packaged build.
**There is no list to maintain.**

## Quick start

1. Enable the plugin.
2. Mark a `UFUNCTION` with `meta=(ConsoleDeck="Section|Name")`. In a Blueprint: the function's
   **Metadata** section, key `ConsoleDeck`.
3. Set your GameMode's **HUD Class** to `ConsoleDeck HUD` — or add two lines to the HUD you already have:

   ```cpp
   void AMyHUD::DrawHUD()          { Super::DrawHUD();          UConsoleDeckStatics::DrawDeck(this, Canvas); }
   void AMyHUD::Tick(float Delta)  { Super::Tick(Delta);        UConsoleDeckStatics::TickDeckInput(this, PlayerOwner, Delta); }
   ```

4. Press **F8**, or **View / Back** on a pad.

## Controls

| | Gamepad | Keyboard |
| --- | --- | --- |
| Open / close | View | F8 |
| Move | D-pad ↑↓ | ↑ ↓ |
| Value | D-pad ←→ | ← → |
| Section | LB / RB | PgUp / PgDn |
| Run | A | Enter |
| Rebuild | X | F5 |
| Reset | Y | Delete |
| Close | B | Escape |

All rebindable in **Project Settings → Plugins → ConsoleDeck**.

## Console

`ConsoleDeck.Open` · `Close` · `Rebuild` · `Unlock <word>` · `Lock` · `List` · `Stats` ·
`Invoke <Section|Name> [args]` · `Set <text>` · `Bake`

## Two things worth knowing before you ship

**Metadata does not survive a cook.** `WITH_METADATA` follows `WITH_EDITORONLY_DATA`. The editor bakes
the catalog into `DefaultGame.ini` after every rebuild and the packaged build reads that — check the file
in.

**The Shipping lock is a lock on a door, not a safe.** The menu is compiled into Shipping on purpose, and
opens only after `ConsoleDeck.Unlock <word>`. The word sits in a packaged ini file. If your cheats must
be unreachable in a public build, do not ship the plugin in that build.

Both are explained in full in `Docs/DOCUMENTATION.md`.

## Demo

`Content/ConsoleDeck/ConsoleDeck/Maps/L_ConsoleDeckDemo` — open it, press Play, press **F8**.

Nine marked functions that really change something: god mode, fly speed, respawn, spawned targets,
time of day, target spin, a marker with presets, and one that returns a string to the status line. They
live on `AConsoleDeckSampleCheats` — an ordinary actor, one metadata key per function, nothing else.
Delete that class in your own project, or narrow `ScanPathFilters` to your own code.

## Tests

`ConsoleDeck.*` in the Automation window, or in a commandlet on a build server.

---

Unreal Engine 5.8, Win64. One runtime module. No UMG, no editor module — the menu is drawn on `UCanvas`.

Support: <mailto:teufelsilvan@gmail.com>

<!-- SF-STORE-BLOCK:BEGIN -->
## 🛒 Source-available — see before you buy

This repository contains the **full source** of a commercial Unreal Engine plugin. It is **source-available, not open source**: read it, evaluate it, then buy a license to use it. See **the Fab Content License Agreement / Unreal Engine EULA (purchase required)**.

**Get it / Buy:**
- Fab store — all our UE5 plugins: https://www.fab.com/sellers/Silvan%20Teufel

_This plugin does not have its own Fab listing yet — the store link above is where everything we currently sell lives._

### 📬 **Free UE5 Snippet-Pack**

10 ready-to-use C++/Blueprint building blocks (subsystems, versioned saves, async nodes, editor tooling) — MIT licensed. Get it by joining the newsletter — plus a heads-up when something new ships. Double opt-in, unsubscribe in one click, no address sharing.

👉 **[Get the free pack](https://silvan.teufel-engineering.com/newsletter/plugins/?q=gh)**

_© 2026 Silvan Teufel. All rights reserved._
<!-- SF-STORE-BLOCK:END -->
