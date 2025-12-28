# P_MWCS Guide

Quick checklist for using MWCS (Modular Widget Creation System).

For full details, see [README.md](./README.md).

## 1) Register spec providers

MWCS discovers specs from an allowlist of C++ classes.

In this project, that list is configured in `Config/DefaultEditor.ini` under:

- `[/Script/P_MWCS.MWCS_Settings]`
- `+SpecProviderClasses=/Script/<Module>.<Class>`

Ordering tip:

- Providers are processed in list order.
- Prefer ordering from “leaf” widgets (buttons/panels) → overlays/settings → containers (HUD/Menu) so nested widget classes exist when containers are generated.

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

## 6) Extract spec from a Widget Blueprint

If you need to capture a Widget Blueprint’s current hierarchy as JSON (to help update C++ `GetWidgetSpec()`), use the MWCS Tool EUW:

- Select a `WBP_*` asset in the Content Browser
- Open **Tools → MWCS**
- Click **Extract Selected WBP**

It writes to `Saved/MWCS/ExtractedSpecs/` and copies JSON to the clipboard.

### What gets exported (schema parity)

The exporter is shaped to match the project’s `GetWidgetSpec()` conventions for the supported parity set:

- `DesignerPreview` (e.g. `SizeMode`, `CustomSize`, plus best-effort UX hints like `ZoomLevel` and `ShowGrid`)
- `Hierarchy` (wrapped as `Hierarchy.Root`)
- `Design` (root object keyed by widget name)
- `Dependencies` (best-effort string array of referenced object paths)

MWCS validation also checks this same supported parity set (plus ParentClass and required widgets).

Write-only sections like `Bindings` / `Delegates` are intentionally ignored for extraction parity.

## Architecture: Why Hierarchy AND Design?

MWCS specs are split into **Hierarchy** (structure) and **Design** (appearance) sections. This is intentional.

### Quick Summary

- **Hierarchy** = WHAT widgets exist + WHERE they are (tree structure, slots, layout)
- **Design** = HOW widgets look (colors, fonts, brushes, text content)

Think: **HTML + CSS**
- Hierarchy ≈ HTML (structure)
- Design ≈ CSS (styling)

### Why Separate?

#### ✅ Clear Separation of Concerns

```json
// Hierarchy: Structure (widget tree)
"Hierarchy": {
    "Type": "Image",
    "Name": "Background",
    "Slot": {"Anchors": "fill"}
}

// Design: Appearance (styling)
"Design": {
    "Background": {
        "ColorAndOpacity": {"R": 0, "G": 0, "B": 0, "A": 0.85},
        "Brush": {"DrawAs": "Box"}
    }
}
```

#### ✅ Optimized Access Patterns

- **Hierarchy**: Recursive tree traversal (create widgets)
- **Design**: Name-based lookup (apply styling by widget name)
- Each section optimized for its use case

#### ✅ Cleaner Specs

- Hierarchy: Scan to understand widget composition
- Design: Jump alphabetically to find widget styling
- Mixing them clutters the tree with visual details

#### ✅ Partial Widget Support

- Not all widgets need Design (e.g., `CanvasPanel`, `VerticalBox`)
- Hierarchy defines structure; Design is opt-in for customization

### Why NOT Merge?

Merging would:
- ❌ Clutter hierarchy with visual noise
- ❌ Require navigating deep trees to update styling
- ❌ Increase code complexity (create + style in one pass)
- ❌ Break existing 50+ widget specs

### Real-World Analogy

**Current (HTML + CSS - Standard)**:
```html
<div><h1 class="title">Hello</h1></div>  <!-- Structure -->
.title { color: blue; }                    <!-- Style -->
```

**Merged (Inline Styles - Anti-Pattern)**:
```html
<h1 style="color: blue;">Hello</h1>  <!-- Mixed! -->
```

### Conclusion

The **Hierarchy/Design split is intentional and beneficial**. It follows industry patterns (HTML/CSS), optimizes for different use cases, and keeps specs clean and maintainable.

**For detailed rationale, code examples, and migration cost analysis, see [README.md - Architecture section](./README.md#architecture-hierarchy-vs-design-separation).**

---

## 7) A_WCG: HTML → Widget Conversion

P_MWCS includes **A_WCG** (Atomic Web Component Generator) as a submodule for converting web pages to widget specs.

### Quick Start

```powershell
cd Plugins/P_MWCS/A_WCG

# Build the CLI tool
.\Scripts\Build.ps1

# Fetch a website
.\Scripts\Fetch.ps1 -Url "https://example.com"

# Generate preview (builds, converts, opens in browser)
.\Scripts\RunPreview.ps1 -Name "example_com"
```

### Workflow: HTML to Widget Blueprint

```
┌─────────────────┐     ┌──────────────┐     ┌─────────────────┐
│  HTML/CSS File  │ ──► │   A_WCG CLI  │ ──► │  JSON + C++ Spec │
└─────────────────┘     └──────────────┘     └─────────────────┘
                                                      │
                              ┌───────────────────────┘
                              ▼
                    ┌─────────────────┐     ┌─────────────────┐
                    │  MWCS Commandlet │ ──► │ Widget Blueprint │
                    └─────────────────┘     └─────────────────┘
```

### Current Support Status

| Feature | Status | Notes |
|---------|--------|-------|
| HTML Structure | ✅ Full | All common elements mapped |
| CSS Colors/Fonts | ⚠️ Partial | Basic properties work |
| CSS Layout | ⚠️ Partial | Simple flexbox/padding |
| JavaScript | ❌ None | Strictly ignored |

### Generated Files

| File | Purpose |
|------|---------|
| `ClassName.json` | MWCS widget specification |
| `ClassName.h` | C++ header with UPROPERTY bindings |
| `ClassName.cpp` | GetWidgetSpec() with embedded JSON |
| `ClassName_preview.html` | Browser preview for comparison |

### Integration Steps

1. **Generate**: Run `.\Scripts\Convert.ps1 -Source file.html -ClassName MyWidget`
2. **Copy**: Move `.h`, `.cpp`, `.json` to `Source/YourModule/UI/`
3. **Register**: Add class to MWCS `SpecProviderClasses`
4. **Create**: Run `MWCS_CreateWidgets -Mode=ForceRecreate`

### Element Mapping Reference

| HTML | UMG Widget | Notes |
|------|-----------|-------|
| `<div>` | VerticalBox | TextBlock if text-only |
| `<h1>`-`<h6>` | TextBlock | With default font sizes |
| `<p>`, `<span>` | TextBlock | |
| `<a>`, `<button>` | Button | Transparent styling |
| `<img>` | Image | |
| `<ul>`, `<ol>` | VerticalBox | |
| `<li>` | HorizontalBox | |
| `<input>` | EditableText | |
| `<form>` | VerticalBox | |

### Tips

- **JavaScript content missing?** Use browser "Save As → Webpage, Complete" to capture rendered DOM
- **Layout broken?** Check generated JSON hierarchy matches expected structure
- **Text invisible?** A_WCG defaults to white text - ensure dark backgrounds

### Documentation

- Full A_WCG docs: [`A_WCG/README.md`](./A_WCG/README.md)
- A_WCG guide: [`A_WCG/GUIDE.md`](./A_WCG/GUIDE.md)
