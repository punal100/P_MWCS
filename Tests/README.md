# MWCS Test Suite
This directory contains automated tests for the MWCS plugin.

## Tests

### RunExtractionParityTest.ps1
Tests bidirectional extraction parity for Image and Throbber widgets.

**Usage:**
```powershell
# Auto-detect project and UE paths (run from anywhere in project)
.\RunExtractionParityTest.ps1

# Specify paths explicitly
.\RunExtractionParityTest.ps1 -ProjectPath "D:\Projects\MyProject\MyProject.uproject" -UEPath "D:\UE\UE_5.5"
```

**What it tests:**
- ✅ Widget creation from C++ specs (ForceRecreate)
- ✅ Widget validation (no errors/warnings)
- ✅ Extraction of `Brush.DrawAs` for Image widgets
- ✅ Extraction of `NumberOfPieces` and `Image` brush for Throbber widgets

**Requirements:**
- P_MWCS plugin enabled in project
- At least one widget spec provider registered
- Widgets with Image/Throbber design properties (for full coverage)

## Adding New Tests

To add more test scripts:
1. Follow the naming convention: `Run[TestName].ps1`
2. Support `-ProjectPath` and `-UEPath` parameters with auto-detection
3. Use MWCS commandlets for testing (`MWCS_CreateWidgets`, `MWCS_ValidateWidgets`, etc.)
4. Update this README with test description
