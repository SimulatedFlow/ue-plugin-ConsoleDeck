# ConsoleDeck — In-Game Cheat Menu Built From Your Own Code

**Full documentation: <https://wiki.teufel-engineering.com/en/ConsoleDeck/documentation>**

ConsoleDeck brings **no cheats of its own**. It shows what your project can already do.

Mark a function where it already lives:

```cpp
UFUNCTION(BlueprintCallable, meta = (ConsoleDeck = "Player|God Mode"))
void ToggleGodMode();
```

That is the whole integration. The function is in the menu, on a gamepad, in a packaged build.

---

## The problem

Two tools exist for switching something on while a game is running, and both need a keyboard.

* **The console** needs the command name typed from memory. There is no list, no discovery, and no
  console at all on a pad.
* **`UCheatManager`** is the same thing with a different owner: exec functions, typed by name.
* **The Gameplay Debugger** *shows* state — AI, perception, navigation. It switches nothing.
* **A hand-written debug menu** — the one thing on the store that does this — is a list somebody keeps
  up to date. Two sprints later it is wrong: it does not know about the function that was added
  yesterday, and it still offers the one that was deleted last week.

The case that fails today is specific and common: a tester, on a pad, in a packaged build, who needs to
turn something on. They cannot type, they do not know the command name, and nobody is going to make them
a build with a menu in it.

## What ConsoleDeck does

**It builds itself.** One walk over the reflection data at startup finds every `UFUNCTION` carrying the
`ConsoleDeck` metadata key, splits the value at the `|` into a section and a name, and that is the menu.
**There is no list to maintain.** A function that gets marked appears; one that gets deleted disappears.

**It is a gamepad menu first.** D-pad and shoulder buttons, a focus frame, a key repeat rate with
acceleration so a range of nought to ten thousand is crossable without letting go. No mouse is needed
anywhere.

**It knows types, not text.** A `bool` becomes a switch. A number becomes a value with limits taken from
`ClampMin` / `ClampMax`. An `enum` becomes a choice. An `FString` becomes a text field, or a list of
presets when you give it some. The call goes through `ProcessEvent` with a packed parameter buffer —
the same route the Blueprint VM takes, so a native function, a Blueprint function and a
BlueprintImplementableEvent all behave identically.

**It finds things that are not the player.** Beside the pawn and the player controller, it resolves game
modes, game states, player states, HUDs, actors anywhere in the level, actor components, and all four
kinds of subsystem. A search list in the project settings loads classes that nothing has touched yet, so
a manager subsystem nobody has called into still turns up.

**It is honest when it cannot help.** An entry whose target does not exist right now is **greyed out with
the reason** — *"no GameMode of this class in this world — a client has none"* — and not silently
dropped. A missing button looks like a bug in the plugin. A greyed-out one with a sentence next to it is
information.

**It survives Shipping.** The menu is compiled into a packaged Shipping build and stays shut until it is
unlocked. See *Shipping* below, which also says what that lock is and is not worth.

---

## Installation

**From Fab.** Add ConsoleDeck to your library, install it for Unreal Engine 5.8 from the Epic Games
Launcher, then in your project: **Edit → Plugins → Engine Tools → ConsoleDeck → Enabled**, and restart
the editor when asked.

**Into a single project.** Copy the `ConsoleDeck` folder into `<YourProject>/Plugins/` so that
`<YourProject>/Plugins/ConsoleDeck/ConsoleDeck.uplugin` exists. Right-click the `.uproject` →
**Generate Visual Studio project files**, then build. A Blueprint-only project needs a C++ toolchain the
first time, because ConsoleDeck ships a code module; adding any empty C++ class turns the project into a
code project and the plugin compiles with it.

**Verify it is there.** Start the game (PIE is enough) and type `ConsoleDeck.Stats` into the console. A
line with an entry count, a section count and a scan time in milliseconds means the subsystem is up and
the catalog was built.

Nothing else is required — no mapping context, no input action asset, no widget, no config file. The
plugin has no dependency on any other marketplace plugin and hard-codes no paths.

**Requirements**

