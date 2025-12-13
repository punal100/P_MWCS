﻿# P_MWCS (Modular Widget Creation System)

P_MWCS is an Unreal Engine **editor-only** plugin that deterministically **creates / repairs / validates** Widget Blueprints from JSON specs (no Python).

The source of truth is a C++ `static FString GetWidgetSpec()` on “spec provider” classes. MWCS discovers these providers from an allowlist, parses the JSON into typed structs, then rebuilds the UMG `WidgetTree` deterministically.

## Requirements

- Unreal Engine 5.5 (source build)
- Editor-only module (`Type: Editor`) — not included in packaged builds
- No Python dependency

## What MWCS does

- **Discover specs** from configured spec providers
- **Validate** assets (parent class, required widgets, hierarchy expectations)
- **Create Missing**: only creates assets that don’t exist
- **Repair**: deterministically rebuilds the widget tree for existing assets
- **ForceRecreate**: deterministic “rebuild everything” mode (implemented as an in-place rebuild to avoid deletion-time engine warnings in strict CI)
- **Generate/Repair Tool EUW**: creates an Editor Utility Widget from a dedicated tool spec provider
- **CI/headless support** via commandlets with non-zero exits on errors/warnings

All build modes:

- Rebuild the `WidgetTree`
- Compile
- Save the package

## Install / Enable

1. Ensure the plugin folder is at `A_MiniFootball/Plugins/P_MWCS/`
2. Enable **P_MWCS** in the Plugins window
3. Restart the editor

## Settings

MWCS settings are exposed as `UMWCS_Settings : UDeveloperSettings`.

In **Project Settings → MWCS**, configure:

- **Spec Provider Classes (Allowlist)**: list of C++ classes whose `GetWidgetSpec()` will be called
- **OutputRootPath**: long package path for generated WBPs (example: `/Game/UI/Widgets`)
- **Tool EUW Output Path / Name / Spec Provider Class** (optional)

## Spec provider contract

MWCS expects a static function:

```cpp
UFUNCTION(BlueprintCallable, BlueprintPure)
static FString GetWidgetSpec();
```

At minimum, the JSON should include:

- `BlueprintName` (string)
- `ParentClass` (string, soft class path like `/Script/YourModule.YourWidgetClass`)
- `Hierarchy` (tree)
- `Bindings` (object; required/optional)
- `Version` (int)
- `DesignerPreview` (object; replaces legacy `DesignerToolbar`)

### `DesignerPreview` (replaces `DesignerToolbar`)

MWCS uses `DesignerPreview` to apply preview sizing deterministically.

Example:

```json
"DesignerPreview": {
   "SizeMode": "FillScreen",
   "CustomSize": { "X": 1920, "Y": 1080 },
   "ZoomLevel": 14,
   "bShowGrid": true
}
```

Notes:

- `SizeMode` / `CustomSize` are applied strictly.
- `ZoomLevel` / `bShowGrid` are treated as best-effort UX hints (engine versions may not persist these per-asset).

### Nested widgets: `Type: "UserWidget"` + `WidgetClass`

To place a nested user widget in a hierarchy, use:

```json
{
  "Type": "UserWidget",
  "Name": "MatchInfo",
  "WidgetClass": "/Script/P_MiniFootball.MF_MatchInfo",
  "SlotType": "CanvasPanel"
}
```

Resolution rules:

1. If `WidgetClass` is provided, MWCS loads it.
2. Otherwise, MWCS may infer the class from binding metadata.
3. Final fallback is `UUserWidget`.

## Editor UI

MWCS adds a tool tab:

- **Tools → MWCS**

It exposes buttons for Validate / Create Missing / Repair / ForceRecreate / Tool EUW, and prints a summarized report.

## Commandlets (CI / Headless)

Commandlets:

- `MWCS_ValidateWidgets`
- `MWCS_CreateWidgets` (`-Mode=CreateMissing|Repair|ForceRecreate`, default `CreateMissing`)

Exit behavior:

- `-FailOnErrors` → exit code 1 if any errors
- `-FailOnWarnings` → exit code 2 if any warnings

Recommended commandlet invocation:

```bat
Engine\Binaries\Win64\UnrealEditor-Cmd.exe "D:\Projects\UE\A_MiniFootball\A_MiniFootball.uproject" -run=MWCS_CreateWidgets -Mode=ForceRecreate -FailOnErrors -FailOnWarnings -unattended -nop4 -NullRHI -stdout -FullStdOutLogOutput

Engine\Binaries\Win64\UnrealEditor-Cmd.exe "D:\Projects\UE\A_MiniFootball\A_MiniFootball.uproject" -run=MWCS_ValidateWidgets -FailOnErrors -FailOnWarnings -unattended -nop4 -NullRHI -stdout -FullStdOutLogOutput
```

## Reports

Every Validate/Build run writes a machine-readable report to:

- `Saved/MWCS/Reports/MWCS_<Label>_<Timestamp>.json`

## Supported widget types

MWCS supports an explicit mapping from node `Type` → UMG class:

- Containers: `CanvasPanel`, `VerticalBox`, `HorizontalBox`, `Overlay`, `Border`, `ScrollBox`, `WidgetSwitcher`
- Controls: `Button`, `TextBlock`, `Image`, `Spacer`, `Throbber`, `MultiLineEditableTextBox`
- Nested: `UserWidget` (with `WidgetClass`)

Unsupported types are reported as build/validation errors.

## Author

Punal Manalan
