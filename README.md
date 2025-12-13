﻿# P_MWCS (Modular Widget Creation System)

P_MWCS is a **standalone Unreal Engine editor-only plugin** that **fully replaces Python** for spec-driven Widget Blueprint creation, repair, and validation.

Core idea: your C++ widget classes expose a single source of truth via a static `GetWidgetSpec()` JSON string. MWCS consumes that JSON deterministically to create/repair/validate UMG Widget Blueprints (and a generated tooling EUW).

## Requirements

- Unreal Engine: this project currently builds against UE 5.5 (source build)
- Editor-only: the module is `Type: Editor` and does not ship in packaged builds
- No Python: MWCS does not use Python or require the Python Editor Script Plugin

## What MWCS does

- **Discover specs** from an allowlist of C++ “spec provider” classes
- **Validate** existing assets against the spec (parent class, required named widgets, hierarchy)
- **Create Missing** widget blueprints without touching existing assets
- **Repair** existing widget blueprints non-destructively (adds missing required elements; does not delete unknown user content)
- **Force Recreate** deletes and fully rebuilds assets deterministically (explicit action)
- **Generate/Repair Tool EUW**: creates an Editor Utility Widget Blueprint from a dedicated tool spec provider
- **CI/headless support** via commandlets with non-zero exits on failure

All operations:

- Build/repair the `WidgetTree`
- Compile synchronously
- Save the package

This avoids “open asset once to finalize” timing issues.

## Install / Enable

1. Ensure the plugin folder is at:
   - `A_MiniFootball/Plugins/P_MWCS/`
2. Enable **P_MWCS** in the Plugins window.
3. Restart the editor.

## Spec provider contract

MWCS expects spec provider classes to implement a static function:

```cpp
UFUNCTION(BlueprintCallable, BlueprintPure)
static FString GetWidgetSpec();
```

The JSON must minimally include:

- `BlueprintName` (string)
- `ParentClass` (string, soft class path like `/Script/YourModule.YourWidgetClass`)
- `Hierarchy` (tree)
- `Bindings` (object with required/optional entries)
- `Version` (int)

Unknown fields are allowed and ignored.

## Settings

MWCS settings are implemented via `UMWCS_Settings : UDeveloperSettings`.
In **Project Settings**, search for “MWCS” and configure:

- **Spec Provider Classes (Allowlist)**: list of C++ classes whose `GetWidgetSpec()` will be called
- **Widget Blueprint Output Root**: long package path for generated WBPs (default: `/Game/UI/Widgets`)
- **Tool EUW Output Path**: long package path for the generated tooling EUW (default: `/Game/Editor/MWCS`)
- **Tool EUW Asset Name**: default `EUW_MWCS_Tool`
- **Tool EUW Spec Provider Class**: default `/Script/P_MWCS.MWCS_ToolWidgetSpecProvider`

## Editor UI (Slate Tool Tab)

MWCS adds a menu entry under the editor Tools menu:

- **Tools → MWCS**

This opens a dockable tab with buttons:

- Validate
- Create Missing
- Repair
- Force Recreate
- Generate/Repair Tool EUW

The tab includes a read-only log that prints a summary plus any issues found.

## Generated Tool EUW

The **Generate/Repair Tool EUW** button generates an **Editor Utility Widget Blueprint** from JSON (the tool spec provider’s `GetWidgetSpec()`).

Milestone note:

- The EUW’s **designer hierarchy/layout is generated deterministically**.
- Button click wiring into MWCS can be manual initially (graph automation can be added later without introducing Python).

## Commandlets (CI / Headless)

MWCS provides commandlets:

- `MWCS_ValidateWidgets` (class: `UMWCS_ValidateWidgetsCommandlet`)
- `MWCS_CreateWidgets` (class: `UMWCS_CreateWidgetsCommandlet`)

Common flags:

- `-FailOnErrors` (exit code 1 if errors)
- `-FailOnWarnings` (exit code 2 if warnings)
- `-Mode=CreateMissing|Repair|ForceRecreate` (Create commandlet only; default is CreateMissing)

Example (from Engine root):

```bat
Engine\Binaries\Win64\UnrealEditor-Cmd.exe "D:\Projects\UE\A_MiniFootball\A_MiniFootball.uproject" -run=MWCS_ValidateWidgets -FailOnErrors

Engine\Binaries\Win64\UnrealEditor-Cmd.exe "D:\Projects\UE\A_MiniFootball\A_MiniFootball.uproject" -run=MWCS_CreateWidgets -Mode=Repair -FailOnErrors
```

## Reports

Every Validate/Build/ToolEUW run writes a machine-readable report to:

- `Saved/MWCS/Reports/MWCS_<Label>_<Timestamp>.json`

## Supported widget types (builder mapping)

The builder currently supports a conservative, explicit mapping from spec node type → UMG widget class:

- `CanvasPanel`, `VerticalBox`, `HorizontalBox`, `Overlay`, `Border`
- `Button`, `TextBlock`, `Image`, `Spacer`
- `MultiLineEditableTextBox`

Unsupported types are reported as validation/build errors.

## Author

Punal Manalan