| | |
| --- | --- |
| Engine | Unreal Engine **5.8** |
| Modules | one — `ConsoleDeck`, Runtime, `PreDefault` |
| Development platform | Windows |
| Target platform | **Win64** (`PlatformAllowList`) |
| Build configurations | Debug, Development, Test, **Shipping** |
| Engine dependencies | `Core`, `CoreUObject`, `Engine`, `InputCore`, `DeveloperSettings`, `RenderCore` |
| Not required | UMG, Enhanced Input, UnrealEd, any other plugin |

---

## Quick start

1. Enable the plugin.
2. Mark a function:

   ```cpp
   UFUNCTION(BlueprintCallable, meta = (ConsoleDeck = "Player|God Mode"))
   void ToggleGodMode();

   UFUNCTION(BlueprintCallable, meta = (ConsoleDeck = "World|Spawn Wave", ConsoleDeckArgs = "Count=10:1..64"))
   void SpawnWave(int32 Count);
   ```

   In a Blueprint, the same thing: open **Class Settings → Details → Metadata** on the function, add the
   key `ConsoleDeck` with the value `Player|God Mode`.

3. Set your GameMode's **HUD Class** to `ConsoleDeck HUD`, or add two lines to the HUD you already have
   (see *Hosting the deck in your own HUD*).
4. Press **F8**, or **View / Back** on a pad.

---

## Marking a function

### `ConsoleDeck`

The value is `Section|Name`.

| Written | Section | Name |
| --- | --- | --- |
| `"Player\|God Mode"` | Player | God Mode |
| `"Toggle Wireframe"` | General | Toggle Wireframe |
| `"\|Orphan"` | General | Orphan |
| `""` | General | *the function's own name* |
| `"World\|Spawn\|Wave"` | World | Spawn\|Wave |

Only the **first** bar splits. Whitespace is trimmed on both parts. Sections are matched
case-insensitively, so `Player` and `player` are one tab and not two that look identical.

### `ConsoleDeckArgs`

Defaults, limits and presets, one clause per parameter, separated by `;`:

```
ConsoleDeckArgs = "Count=10:1..64; Speed=600:100..2000; Marker=checkpoint|boss|end"
```

* `Name=Value` — the value the entry starts with, and what **Reset** goes back to.
* `:Min..Max` — the limits. The colon only counts as a separator when a range really follows it, so a
  string default containing one is safe.
* `a|b|c` — presets. This is the only way a string parameter is usable on a pad, where there is no
  keyboard to type `checkpoint` with.

A clause naming a parameter that does not exist is ignored and logged. A typo in a metadata string must
never be the reason a project fails to start.

### Limits from `ClampMin` / `ClampMax`

```cpp
UFUNCTION(BlueprintCallable, meta = (ConsoleDeck = "Player|Speed", ClampMin = "100", ClampMax = "2000"))
void SetSpeed(float Speed);
```

UHT has no syntax for per-parameter limits on a `UFUNCTION`, so a function-level clamp applies to every
number in the signature — which is right for the overwhelmingly common case of one number. When you need
finer control, or different limits per parameter, use `ConsoleDeckArgs`; it wins over the function-level
clamp, and unlike metadata it survives a cook.

### Parameter types

| Type | Control | Left / Right |
| --- | --- | --- |
| `bool` | switch | flips |
| `int32`, `int64`, `uint8`, … | number | ±1, or a twentieth of the range when limits are known |
| `float`, `double` | number | a twentieth of the range, or a tenth of the current value |
| `enum class`, `TEnumAsByte` | choice | cycles, wrapping |
| `FString`, `FName`, `FText` | text field, or presets | cycles the presets |
| anything else | — | the entry is greyed out, with the type in the reason |

Return values and output parameters are read back after the call and shown in the status line, not
edited before it. Latent functions are not callable and say so.

---

## Controls

| Action | Gamepad | Keyboard |
| --- | --- | --- |
| Open / close | View (Back) | F8 |
| Move | D-pad up/down, left stick | ↑ ↓ |
| Change value | D-pad left/right | ← → |
| Change section | LB / RB | Page Up / Page Down |
| Run the entry | A | Enter |
| Rebuild the catalog | X | F5 |
| Reset to defaults | Y | Delete |
| Close, or discard typing | B | Escape |

Every one of these is a `TArray<FKey>` in the project settings. Holding a direction repeats after
`RepeatDelaySeconds` and then every `RepeatRateSeconds`, and while repeating the step size is multiplied
by `RepeatStepMultiplier` — which is what makes a wide range crossable on a pad.

