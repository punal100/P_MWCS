# P_MWCS Testing Framework

## Overview

Automated modular testing for P_MWCS widget generation system.

### Features

- ✓ **Auto-Detection** - Automatically finds project and UE installation
- ✓ **Modular** - Works with ANY UE project using P_MWCS
- ✓ **CI-Ready** - Proper exit codes for CI/CD pipelines
- ✓ **Comprehensive** - Creation, extraction, round-trip testing
- ✓ **Detailed Logging** - Clear pass/fail diagnostics

## Quick Start

### Run All Tests (Auto-Detect)

```powershell
cd Plugins/P_MWCS/Tests
.\RunMWCSTests.ps1
```

**Exit Codes:**
- `0` = All tests passed
- `1` = Some tests failed

### Run Specific Test Suite

```powershell
# Creation tests only
.\RunMWCSTests.ps1 -TestSuite Creation

# Extraction tests only
.\RunMWCSTests.ps1 -TestSuite Extraction

# Round-trip tests only
.\RunMWCSTests.ps1 -TestSuite RoundTrip
```

### Run with Explicit Paths

```powershell
.\RunMWCSTests.ps1 `
  -ProjectPath "D:\MyGame" `
  -UEPath "C:\Program Files\Epic Games\UE_5.5" `
  -TestSuite All
```

## Test Suites

### 1. Widget Creation
- Create widgets from specs
- Validate generated assets
- **Success**: No errors, all widgets compile

### 2. Widget Extraction
- Extract specs from generated code
- Compare against original specs
- **Success**: Extraction completes without errors

### 3. Round-Trip
- Spec → Code generation → Validation
- Validates full workflow integrity
- **Success**: Generate and validate pass

## Module Architecture

```
Tests/
├── Modules/
│   ├── MWCS-Common.psm1      # Utilities, detection
│   └── MWCS-Commandlets.psm1 # Commandlet wrappers
├── TestSpecs/                # Test JSON specs
├── RunMWCSTests.ps1          # Master runner
└── README.md
```

### MWCS-Common.psm1
- `Find-ProjectRoot` - Auto-detect .uproject
- `Find-UnrealEngine` - Auto-detect UE installation
- `Write-TestLog` - Formatted test logging
- `Test-MWCSPluginEnabled` - Verify plugin enabled

### MWCS-Commandlets.psm1
- `Invoke-MWCSCreateWidgets` - Create widgets
- `Invoke-MWCSValidateWidgets` - Validate widgets
- `Invoke-MWCSExtractWidgets` - Extract specs

## CI/CD Integration

### GitHub Actions

```yaml
name: MWCS Tests
on: [push, pull_request]

jobs:
  test:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v2
      - name: Run MWCS Tests
        run: .\Plugins\P_MWCS\Tests\RunMWCSTests.ps1 -TestSuite All
```

## Troubleshooting

### "Could not find .uproject file"
Run from project root or specify `-ProjectPath`:
```powershell
.\RunMWCSTests.ps1 -ProjectPath "C:\Path\To\Project"
```

### "Could not find Unreal Engine installation"
Specify UE path explicitly:
```powershell
.\RunMWCSTests.ps1 -UEPath "D:\UE\UE_S"
```

### "P_MWCS plugin not enabled"
Enable P_MWCS in your .uproject or add plugin to Plugins directory.

---

**Version:** 1.0  
**Date:** December 21, 2025
