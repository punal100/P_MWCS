# P_MWCS Guide

Quick checklist for using MWCS (Modular Widget Creation System).

For full details, see [README.md](./README.md).

## 1) Register spec providers

MWCS discovers specs from an allowlist of C++ classes.

In this project, that list is configured in `Config/DefaultEditor.ini` under:

- `[/Script/P_MWCS.MWCS_Settings]`
- `+SpecProviderClasses=/Script/<Module>.<Class>`

## 2) Choose output root

Set `OutputRootPath` (long package path). This project uses:

- `/Game/UI/Widgets`

## 3) Generate / repair widgets (headless)

Run commandlets:

```bat
Engine\Binaries\Win64\UnrealEditor-Cmd.exe "D:\Projects\UE\A_MiniFootball\A_MiniFootball.uproject" -run=MWCS_CreateWidgets -Mode=ForceRecreate -FailOnErrors -FailOnWarnings -unattended -nop4 -NullRHI -stdout -FullStdOutLogOutput
```

## 4) Validate widgets (headless)

```bat
Engine\Binaries\Win64\UnrealEditor-Cmd.exe "D:\Projects\UE\A_MiniFootball\A_MiniFootball.uproject" -run=MWCS_ValidateWidgets -FailOnErrors -FailOnWarnings -unattended -nop4 -NullRHI -stdout -FullStdOutLogOutput
```

## 5) Reports

Every run writes JSON reports to:

- `Saved/MWCS/Reports/`

If the log says an asset was created but you don’t see a `.uasset`, check the report for unsupported widget types or other failures.