A `bool` entry flips **and runs** on a single press of A. God Mode is the most-used entry in any cheat
menu, and asking for two presses there would be one too many.

A free-text parameter with no presets goes into typing mode on A. Letters, digits, space, full stop,
hyphen and underscore, with Shift for capitals; Enter keeps it, Escape throws it away. On a machine with
a console, `ConsoleDeck.Set <text>` does the same thing without leaving the keyboard.

### Input while the deck is open

The deck **polls raw key state** through `APlayerController` rather than binding input actions. That
works under the legacy input stack and under Enhanced Input alike, with no mapping context, no input
action assets and no priority to agree with your project — a debug menu that asks you to author an input
action before you can open it is a menu nobody opens while hunting a bug.

The consequence is worth stating plainly: **your own bindings still fire while the deck is open.**
`bBlockMoveAndLookWhileOpen` (on by default) calls `SetIgnoreMoveInput` / `SetIgnoreLookInput`, which
stops the pawn moving and the camera turning, and that is all it does. If a face button also does
something in your game, gate it:

```cpp
if (UConsoleDeckStatics::IsDeckOpen(this)) { return; }
```

or turn on `bPauseGameWhileOpen`. There is no way for a plugin to guess which of your bindings should
lose.

---

## Where the entries come from

`UConsoleDeckSubsystem` builds the catalog **once, when the game instance comes up**, and keeps it. The
scan is a `TObjectIterator<UClass>` over every loaded class, and on a large project with thousands of
Blueprints in memory that is milliseconds, not microseconds. Doing it every time somebody presses F8
would make the menu feel broken on exactly the machines where it matters most, so it happens once — and
**the time it took is printed in the header of the menu**, next to what it found:

```
9 entries in 3 sections, scan 47.3 ms | live metadata
```

Nobody has to take that cost on trust.

Three settings shape the walk:

* **`bScanAllLoadedClasses`** (on) — the walk itself. This is what makes the plugin work with no
  configuration at all.
* **`ScanPathFilters`** — narrow it. `/Script/MyGame` and `/Game/` between them cut the walk down to your
  own code and your own content, which is where your marked functions are. Engine classes carry no
  `ConsoleDeck` key.
* **`ScanClasses`** — classes to **load** before looking. A class nobody has touched has no `UClass` in
  memory, and no walk over what is loaded can find what is not there. This is how a subsystem, a manager
  or a GameMode that has not been used yet still turns up in the menu.

A function is filed under the class that **declares** it, once. Without that, a base class with one
marked function would produce one entry per subclass, and the menu would be a wall of the same name. The
editor's `SKEL_`, `REINST_` and `TRASHCLASS_` copies of Blueprint classes are skipped, so nothing appears
twice in play-in-editor.

`ConsoleDeck.Rebuild`, or **X** in the menu, walks again — for the function you added since the game
started.

## Entries that cannot be used

Every time the deck opens, each entry looks for its target instance again. What it looks at, in order:

1. Subsystems — game instance, world, local player, engine.
2. The game instance itself.
3. For actors: the player controller, its pawn, its HUD, its player state, the GameMode, the GameState,
   and then every actor in the level.
4. For components: the pawn, the player controller, and then every actor in the level.

If nothing is found, the entry **stays in the list, greyed out, with the reason**:

* *no GameMode of this class in this world — a client has none*
* *no BP_BossArena spawned in this world*
* *game instance subsystem UEconomySubsystem is not running*
* *Count is a FVector, which the deck cannot edit*

An entry that is greyed out is also **not called** — pressing A on it writes the reason to the status
line and does nothing else. A row that looks disabled but still fires would be worse than no greying at
all.

The target is resolved **again, immediately before every call**, not reused from when the menu opened. An
actor can be destroyed in between, and a cheat menu that crashes the game it exists to debug has failed
at the only job it had.

---

## Cooking: why there is a baked catalog

**Function metadata does not survive a cook.** `WITH_METADATA` follows `WITH_EDITORONLY_DATA`, so in a
packaged game the `meta=(ConsoleDeck=…)` key the whole plugin is built on is simply not in memory. Every
tool that reads metadata at runtime has this problem; most of them do not mention it.

ConsoleDeck deals with it in the open:

* In the editor, after every rebuild, the catalog is written into **`DefaultGame.ini`** — class path,
  function name, section, display name and the `ConsoleDeckArgs` string. That is
  `bAutoBakeInEditor`, on by default, and `ConsoleDeck.Bake` does it on demand.
* The file is only rewritten when something actually changed, and the list is sorted, so two people who
  rebuild on two machines produce the same file and source control has nothing to argue about.
* **Check it in.** It is the difference between "the menu works on my machine" and "the menu works on the
  build the testers were given".
* In a cooked build there is no metadata to scan, so the deck loads the baked lines and resolves each one
  back to a real `UFunction` by name. Limits and presets ride along in the same line, because those are
  metadata too and would otherwise leave a slider in the packaged build with no ends.

The header of the menu says which of the two you are looking at — `live metadata` or `baked catalog`. When
a function is missing in a packaged build, that word tells you whether it was never marked or never baked.

---

## Shipping: why the menu stays in, and how to lock it

The deck is compiled into a Shipping build. That is a decision, not an oversight, and it comes with a
risk worth naming.

**Why not compile it out.** The bugs worth chasing are the ones that only happen in Shipping: the ones
that need optimisations on, checks off, the real cook and the real data. A debug menu that disappears in
exactly that configuration is a debug menu that is never there when it is needed. Shipping is also the
build QA is given, on the hardware QA has, with a pad and no keyboard.

**How it is shut.** In a Shipping build, `IsUnlocked()` returns false unless:

* `ConsoleDeck.Unlock <word>` was given with the word from `UnlockWord` in the project settings, **or**
* `bAllowInShippingWithoutUnlock` is on — for an internal build that goes to testers and nowhere else.

An **empty `UnlockWord` in a Shipping build means the deck cannot be opened at all**. A setting somebody
forgot has to fail the safe way round.

In Development and Debug builds the deck is open by default, because that is the build you are already
debugging in. `bRequireUnlockOutsideShipping` asks for the word there too.

**What the lock is worth.** It is a lock on a door, not a safe. The word sits in a packaged `.ini` file
and anybody willing to open that file will find it. It stops a player stumbling into the menu; it does
not stop somebody who is looking for it, and it is not a defence against a determined user of a
competitive game.

**If your cheats must be unreachable in a public build, do not ship this plugin in that build.** Remove
the plugin from the `Plugins` list of the build you publish, or give the module a
`PlatformDenyList` / target rule that excludes your shipping configuration. That is a real answer.
`UnlockWord` is a speed bump, and this documentation would rather say so than let somebody find out
later.

---

## Console commands

| Command | What it does |
| --- | --- |
| `ConsoleDeck.Open` | Open the menu. Refused when locked. |
| `ConsoleDeck.Close` | Close it. |
| `ConsoleDeck.Rebuild` | Walk the classes again and report what changed. |
| `ConsoleDeck.Unlock <word>` | Unlock the deck for this session. |
| `ConsoleDeck.Lock` | Shut it again. |
| `ConsoleDeck.List` | Write the whole catalog to the log, greyed-out reasons included. |
| `ConsoleDeck.Stats` | Entry count, section count, scan time, class count, source, lock state. |
| `ConsoleDeck.Invoke <Section\|Name> [args]` | Call an entry. Wrong arguments are refused, not guessed. |
| `ConsoleDeck.Set <text>` | Put text into the focused parameter. |
| `ConsoleDeck.Bake` | Write the catalog into `DefaultGame.ini`. Editor only. |

`ConsoleDeck.Invoke` takes fewer arguments than the function has parameters — the missing ones keep
whatever is dialled in, which makes calling an entry from the console as short as pressing the button in
the menu. **More** arguments than parameters is an error, because it almost always means the caller has
the wrong function in mind. Everything the menu does is reachable by typing; the menu exists for the case
where nobody can.

---

## Project settings

**Project Settings → Plugins → ConsoleDeck**, stored in `DefaultGame.ini`.

| Setting | Default | What it is |
| --- | --- | --- |
| `OpenKeys` | F8, Gamepad_Special_Left | Any of these opens the deck. |
| `bOpenKeyAlsoCloses` | true | The open key closes it again. |
| `bPauseGameWhileOpen` | false | Pause while the menu is up. |
| `bBlockMoveAndLookWhileOpen` | true | `SetIgnoreMoveInput` / `SetIgnoreLookInput` while open. |
| `UpKeys` … `ResetKeys` | see *Controls* | Every navigation key, rebindable. |
| `RepeatDelaySeconds` | 0.4 | Hold before a direction starts repeating. |
| `RepeatRateSeconds` | 0.07 | Seconds between repeats. |
| `RepeatStepMultiplier` | 4.0 | Step size while repeating. |
| `bScanAllLoadedClasses` | true | Walk every loaded class at startup. |
| `ScanPathFilters` | *(empty)* | Only scan classes under these paths. |
| `ScanClasses` | *(empty)* | Classes to load before scanning. |
| `bBuildCatalogOnStartup` | true | Build when the game instance comes up. |
| `bIncludeBlueprintClasses` | true | Include Blueprint-generated classes. |
| `bAutoBakeInEditor` | true | Write the catalog to config after every rebuild in the editor. |
| `BakedCatalog` | *(written)* | The baked catalog. Check it in. |
| `UnlockWord` | *(empty)* | The word `ConsoleDeck.Unlock` wants in Shipping. |
| `bAllowInShippingWithoutUnlock` | false | Open in Shipping with no word. |
| `bRequireUnlockOutsideShipping` | false | Ask for the word in Development too. |
| `bLogInvocations` | true | A log line per call. |
| `PanelPosition` | 48, 60 | Top left corner, in pixels. |
| `PanelWidth` | 640 | Width before the scale. |
| `PanelScale` | 1.5 | Everything is multiplied by this. The engine's small font is nine pixels tall — fine on a monitor, unreadable on a television across a room, which is where a pad-driven menu gets used. |
| `VisibleRows` | 12 | Rows before the list scrolls. |
| `bShowStatsHeader` | true | The header with the counts and the scan time. |
| `bShowKeyHintFooter` | true | The footer with the key bindings. |

---

## Classes and API

Six public classes, three public structs, two enums. Everything is `CONSOLEDECK_API` and everything
Blueprint-facing sits under the `ConsoleDeck` category.

| Class | Base | What it is |
| --- | --- | --- |
| `UConsoleDeckSubsystem` | `UGameInstanceSubsystem` | The catalog, the lock and the menu state. One per game instance, so it survives a level change. |
| `AConsoleDeckHUD` | `AHUD` | The default host. Set it as your GameMode's HUD class and the deck works with no further wiring. |
| `UConsoleDeckEntry` | `UObject` | One line of the menu: a `UFunction`, the class it was found on, its parameters and the reason it is greyed out. |
| `UConsoleDeckStatics` | `UBlueprintFunctionLibrary` | The Blueprint surface, plus the pure logic that needs no world. |
| `UConsoleDeckSettings` | `UDeveloperSettings` | Project Settings → Plugins → ConsoleDeck, stored in `DefaultGame.ini`. |
| `AConsoleDeckSampleCheats` | `AActor` | The nine marked functions the demo map runs on. Delete it in your own project. |

| Struct / enum | What it carries |
| --- | --- |
| `FConsoleDeckParam` | One parameter: name, kind, current value as text, default, min/max, step, presets, type name. |
| `FConsoleDeckSection` | One tab: a name and the indices of the entries in it. |
| `FConsoleDeckStats` | Entries, sections, classes scanned, functions marked, unavailable count, scan milliseconds, live-or-baked. |
| `FConsoleDeckEntryView` | The section/name pair `BuildSections` groups, kept free of `UObject`s so it is testable. |
| `FConsoleDeckBakedEntry` | One line of the baked catalog: class path, function name, section, display name, `ConsoleDeckArgs`. |
| `EConsoleDeckParamKind` | `Bool`, `Int`, `Float`, `String`, `Name`, `Text`, `Enum`, `Unsupported`. |
| `EConsoleDeckAvailability` | `Ready`, `NoInstance`, `NoWorld`, `UnsupportedSignature`, `NotFound`. |

### `UConsoleDeckSubsystem`

```cpp
#include "ConsoleDeckSubsystem.h"

if (UConsoleDeckSubsystem* Deck = UConsoleDeckSubsystem::Get(this))
{
    Deck->Rebuild();                       // walk the classes again
    Deck->RefreshAvailability();           // re-resolve every target, refresh the greyed-out reasons

    const FConsoleDeckStats& Stats = Deck->GetStats();
    UE_LOG(LogTemp, Display, TEXT("%d entries in %d sections, scan %.1f ms"),
        Stats.Entries, Stats.Sections, Stats.ScanMilliseconds);

    FString Result;
    Deck->InvokeByPath(TEXT("World|Spawn Targets"), { TEXT("24") }, Result);

    Deck->Open();                          // refused, and returns false, when the deck is locked
}
```

| Function | Notes |
| --- | --- |
| `static Get(WorldContextObject)` | Null outside a game. |
| `Rebuild()` | Scans, or reads the baked catalog in a cooked build. Keeps the selection where it can. |
| `RefreshAvailability()` | Re-resolves every target. Runs automatically on open. |
| `GetEntries()` / `GetSections()` / `GetStats()` | Native accessors, no copy. Blueprint gets copying twins. |
| `FindEntry(Path)` | `"Section\|Name"`, case-insensitive. |
| `Invoke(Entry)` / `InvokeByPath(Path, Args, OutResult)` | The text form runs the same coercion the menu does. |
| `BakeCatalog()` | Editor only. Returns the number of entries written, or `-1` elsewhere. |
| `IsUnlocked()` / `TryUnlock(Word)` / `Lock()` | The Shipping lock. See *Shipping*. |
| `Open()` / `Close()` / `Toggle()` / `IsOpen()` | `Open` returns false when locked. |
| `DrawDeck(Canvas)` / `TickInput(PC, DeltaSeconds)` | What `AConsoleDeckHUD` forwards to. |
| `DumpToLog()` | The whole catalog, greyed-out reasons included. |
| `OnEntryInvoked` / `OnVisibilityChanged` | `BlueprintAssignable` multicast delegates. |

```cpp
// Mute your own input while the menu is up
Deck->OnVisibilityChanged.AddDynamic(this, &AMyPlayerController::HandleDeckVisibility);

// React to what a tester pressed
Deck->OnEntryInvoked.AddDynamic(this, &AMyGameMode::HandleCheatUsed);
// void HandleCheatUsed(UConsoleDeckEntry* Entry, bool bSucceeded, const FString& Result);
```

### `UConsoleDeckEntry`

```cpp
if (UConsoleDeckEntry* Entry = Deck->FindEntry(TEXT("Player|Fly Speed")))
{
    if (!Entry->IsReady())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: %s"), *Entry->GetPath(), *Entry->UnavailableReason);
    }

    Entry->Params[0].Value = TEXT("1800");
    Entry->Invoke(GetWorld());             // resolves the target again first, then ProcessEvent
    UE_LOG(LogTemp, Display, TEXT("%s"), *Entry->LastResult);
}
```

`Section`, `DisplayName`, `TargetClass`, `ClassPath`, `Params`, `Availability`, `UnavailableReason`,
`LastResult`, `bSignatureSupported`, `SignatureReason`, `ArgSpec`, `CachedTarget` are all
`BlueprintReadOnly`. `AdjustParam(Index, Direction, StepScale)` is the d-pad; `ResetParams()` is the
reset key.

### `UConsoleDeckStatics` — the pure logic

These take no world, touch no `UObject` and are the reason the tests run in a commandlet:

```cpp
FString Section, Name;
UConsoleDeckStatics::ParseMeta(TEXT("Player|God Mode"), Section, Name);   // "Player", "God Mode"
UConsoleDeckStatics::ParseMeta(TEXT("God Mode"), Section, Name);          // "General", "God Mode"

TArray<FConsoleDeckSection> Sections = UConsoleDeckStatics::BuildSections(Views);

FString Value, Error;
if (!UConsoleDeckStatics::CoerceValue(Param, TEXT("twelve"), Value, Error))
{
    // Error: "twelve is not a whole number" - refused, not silently turned into 0
}

const double Clamped = UConsoleDeckStatics::ClampToParam(Param, 99999.0);
UConsoleDeckStatics::ApplyArgSpec(TEXT("Count=10:1..64"), Params);
```

| Function | Notes |
| --- | --- |
| `ParseMeta` | Splits at the **first** bar only. No bar → section `General`. |
| `BuildSections` | Case-insensitive grouping, alphabetical and stable, so the menu never reshuffles. |
| `CoerceValue` / `CoerceArgs` | Refuse what they cannot convert. Missing args keep the dialled-in value; extra args are an error. |
| `ClampToParam` | `ClampMin` / `ClampMax`, applied to the arrows as well as to typed text. |
| `ApplyArgSpec` | Reads `ConsoleDeckArgs` into the parameters it names; unknown names are logged, not fatal. |
| `DescribeFunction` | Native only. Turns a `UFunction` into `FConsoleDeckParam`s and says why a signature is unsupported. |

### `UConsoleDeckStatics` — the Blueprint surface

`Open Deck`, `Close Deck`, `Toggle Deck`, `Is Deck Open`, `Is Deck Unlocked`, `Unlock Deck`,
`Rebuild Deck`, `Invoke Entry`, `Get Deck Stats`, `Draw Deck`, `Tick Deck Input` — all with a world
context, so they work from any Blueprint.

```cpp
// C++ from anywhere with a world context
UConsoleDeckStatics::ToggleDeck(this);

FString Result;
UConsoleDeckStatics::InvokeEntry(this, TEXT("Debug|Set Marker"), { TEXT("boss") }, Result);

if (UConsoleDeckStatics::IsDeckOpen(this)) { return; }   // gate your own bindings
```

---

## Hosting the deck in your own HUD

`AConsoleDeckHUD` is the shortcut, not the requirement. Most projects already have a HUD class and are
not going to give it up for a debug menu, so the drawing and the input live in the subsystem and the HUD
actor is two forwarding calls:

```cpp
void AMyHUD::DrawHUD()
{
    Super::DrawHUD();
    UConsoleDeckStatics::DrawDeck(this, Canvas);
}

void AMyHUD::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UConsoleDeckStatics::TickDeckInput(this, PlayerOwner, DeltaSeconds);
}
```

Set `PrimaryActorTick.bTickEvenWhenPaused = true` on your HUD if you turn on `bPauseGameWhileOpen` — a
menu that can pause the game has to keep working afterwards.

The menu is drawn on `UCanvas`, never in UMG. No widget tree has to be cooked, referenced or kept alive
by your project, and the deck works in a level that contains nothing but a floor.

In Blueprint the same two calls exist as **Draw Deck** and **Tick Deck Input** nodes, so a Blueprint HUD
hosts the deck without any C++ at all.

---

## The demo map

Package path `/ConsoleDeck/ConsoleDeck/Maps/L_ConsoleDeckDemo`, on disk
`Plugins/ConsoleDeck/Content/ConsoleDeck/Maps/L_ConsoleDeckDemo.umap` — everything the plugin ships as
content lives under that one pack folder.

Open it, press Play, press **F8** — or View on a pad.

A small room with a floor, two plinths and a directional light. The nine entries in the menu come from
`AConsoleDeckSampleCheats` (`Public/ConsoleDeckSampleCheats.h`), which is an ordinary actor with ordinary
functions; the only thing that puts any of them in the menu is the metadata key on the line above each one.
Read that header next to the running menu and the whole plugin is explained in one screen.

| Entry | Parameter | What it does |
| --- | --- | --- |
| `Player\|God Mode` | a bool, drawn as a switch | Flips the flag and the HUD readout. |
| `Player\|Fly Speed` | a number, 150–3000 | The pawn really flies faster. |
| `Player\|Respawn At Start` | none, drawn as a button | Puts the pawn back at the start. |
| `World\|Spawn Targets` | a whole number, 1–40 | That many cubes appear, on a grid. |
| `World\|Clear Targets` | none | Destroys them again. |
| `World\|Time Of Day` | a number, 0–24 | Turns the directional light; noon is overhead. |
| `World\|Target Spin` | a number, 0–720 | Degrees per second the cubes turn at. |
| `Debug\|Set Marker` | a string with four presets | Sets a label, no keyboard needed. |
| `Debug\|Report Targets` | none, returns a string | The return value lands on the status line. |

Nothing in the demo is a mock-up: every entry really changes something, because a screenshot of a menu
where the buttons do nothing is worth nothing.

The rest of the map is four Blueprints and five materials, all under the one pack folder:
`BP_ConsoleDeckSampleCheats` is the placed actor — its Details panel is where `Target Class`, `Sun Light`
and the spawn grid are set. `BP_ConsoleDeckDemoGameMode` sets the HUD class to `BP_ConsoleDeckDemoHUD`,
which derives from `ConsoleDeck HUD` and adds one line of `DrawText` showing the state the cheats change,
so you can see the effect without looking away from the menu. `BP_ConsoleDeckTarget` is the cube that
gets spawned. The menu itself is Canvas — there is no UMG anywhere in the demo.

**In your own project, delete the sample.** It is a runtime class in this module, so its nine entries are
in the catalog whether or not the actor is in the level — greyed out, with the reason, as any entry whose
target is missing would be. Setting `ScanPathFilters` to `/Script/YourGame` and `/Game/` removes them and
cuts the startup scan down to your own code at the same time, which is worth doing regardless.

---

## Tests

`ConsoleDeck.*`, in `Automation` → `ConsoleDeck`. They run in the editor and in a commandlet, which means
on a build server:

1. `ParseMetaSplitsSectionAndFallsBackToGeneral`
2. `BuildSectionsGroupsAndIsStable` — same catalog, same menu, whatever order the class walk found it in
3. `CoerceConvertsAndRefusesInsteadOfGuessing` — `twelve`, `12abc` and `maybe` are refused, not turned
   into a zero
4. `ClampMinAndClampMaxAreHeld` — from the keyboard and from the d-pad
5. `MissingTargetIsGreyedOutWithAReasonAndNotCalled`
6. `ArgSpecCarriesDefaultsLimitsAndPresets`

---

## What ConsoleDeck is not

* **Not a replacement for the console.** Everything here is reachable by typing, and every console
  command your project has still works. The deck is for the case where there is no keyboard.
* **Not a source of cheats.** It ships none. It calls yours.
* **Not a security feature.** See *Shipping*.
* **Not a UMG menu.** It is drawn on `UCanvas`, and it cannot be styled with your UI theme. That is the
  price of working in a packaged build with no widget tree.
* **Not able to edit structs, objects or arrays.** Those parameters are shown greyed out with the type
  named. A control that pretended to edit an `FVector` and silently sent zeroes would be worse.
* **Not a multiplayer tool.** It calls a function on an instance in the world it is running in. A
  `Server` RPC routes as it normally would; a GameMode function on a client is greyed out, and says so.

---

## Supported platforms and engine

**Unreal Engine 5.8.** The plugin declares `"EngineVersion": "5.8.0"` and is built and tested against
that version only. Nothing in it uses an experimental API, but no claim is made for 5.7 or earlier.

| | |
| --- | --- |
| **Supported development platforms** | Windows |
| **Supported target build platforms** | **Win64** |
| **Build configurations** | Debug, DebugGame, Development, Test, **Shipping** — the deck is compiled into all of them |
| **Editor** | Works in PIE and in Standalone; there is no editor module and no editor-only feature except `ConsoleDeck.Bake` |
| **Cooked builds** | Supported through the baked catalog — see *Cooking* |
| **Blueprint-only projects** | Supported once the project has a C++ toolchain; the plugin ships a code module |
| **Network** | Not replicated. The deck calls a function on an instance in the world it runs in; a `Server` RPC routes as it normally would |
| **Dedicated server** | The module compiles, but a server has no local player controller and therefore no menu. Use `ConsoleDeck.Invoke` there |

The single module carries `"PlatformAllowList": [ "Win64" ]`. Other platforms are not listed because
they have not been tested; the code contains no Windows-specific API, so widening the list is a
build-and-test exercise rather than a port. Console platforms are not covered by a Fab distribution
either way.

**No third-party code.** No bundled libraries, no GPL or otherwise restricted source, no dependency on
any other marketplace plugin, and no hard-coded absolute paths.

---

## Support

* Documentation: <https://wiki.teufel-engineering.com/en/ConsoleDeck/documentation>
* Support: <mailto:teufelsilvan@gmail.com>

Unreal Engine 5.8, Win64. One runtime module, no editor module, no UMG.

*Copyright 2026 Silvan Teufel. All Rights Reserved.*
