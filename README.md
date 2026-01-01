﻿# P_MWCS (Modular Widget Creation System)

P_MWCS is an Unreal Engine **editor-only** plugin that deterministically **creates / repairs / validates** Widget Blueprints from JSON specs (no Python).

For a quick checklist, see [GUIDE.md](./GUIDE.md).

The source of truth is a C++ `static FString GetWidgetSpec()` on “spec provider” classes. MWCS discovers these providers from an allowlist, parses the JSON into typed structs, then rebuilds the UMG `WidgetTree` deterministically.

## Requirements

- Unreal Engine 5.5 (source build)
- Editor-only module (`Type: Editor`) — not included in packaged builds
- No Python dependency

---

## A_WCG Submodule (HTML → Widget Conversion)

P_MWCS includes **A_WCG** (Atomic Web Component Generator) as a Git submodule at `A_WCG/`.

A_WCG is a standalone C++ CLI tool that converts HTML/CSS web pages into MWCS-compatible widget specifications.

### Current Status (v1.0)

| Feature | Status |
|---------|--------|
| **HTML Structure** | ✅ Fully Supported |
| **CSS Support** | ⚠️ Partial/Experimental (colors, fonts, basic layout) |
| **JavaScript** | ❌ Not Supported (strictly ignored) |

### What A_WCG Generates

From a single HTML file, A_WCG produces:

1. **JSON Spec** (`ClassName.json`) - MWCS widget specification
2. **C++ Header** (`ClassName.h`) - UObject class with `UPROPERTY` bindings
3. **C++ Source** (`ClassName.cpp`) - `GetWidgetSpec()` implementation with embedded JSON
4. **Preview HTML** (`ClassName_preview.html`) - Browser preview for visual comparison

### Quick Usage

```powershell
# Navigate to A_WCG
cd Plugins/P_MWCS/A_WCG

# 1. Build the tool
.\Scripts\Build.ps1 -Configuration Release

# 2. Fetch a website (downloads HTML/CSS/JS)
.\Scripts\Fetch.ps1 -Url "https://example.com" -Name "example_com"

# 3. Generate preview (builds, converts, opens preview)
.\Scripts\RunPreview.ps1 -Name "example_com"

# 4. Convert only (generates .json, .h, .cpp, _preview.html)
.\Scripts\Convert.ps1 -Source ".\fetched\example_com.html" -ClassName "ExampleWidget"
```

### Generated Widget Hierarchy

A_WCG wraps web content in a scrollable structure:

```
RootCanvas (CanvasPanel)
└── ContentScroll (ScrollBox) ← Enables scrolling
    └── ContentRoot (VerticalBox) ← Flow layout
        └── [HTML Content as UMG widgets]
```

### Element Mapping

| HTML Element | UMG Widget |
|--------------|------------|
| `<div>` | VerticalBox (or TextBlock if text-only) |
| `<p>`, `<span>`, `<h1>`-`<h6>` | TextBlock |
| `<a>`, `<button>` | Button |
| `<img>` | Image |
| `<ul>`, `<ol>` | VerticalBox |
| `<li>` | HorizontalBox |
| `<input>` | EditableText |

### Limitations

- **JavaScript content not captured** - A_WCG parses static HTML only; dynamically-rendered content requires a browser "Save As Complete" snapshot
- **CSS mapping partial** - Complex layouts (flexbox, grid, animations) may not translate accurately
- **Best for simple UIs** - Forms, menus, documentation pages translate well; complex web apps may require manual adjustment

### Integration with MWCS

After generating files:

1. Copy `ClassName.h`, `ClassName.cpp`, `ClassName.json` to your project's Source folder
2. Add the class to `SpecProviderClasses` in MWCS settings
3. Run `MWCS_CreateWidgets` commandlet to generate the Widget Blueprint

For detailed A_WCG documentation, see [`A_WCG/README.md`](./A_WCG/README.md) and [`A_WCG/GUIDE.md`](./A_WCG/GUIDE.md).

---

## What MWCS does

- **Discover specs** from configured spec providers
- **Validate** assets against the supported parity set (parent class, required widgets, `DesignerPreview`, `Hierarchy`, `Design`, and best-effort `Dependencies` path validity)
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
- **Tool EUW Output Path / Name / Spec Provider Class**: MWCS's own Editor Utility Widget
- **External Tool EUWs**: Modular array for external plugins to register their own Tool EUWs (see below)

### External Tool EUWs

Any plugin can generate its own Editor Utility Widget through MWCS without MWCS needing to know about it. Configure in `DefaultEditor.ini`:

```ini
[/Script/P_MWCS.MWCS_Settings]
+ExternalToolEuws=(ToolName="MyPlugin",OutputPath="/Game/Editor/MyPlugin",AssetName="EUW_MyPlugin_Tool",SpecProviderClass="/Script/MyPlugin.MyToolWidgetSpec")
```

The plugin then calls: `FMWCS_Service::Get().GenerateOrRepairExternalToolEuw(TEXT("MyPlugin"))`


Note on ordering:

- MWCS processes `SpecProviderClasses` in the order they are listed.
- If your specs reference nested widgets (e.g. `Type: "UserWidget"` with `WidgetClass`), it’s best to list “leaf” widgets first, then container widgets (HUD/Menu/etc.) so dependencies exist when the container WBPs are built.

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
  "ShowGrid": true
}
```

Notes:

- `SizeMode` / `CustomSize` are applied strictly.
- `ZoomLevel` / `ShowGrid` are treated as best-effort UX hints (engine versions may not persist these per-asset).

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

## Spec Format Reference

This section documents all accepted JSON formats for `GetWidgetSpec()`.

### Slot Configuration

Slots define how a widget is positioned/sized within its parent container.

#### CanvasPanel Slots

For children of `CanvasPanel`, use canvas slot properties:

```json
"Slot": {
    "Anchors": {"Min": {"X": 0.5, "Y": 0}, "Max": {"X": 0.5, "Y": 0}},
    "Offsets": {"Left": 0, "Top": 50, "Right": 0, "Bottom": 0},
    "Alignment": {"X": 0.5, "Y": 0},
    "AutoSize": true,
    "ZOrder": 0
}
```

**Alternative format** - Position+Size (converted to Offsets internally):

```json
"Slot": {
    "Anchors": {"Min": {"X": 0.5, "Y": 0.5}, "Max": {"X": 0.5, "Y": 0.5}},
    "Position": {"X": 0, "Y": 0},
    "Size": {"X": 800, "Y": 600},
    "Alignment": {"X": 0.5, "Y": 0.5}
}
```

**Offsets array format** (Left, Top, Right, Bottom):

```json
"Slot": {
    "Anchors": {"Min": {"X": 0, "Y": 0}, "Max": {"X": 1, "Y": 1}},
    "Offsets": [0, 0, 0, 0]
}
```

#### HorizontalBox / VerticalBox Slots

For children of box containers:

```json
"Slot": {
    "HAlign": "Fill",
    "VAlign": "Center",
    "Padding": {"Left": 10, "Top": 5, "Right": 10, "Bottom": 5},
    "Size": {"Rule": "Auto"}
}
```

**Size.Rule values:**
- `"Auto"` - Widget sizes to content (ESlateSizeRule::Automatic)
- `"Fill"` - Widget fills available space (ESlateSizeRule::Fill)
- With Fill, use `"Value": 1.0` for fill weight

**Padding formats** (all equivalent):
```json
"Padding": {"Left": 10, "Top": 5, "Right": 10, "Bottom": 5}
"Padding": [10, 5, 10, 5]
"Padding": 10
```

**Alignment values:** `"Left"`, `"Center"`, `"Right"`, `"Fill"` (for HAlign)  
**VAlign values:** `"Top"`, `"Center"`, `"Bottom"`, `"Fill"`

#### Overlay Slots

For children of `Overlay`:

```json
"Slot": {
    "HAlign": "Fill",
    "VAlign": "Fill",
    "Padding": 0
}
```

### Properties Section

`Properties` defines widget-specific settings (NOT slot settings):

```json
{
    "Type": "VerticalBox",
    "Name": "ContentBox",
    "Properties": {
        "SizeToContent": true,
        "Spacing": 8
    }
}
```

**Do NOT confuse with Slot** - Properties are widget settings, Slot is parent layout.

### Architecture: Hierarchy vs Design Separation

MWCS intentionally separates widget specifications into two distinct sections: **Hierarchy** and **Design**. This architectural decision follows industry-proven patterns and provides significant benefits for maintainability and clarity.

#### Why Two Sections?

**Hierarchy Section** defines widget **STRUCTURE**:
- What widgets exist
- Parent-child relationships (tree composition)
- Widget types (`Image`, `TextBlock`, `VerticalBox`, etc.)
- Widget names for C++ bindings
- Slot/layout properties (Anchors, Position, Padding, Alignment)

**Design Section** defines widget **APPEARANCE**:
- Visual properties (ColorAndOpacity, Font, Size)
- Brush configuration (DrawAs, TintColor, Tiling, Margin)
- Text content and formatting
- Widget-specific behavior properties

**This separation mirrors the HTML + CSS pattern from web development:**

```html
<!-- HTML: Structure (analogous to Hierarchy) -->
<div class="container">
    <h1 class="title">Hello</h1>
</div>

<!-- CSS: Appearance (analogous to Design) -->
.title {
    font-size: 24px;
    color: blue;
}
```

#### Benefits of Current Architecture ✅

**1. Clarity of Intent**

Current (separated):
```json
"Hierarchy": {
    "Type": "Image",
    "Name": "Background",
    "Slot": {"Anchors": "fill"}
}
"Design": {
    "Background": {
        "ColorAndOpacity": {"R": 0, "G": 0, "B": 0, "A": 0.85},
        "Brush": {"DrawAs": "Box"}
    }
}
```

Alternative (merged - NOT recommended):
```json
"Hierarchy": {
    "Type": "Image",
    "Name": "Background",
    "Slot": {"Anchors": "fill"},
    "ColorAndOpacity": {...},  // Visual property mixed with structure!
    "Brush": {...}
}
```

**2. Spec Readability**

- **Hierarchy**: Scan top-to-bottom to understand widget tree structure
- **Design**: Scan alphabetically by widget name to find styling
- Mixing them clutters hierarchical view with visual details

**3. Different Access Patterns (Optimized)**

- **Hierarchy**: Recursive tree traversal during widget creation
- **Design**: Direct lookup by widget name `DesignMap[widgetName]`
- Each section optimized for its specific use case

**4. Partial Widget Support**

- Not all widgets need Design properties (containers like `CanvasPanel`, `VerticalBox`)
- Hierarchy defines required structure
- Design is opt-in for visual customization
- Empty Design section doesn't clutter Hierarchy

**5. Separation of Concerns**

| Aspect | Hierarchy | Design |
|--------|-----------|--------|
| **Concern** | Structure | Appearance |
| **Analogy** | HTML DOM | CSS Styling |
| **Focus** | What widgets exist | How widgets look |
| **Typical Changes** | Add/remove widgets | Adjust colors/fonts |
| **Lookup Pattern** | Tree traversal | Name-based dictionary |

#### Why NOT Merge Them? ❌

**Con 1: Cluttered Hierarchy View**

Merging visual properties into the tree structure makes it harder to understand widget composition:

```json
// Current: Clean structure
"Children": [
    {"Type": "VBox", "Name": "ContentBox"},
    {"Type": "TextBlock", "Name": "StatusText"}
]

// Merged: Visual noise
"Children": [
    {
        "Type": "VBox",
        "Name": "ContentBox",
        "Padding": {...},
        "SomeProperty": "..."
    },
    {
        "Type": "TextBlock",
        "Name": "StatusText",
        "Font": {"Size": 24, "Typeface": "Regular"},
        "ColorAndOpacity": {"R": 1, "G": 1, "B": 1, "A": 1},
        "Text": "LOADING...",
        "Justification": "Center"
        // Can't see structure through the styling!
    }
]
```

**Con 2: Harder to Update Appearance**

Current: Jump to Design section, find widget by name (easy alphabetical lookup)
```json
"Design": {
    "BackgroundOverlay": {...},  // Easy to locate
    "StatusText": {...}
}
```

Merged: Must navigate tree structure to find widget (deep nesting)
```json
"Hierarchy": {
    "Children": [
        {"Children": [    // Navigate tree depth
            {"Children": [
                {"Name": "StatusText", "Font": ...}  // Finally!
            ]}
        ]}
    ]
}
```

**Con 3: Code Complexity Increase**

Current (simple):
```cpp
// Create all widgets from Hierarchy
CreateWidgetsRecursive(HierarchyObj);

// Apply design to all widgets
for (auto& [WidgetName, DesignObj] : DesignMap) {
    ApplyDesignMeta(GetWidget(WidgetName), DesignObj);
}
```

Merged (complex - would require):
```cpp
// Create widgets AND apply design during same traversal
CreateWidgetsRecursive(HierarchyObj, bApplyDesignToo);
// OR: Store created widgets and re-traverse
// Both approaches more complex!
```

**Con 4: Duplicate Data Problem**

Some properties could appear in BOTH sections with different purposes:
- `"Text"` in Hierarchy (default) vs Design (designer-set value)
- `"FontSize"` in Hierarchy (shorthand) vs Design.Font.Size (detailed config)

Merging would require complex rules about which takes precedence.

#### Real-World Comparison

**MWCS = HTML + CSS (Industry Standard)**
```html
<!-- Structure: Hierarchy -->
<div class="container">
    <h1 class="title">Hello</h1>
</div>

<!-- Appearance: Design -->
.title { font-size: 24px; color: blue; }
```

**Merged = Inline Styles (Anti-Pattern)**
```html
<!-- Everything mixed (harder to maintain) -->
<div class="container">
    <h1 style="font-size: 24px; color: blue;">Hello</h1>
</div>
```

#### Migration Cost (If Merging Were Pursued)

**NOT recommended**, but for reference:
- Rewrite: MWCS_WidgetBuilder.cpp + MWCS_ToolEUW.cpp extraction/application logic
- Update: All 50+ widget GetWidgetSpec() functions
- Risk: Breaking all existing specs
- Effort: ~1 week development + testing
- **ROI**: Negative (saves ~10% JSON lines, costs 1 week + breaks everything)

#### Conclusion

The **Hierarchy vs Design separation is intentional and beneficial**:

- ✅ Clear separation of concerns (structure vs appearance)
- ✅ Follows industry-standard HTML/CSS pattern
- ✅ Optimized for different access patterns
- ✅ Easier spec readability and maintenance
- ✅ No compelling benefit to justify merging

**This architecture is designed to stay.**

### Design Section


The `Design` section applies styling to named widgets:

```json
"Design": {
    "TitleText": {
        "Font": {"Size": 24, "Typeface": "Bold"},
        "Text": "PAUSED",
        "ColorAndOpacity": {"R": 1.0, "G": 1.0, "B": 1.0, "A": 1.0}
    },
    "ActionButton": {
        "Style": {
            "Normal": {"TintColor": {"R": 0.2, "G": 0.5, "B": 0.2, "A": 1.0}},
            "Hovered": {"TintColor": {"R": 0.3, "G": 0.6, "B": 0.3, "A": 1.0}},
            "Pressed": {"TintColor": {"R": 0.15, "G": 0.4, "B": 0.15, "A": 1.0}}
        },
        "IsFocusable": true
    },
    "PanelBorder": {
        "BrushColor": {"R": 0.1, "G": 0.1, "B": 0.1, "A": 0.9},
        "Padding": {"Left": 15, "Top": 10, "Right": 15, "Bottom": 10}
    }
}
```

**TextBlock properties:** `Font`, `Text`, `ColorAndOpacity`, `Justification`  
**Button properties:** `Style` (Normal/Hovered/Pressed), `IsFocusable`  
**Border properties:** `BrushColor`, `Padding`, `Background`  
**Image properties:** `ColorAndOpacity`, `Brush`

### Dependencies Section

Dependencies is an array of asset paths referenced by the spec:

**String format (asset paths):**
```json
"Dependencies": [
    "/Engine/EngineFonts/Roboto.Roboto",
    "/Game/UI/Textures/T_ButtonBG.T_ButtonBG"
]
```

**Object format (widget class dependencies):**
```json
"Dependencies": [
    {"Class": "UMF_TeamPanel", "Blueprint": "WBP_MF_TeamPanel", "Required": true},
    {"Class": "UMF_QuickTeamPanel", "Blueprint": "WBP_MF_QuickTeamPanel", "Required": false}
]
```

**Mixed format:**
```json
"Dependencies": [
    "/Engine/EngineFonts/Roboto.Roboto",
    {"Class": "UMF_TeamPanel", "Blueprint": "WBP_MF_TeamPanel", "Required": true}
]
```

### Complete Minimal Example

```json
{
    "WidgetClass": "UMyWidget",
    "BlueprintName": "WBP_MyWidget",
    "ParentClass": "/Script/MyModule.MyWidget",
    "Version": "1.0.0",
    
    "DesignerPreview": {
        "SizeMode": "Desired",
        "ZoomLevel": 14
    },
    
    "Hierarchy": {
        "Root": {
            "Type": "CanvasPanel",
            "Name": "RootCanvas",
            "Children": [
                {
                    "Type": "TextBlock",
                    "Name": "TitleText",
                    "BindingType": "Required",
                    "Text": "Hello",
                    "Slot": {
                        "Anchors": {"Min": {"X": 0.5, "Y": 0.5}, "Max": {"X": 0.5, "Y": 0.5}},
                        "Alignment": {"X": 0.5, "Y": 0.5}
                    }
                }
            ]
        }
    },
    
    "Design": {
        "TitleText": {
            "Font": {"Size": 24, "Typeface": "Bold"},
            "ColorAndOpacity": {"R": 1, "G": 1, "B": 1, "A": 1}
        }
    },
    
    "Bindings": {
        "Required": [
            {"Name": "TitleText", "Type": "UTextBlock", "Purpose": "Main title"}
        ],
        "Optional": []
    },
    
    "Dependencies": []
}
```

### Example: Button with Styled Text Child

```json
{
    "Type": "Button",
    "Name": "ActionButton",
    "BindingType": "Required",
    "Slot": {"HAlign": "Center", "Padding": {"Bottom": 10}},
    "Children": [
        {
            "Type": "TextBlock",
            "Name": "ActionButtonLabel",
            "Text": "CLICK ME",
            "FontSize": 18,
            "Justification": "Center",
            "Slot": {"HAlign": "Center", "VAlign": "Center"}
        }
    ]
}
```

Design for button with all states:

```json
"Design": {
    "ActionButton": {
        "Style": {
            "Normal": {"TintColor": {"R": 0.2, "G": 0.5, "B": 0.2, "A": 1.0}},
            "Hovered": {"TintColor": {"R": 0.3, "G": 0.65, "B": 0.3, "A": 1.0}},
            "Pressed": {"TintColor": {"R": 0.15, "G": 0.4, "B": 0.15, "A": 1.0}}
        },
        "IsFocusable": true
    },
    "ActionButtonLabel": {
        "Font": {"Size": 18, "Typeface": "Bold"},
        "ColorAndOpacity": {"R": 1, "G": 1, "B": 1, "A": 1}
    }
}
```

### Example: VerticalBox Menu with Spacing

```json
{
    "Type": "VerticalBox",
    "Name": "MenuContainer",
    "Properties": {"SizeToContent": true, "Spacing": 8},
    "Children": [
        {
            "Type": "TextBlock",
            "Name": "MenuTitle",
            "Text": "MAIN MENU",
            "Slot": {"HAlign": "Center", "Padding": {"Bottom": 20}}
        },
        {
            "Type": "Button",
            "Name": "PlayButton",
            "Slot": {"HAlign": "Fill", "Padding": {"Left": 20, "Right": 20}},
            "Children": [{"Type": "TextBlock", "Name": "PlayLabel", "Text": "PLAY"}]
        },
        {
            "Type": "Button",
            "Name": "OptionsButton",
            "Slot": {"HAlign": "Fill", "Padding": {"Left": 20, "Right": 20}},
            "Children": [{"Type": "TextBlock", "Name": "OptionsLabel", "Text": "OPTIONS"}]
        },
        {
            "Type": "Button",
            "Name": "QuitButton",
            "Slot": {"HAlign": "Fill", "Padding": {"Left": 20, "Right": 20, "Top": 10}},
            "Children": [{"Type": "TextBlock", "Name": "QuitLabel", "Text": "QUIT"}]
        }
    ]
}
```

### Example: HUD with Anchored Elements

```json
"Hierarchy": {
    "Root": {
        "Type": "CanvasPanel",
        "Name": "HUDCanvas",
        "Children": [
            {
                "Type": "TextBlock",
                "Name": "ScoreText",
                "Text": "0 - 0",
                "Slot": {
                    "Anchors": {"Min": {"X": 0.5, "Y": 0}, "Max": {"X": 0.5, "Y": 0}},
                    "Offsets": {"Left": 0, "Top": 20, "Right": 0, "Bottom": 0},
                    "Alignment": {"X": 0.5, "Y": 0},
                    "AutoSize": true
                }
            },
            {
                "Type": "TextBlock",
                "Name": "TimeText",
                "Text": "00:00",
                "Slot": {
                    "Anchors": {"Min": {"X": 0.5, "Y": 0}, "Max": {"X": 0.5, "Y": 0}},
                    "Offsets": {"Left": 0, "Top": 60, "Right": 0, "Bottom": 0},
                    "Alignment": {"X": 0.5, "Y": 0},
                    "AutoSize": true
                }
            },
            {
                "Type": "HorizontalBox",
                "Name": "BottomBar",
                "Slot": {
                    "Anchors": {"Min": {"X": 0, "Y": 1}, "Max": {"X": 1, "Y": 1}},
                    "Offsets": {"Left": 10, "Top": -50, "Right": 10, "Bottom": 10},
                    "Alignment": {"X": 0, "Y": 1}
                },
                "Children": [
                    {"Type": "TextBlock", "Name": "LeftInfo", "Slot": {"Size": {"Rule": "Fill", "Value": 1}}},
                    {"Type": "TextBlock", "Name": "RightInfo", "Slot": {"HAlign": "Right"}}
                ]
            }
        ]
    }
}
```

### Example: Popup with Border and Nested UserWidget

```json
"Hierarchy": {
    "Root": {
        "Type": "CanvasPanel",
        "Name": "PopupRoot",
        "Children": [
            {
                "Type": "Image",
                "Name": "BackgroundDim",
                "Slot": {
                    "Anchors": {"Min": {"X": 0, "Y": 0}, "Max": {"X": 1, "Y": 1}},
                    "Offsets": {"Left": 0, "Top": 0, "Right": 0, "Bottom": 0}
                }
            },
            {
                "Type": "Border",
                "Name": "PopupBorder",
                "Slot": {
                    "Anchors": {"Min": {"X": 0.5, "Y": 0.5}, "Max": {"X": 0.5, "Y": 0.5}},
                    "Position": {"X": 0, "Y": 0},
                    "Size": {"X": 600, "Y": 400},
                    "Alignment": {"X": 0.5, "Y": 0.5}
                },
                "Children": [
                    {
                        "Type": "VerticalBox",
                        "Name": "PopupContent",
                        "Children": [
                            {
                                "Type": "HorizontalBox",
                                "Name": "Header",
                                "Slot": {"Size": {"Rule": "Auto"}},
                                "Children": [
                                    {"Type": "TextBlock", "Name": "Title", "Text": "POPUP", "Slot": {"Size": {"Rule": "Fill"}}},
                                    {"Type": "Button", "Name": "CloseBtn", "Children": [{"Type": "TextBlock", "Name": "CloseLbl", "Text": "X"}]}
                                ]
                            },
                            {
                                "Type": "UserWidget",
                                "Name": "ContentPanel",
                                "WidgetClass": "/Script/MyModule.MyContentWidget",
                                "Slot": {"Size": {"Rule": "Fill"}}
                            }
                        ]
                    }
                ]
            }
        ]
    }
},
"Design": {
    "BackgroundDim": {
        "ColorAndOpacity": {"R": 0, "G": 0, "B": 0, "A": 0.7}
    },
    "PopupBorder": {
        "BrushColor": {"R": 0.1, "G": 0.1, "B": 0.15, "A": 0.95},
        "Padding": {"Left": 20, "Top": 15, "Right": 20, "Bottom": 15}
    }
},
"Dependencies": [
    {"Class": "UMyContentWidget", "Blueprint": "WBP_MyContentWidget", "Required": true}
]
```

### Example: ScrollBox with Dynamic Content

```json
{
    "Type": "ScrollBox",
    "Name": "ItemScroll",
    "BindingType": "Required",
    "Slot": {"Size": {"Rule": "Fill"}},
    "Children": [
        {
            "Type": "VerticalBox",
            "Name": "ItemList",
            "BindingType": "Required",
            "Properties": {"SizeToContent": true, "Spacing": 4}
        }
    ]
}
```

Note: Dynamic content (like list items) should be added at runtime via C++/Blueprint code to the `ItemList` container.



## Field Naming & Conventions

### Alignment Values

| Value | Meaning |
|-------|---------|
| `"Left"` | HAlign left edge |
| `"Center"` | HAlign center |
| `"Right"` | HAlign right edge |
| `"Fill"` | HAlign to fill space |
| `"Top"` | VAlign top edge |
| `"Bottom"` | VAlign bottom edge |

### Size Rule Values

| Value | Meaning |
|-------|---------|
| `"Auto"` or `"Automatic"` | Automatic - size to content |
| `"Fill"` | Fill available space |

### Margin / Padding Format

| Format | Example | Note |
|--------|---------|------|
| Explicit Object | `{"Left": 10, "Top": 5, "Right": 10, "Bottom": 5}` | Most readable |
| Array | `[10, 5, 10, 5]` | Compact (L, T, R, B) |
| Single Number | `10` | Uniform padding |

### Write-Only Properties

These properties are documentation-only in the spec but implemented logically:

| Property | Widget | Implementation |
|----------|--------|----------------|
| `SizeToContent` | VerticalBox/HorizontalBox | Sets `Size.Rule = Auto` on all child slots |
| `Spacing` | VerticalBox/HorizontalBox | Sets `Padding` on child slots (except first) |

## Widget Types Reference

Complete support matrix for all 15 widget types.

### Container Widgets (3 Types)

#### CanvasPanel
- **Slot Type:** CanvasPanelSlot
- **Properties:** None (see slot)
- **Design Fields:** None
- **Slot Fields:**
  - `Anchors`: Min{X,Y}, Max{X,Y} - Default: {0,0} to {1,1}
  - `Offsets`: Left, Top, Right, Bottom OR `Position` + `Size`
  - `Position`: {X, Y} - Optional (alternative to Offsets)
  - `Size`: {X, Y} - Optional (alternative to Offsets)
  - `Alignment`: {X, Y} - 0-1, default {0.5, 0.5}
  - `ZOrder`: integer - Default: 0
  - `AutoSize`: boolean - Default: false

#### VerticalBox
- **Slot Type:** VerticalBoxSlot
- **Properties:** 
  - `SizeToContent`: boolean - INFERRED if all children Auto (write-only)
  - `Spacing`: float - INFERRED from uniform child padding (write-only)
- **Design Fields:** None
- **Slot Fields:**
  - `HAlign`: "Left"/"Center"/"Right"/"Fill"
  - `VAlign`: "Top"/"Center"/"Bottom"/"Fill"
  - `Padding`: {Left, Top, Right, Bottom} OR single number
  - `Size`: {Rule: "Auto"/"Automatic"/"Fill", Value: float}

#### HorizontalBox
- **Slot Type:** HorizontalBoxSlot
- **Properties:**
  - `SizeToContent`: boolean - INFERRED if all children Auto (write-only)
  - `Spacing`: float - INFERRED from uniform child padding (write-only)
- **Design Fields:** None
- **Slot Fields:** (Same as VerticalBox)

#### Overlay
- **Slot Type:** OverlaySlot
- **Properties:** None
- **Design Fields:** None
- **Slot Fields:**
  - `HAlign`: "Left"/"Center"/"Right"/"Fill"
  - `VAlign`: "Top"/"Center"/"Bottom"/"Fill"
  - `Padding`: {Left, Top, Right, Bottom} OR single number

#### Border
- **Slot Type:** BorderSlot
- **Properties:** None
- **Design Fields:**
  - `BrushColor`: {R, G, B, A}
  - `Padding`: {Left, Top, Right, Bottom}
- **Slot Fields:**
  - `HAlign`: "Left"/"Center"/"Right"/"Fill"
  - `VAlign`: "Top"/"Center"/"Bottom"/"Fill"
  - `Padding`: {Left, Top, Right, Bottom}

#### ScrollBox
- **Slot Type:** ScrollBoxSlot
- **Properties:**
  - `Orientation`: "Vertical"/"Horizontal" - Default: "Vertical"
  - `ScrollBarVisibility`: "Visible"/"Collapsed"/"Hidden"/"HitTestInvisible"
- **Design Fields:** None
- **Slot Fields:**
  - `HAlign`: "Left"/"Center"/"Right"/"Fill"
  - `VAlign`: "Top"/"Center"/"Bottom"/"Fill"
  - `Padding`: {Left, Top, Right, Bottom}
  - `Size`: {Rule: "Auto"/"Fill", Value: float}

### Leaf Widgets (12 Types)

#### Button (CanvasPanel/VerticalBox/HorizontalBox parent only)
- **Slot Type:** ButtonSlot
- **Properties:** None
- **Design Fields:**
  - `NormalTint`: {R, G, B, A} (Legacy) or `Style.Normal.TintColor`
  - `HoveredTint`: {R, G, B, A} (Legacy) or `Style.Hovered.TintColor`
  - `PressedTint`: {R, G, B, A} (Legacy) or `Style.Pressed.TintColor`
  - `DisabledTint`: {R, G, B, A} (Legacy) or `Style.Disabled.TintColor`
  - `IsFocusable`: boolean
- **Slot Fields (ButtonSlot-specific):**
  - `HAlign`: "Left"/"Center"/"Right"/"Fill"
  - `VAlign`: "Top"/"Center"/"Bottom"/"Fill"
  - `Padding`: {Left, Top, Right, Bottom}

#### TextBlock
- **Slot Type:** (Inherited - depends on parent)
- **Inline Properties:**
  - `Text`: String
  - `FontSize`: integer
  - `Justification`: "Left"/"Center"/"Right"
- **Design Fields:**
  - `Font`: {Size, Typeface}
  - `ColorAndOpacity`: {R, G, B, A}
  - `Text`: String (Override)

#### Image
- **Slot Type:** (Inherited - depends on parent)
- **Properties:** None
- **Design Fields:**
  - `ColorAndOpacity`: {R, G, B, A}
  - `Size`: {X, Y} (Legacy - sets Brush.ImageSize)
  - `Brush`: Complete FSlateBrush configuration
    - `DrawAs`: "Image"/"Box"/"Border"/"RoundedBox"/"NoDrawType"
    - `ImageSize`: {X, Y}
    - `TintColor`: {R, G, B, A}
    - `Tiling`: "NoTile"/"Horizontal"/"Vertical"/"Both"
    - `Margin`: {Left, Top, Right, Bottom}

**Example - Image with Box Brush:**
```json
"Design": {
    "BackgroundOverlay": {
        "Size": {"X": 32, "Y": 32},
        "ColorAndOpacity": {"R": 0, "G": 0, "B": 0, "A": 0.85},
        "Brush": {
            "DrawAs": "Box"
        }
    }
}
```

**Example - Image with Complete Brush Properties:**
```json
"Design": {
    "PatternBackground": {
        "Brush": {
            "DrawAs": "Image",
            "ImageSize": {"X": 128, "Y": 128},
            "TintColor": {"R": 0.5, "G": 0.5, "B": 1, "A": 1},
            "Tiling": "Both",
            "Margin": {"Left": 8, "Top": 8, "Right": 8, "Bottom": 8}
        }
    }
}
```

#### Spacer
- **Slot Type:** (Inherited - depends on parent)
- **Properties:**
  - `Size`: {X, Y}
- **Design Fields:** None

#### MultiLineEditableTextBox
- **Slot Type:** (Inherited)
- **Properties:** None (Partial Support)

#### Throbber
- **Slot Type:** (Inherited)
- **Properties:** None
- **Design Fields:**
  - `NumberOfPieces`: integer (default: 3, range: 1-25)
  - `bAnimateHorizontally`: boolean (default: true)
  - `bAnimateVertically`: boolean (default: true)
  - `bAnimateOpacity`: boolean (default: true)
  - `Image`: FSlateBrush (same as Image widget Brush)

**Example - Custom Throbber:**
```json
"Design": {
    "LoadingThrobber": {
        "NumberOfPieces": 8,
        "bAnimateVertically": false,
        "Image": {
            "ImageSize": {"X": 24, "Y": 24},
            "TintColor": {"R": 0, "G": 0.8, "B": 1, "A": 1}
        }
    }
}
```

**Example - Minimal Throbber:**
```json
"Design": {
    "SimpleThrobber": {
        "NumberOfPieces": 6
    }
}
```

#### ComboBoxString
- **Slot Type:** (Inherited)
- **Properties:** None (Partial Support)

#### WidgetSwitcher
- **Slot Type:** (Inherited)
- **Properties:** None

## Editor UI

MWCS adds a tool tab:

- **Tools → MWCS**

It exposes buttons for Validate / Create Missing / Repair / ForceRecreate / Tool EUW, and prints a summarized report.

### Extract Selected WBP (spec helper)

The MWCS Tool EUW also includes **Extract Selected WBP**, which exports the selected Widget Blueprint’s widget tree as a **spec-shaped JSON stub**.

Usage:

1. In the Content Browser, select a `WBP_*` Widget Blueprint.
2. Open **Tools → MWCS**.
3. Click **Extract Selected WBP**.

Output:

- Writes JSON to: `Saved/MWCS/ExtractedSpecs/<BlueprintName>.json`
- Copies the JSON to the clipboard

The exported JSON is shaped to match the project’s `GetWidgetSpec()` conventions for the supported parity set:

- `DesignerPreview`
- `Hierarchy` (wrapped as `Hierarchy.Root`)
- `Design` (root object keyed by widget name)
- `Dependencies` (best-effort string array of referenced asset/object paths)

Note:

- This export is intended as a starting point. MWCS intentionally ignores write-only / non-extractable sections like `Bindings`, `Delegates`, `Comments`, and `PythonSnippets` during extraction parity.

## Updating GetWidgetSpec from Extracted Data

When you have extracted JSON and need to update your C++ `GetWidgetSpec()` to match, follow these rules to avoid losing crucial data.

### Golden Rules

1. **DO NOT delete fields** - Only update values, never remove existing fields
2. **DO NOT change field order** - Preserve the existing structure
3. **DO NOT replace entire sections** - Update individual values within sections
4. **Preserve non-extractable sections** - `Bindings`, `Delegates`, `Comments`, `PythonSnippets` are write-only

### Parity Set (What Matters for Matching)

MWCS validates these sections for parity:
- `DesignerPreview`
- `Hierarchy`
- `Design`
- `Dependencies`

These sections are **write-only** (not extracted, do not modify):
- `Bindings`
- `Delegates`
- `Comments`
- `PythonSnippets`

### What to Update vs Preserve

| Extracted Has | C++ Has | Action |
|---------------|---------|--------|
| `Position`+`Size` | `Offsets` | **Keep Offsets** - both are equivalent |
| `Size: {"Rule": "Auto"}` | Nothing | **Add only if needed** for box slots |
| More `ColorAndOpacity` entries | Fewer entries | **Add the missing entries** |
| More button states | Only `Normal` | **Add `Hovered`/`Pressed`** if desired |
| Font dependency | Empty `Dependencies` | **Add to array**, don't replace |
| Different `Properties` | `Properties` | **Keep existing** `Properties` |

### Step-by-Step Workflow

1. **Extract the WBP**
   ```
   Tools → MWCS → Extract Selected WBP
   ```

2. **Compare sections** - Open extracted JSON and C++ spec side-by-side

3. **For DesignerPreview** - Update values if zoom/size differs:
   ```cpp
   // Before
   "ZoomLevel": 12
   // After (if extracted shows 14)
   "ZoomLevel": 14
   ```

4. **For Hierarchy** - Add missing slot properties, DON'T replace format:
   ```cpp
   // WRONG - replacing Offsets with Position+Size
   "Slot": {"Position": {"X": 0}, "Size": {"X": 100}}
   
   // CORRECT - keep Offsets, add missing fields
   "Slot": {"Offsets": {...}, "Alignment": {...}, "AutoSize": true}
   ```

5. **For Design** - Add missing entries, update values, never delete:
   ```cpp
   // If extracted has ColorAndOpacity but C++ doesn't, ADD it:
   "TitleText": {
       "Font": {"Size": 24, "Typeface": "Bold"},
       "ColorAndOpacity": {"R": 1, "G": 1, "B": 1, "A": 1}  // Added
   }
   ```

6. **For Dependencies** - Merge, don't replace:
   ```cpp
   // Before
   "Dependencies": [
       {"Class": "UMF_TeamPanel", "Blueprint": "WBP_MF_TeamPanel", "Required": true}
   ]
   
   // After (if extracted also has font)
   "Dependencies": [
       {"Class": "UMF_TeamPanel", "Blueprint": "WBP_MF_TeamPanel", "Required": true},
       "/Engine/EngineFonts/Roboto.Roboto"
   ]
   ```

7. **Test** - Run MWCS ForceRecreate + Validate:
   ```bat
   MWCS_CreateWidgets -Mode=ForceRecreate
   MWCS_ValidateWidgets
   ```

### Common Mistakes to Avoid

❌ **Replacing `Properties` with `Slot`**
```cpp
// WRONG - Properties and Slot serve different purposes
"Properties": {"Spacing": 8}  →  "Slot": {"Spacing": 8}

// Properties = widget settings (Spacing, SizeToContent)
// Slot = parent layout settings (HAlign, VAlign, Padding)
```

❌ **Removing child Slot attributes**
```cpp
// WRONG - Don't remove Slot from button's text child
"Children": [{"Type": "TextBlock", "Name": "Label", "Text": "OK"}]

// CORRECT - Keep the alignment
"Children": [{"Type": "TextBlock", "Name": "Label", "Text": "OK", 
              "Slot": {"HAlign": "Center", "VAlign": "Center"}}]
```

❌ **Replacing class dependencies with font-only**
```cpp
// WRONG
"Dependencies": ["/Engine/EngineFonts/Roboto.Roboto"]

// CORRECT - Keep class deps, add font
"Dependencies": [
    {"Class": "UMF_TeamPanel", "Blueprint": "WBP_MF_TeamPanel", "Required": true},
    "/Engine/EngineFonts/Roboto.Roboto"
]
```

❌ **Removing Text values from Design**
```cpp
// WRONG - Don't remove existing values
"TitleText": {"Font": {...}}

// CORRECT - Keep existing Text
"TitleText": {"Font": {...}, "Text": "TITLE"}
```

### Quick Reference: Format Equivalence

These formats are **equivalent** (MWCS accepts both):

| Format A | Format B |
|----------|----------|
| `"Offsets": {"Left": 0, "Top": 50, "Right": 0, "Bottom": 0}` | `"Position": {"X": 0, "Y": 50}, "Size": {"X": 0, "Y": 0}` |
| `"Offsets": [0, 50, 0, 0]` | `"Offsets": {"Left": 0, "Top": 50, ...}` |
| `"Padding": 10` | `"Padding": {"Left": 10, "Top": 10, "Right": 10, "Bottom": 10}` |
| `"Padding": [10, 5, 10, 5]` | `"Padding": {"Left": 10, "Top": 5, ...}` |

**Key insight**: If your C++ uses Format A and extracted shows Format B, **keep Format A** - they produce the same result.

### Real-World Example: WBP_MF_MatchInfo

Here's a complete example comparing extracted JSON with C++ spec and showing the correct update approach.

**Extracted JSON (from WBP):**
```json
{
    "BlueprintName": "WBP_MF_MatchInfo",
    "DesignerPreview": {
        "SizeMode": "DesiredOnScreen",
        "ZoomLevel": 14
    },
    "Hierarchy": {
        "Root": {
            "Type": "CanvasPanel",
            "Name": "RootCanvas",
            "Children": [{
                "Type": "HorizontalBox",
                "Name": "ScoreContainer",
                "Slot": {
                    "Anchors": {"Min": {"X": 0.5, "Y": 0}, "Max": {"X": 0.5, "Y": 0}},
                    "Position": {"X": 0, "Y": 10},
                    "Size": {"X": 400, "Y": 80},
                    "Alignment": {"X": 0.5, "Y": 0}
                },
                "Children": [
                    {"Type": "VerticalBox", "Name": "TeamABox", "Slot": {"HAlign": "Fill", "VAlign": "Fill", "Size": {"Rule": "Fill", "Value": 1}}, "Children": [
                        {"Type": "TextBlock", "Name": "TeamANameText", "Slot": {"HAlign": "Center", "VAlign": "Fill", "Size": {"Rule": "Auto"}}},
                        {"Type": "TextBlock", "Name": "TeamAScoreText", "Slot": {"HAlign": "Center", "VAlign": "Fill", "Size": {"Rule": "Auto"}}}
                    ]},
                    {"Type": "TextBlock", "Name": "MatchTimerText", "Slot": {"Padding": {"Left": 15, "Right": 15}, "HAlign": "Center", "VAlign": "Center", "Size": {"Rule": "Auto"}}},
                    {"Type": "VerticalBox", "Name": "TeamBBox", "Slot": {"HAlign": "Fill", "VAlign": "Fill", "Size": {"Rule": "Fill", "Value": 1}}, "Children": [...]}
                ]
            }]
        }
    },
    "Design": {
        "TeamANameText": {
            "Font": {"Size": 14, "Typeface": "Bold"},
            "ColorAndOpacity": {"R": 1, "G": 1, "B": 1, "A": 1}
        },
        "TeamAScoreText": {
            "Font": {"Size": 36, "Typeface": "Bold"},
            "ColorAndOpacity": {"R": 0.20000000298023224, "G": 0.60000002384185791, "B": 1, "A": 1}
        }
    },
    "Dependencies": ["/Engine/EngineFonts/Roboto.Roboto"]
}
```

**Original C++ GetWidgetSpec():**
```cpp
"Hierarchy": {
    "Root": {
        "Type": "CanvasPanel",
        "Name": "RootCanvas",
        "Children": [{
            "Type": "HorizontalBox",
            "Name": "ScoreContainer",
            "Slot": {
                "Anchors": {"Min": {"X": 0.5, "Y": 0}, "Max": {"X": 0.5, "Y": 0}},
                "Position": {"X": 0, "Y": 10},
                "Size": {"X": 400, "Y": 80},
                "Alignment": {"X": 0.5, "Y": 0}
            },
            "Children": [
                {
                    "Type": "VerticalBox",
                    "Name": "TeamABox",
                    "Children": [
                        {"Type": "TextBlock", "Name": "TeamANameText", "BindingType": "Optional", "Text": "TEAM A", "FontSize": 14},
                        {"Type": "TextBlock", "Name": "TeamAScoreText", "BindingType": "Required", "Text": "0", "FontSize": 36}
                    ]
                }
            ]
        }]
    }
},
"Design": {
    "TeamAScoreText": {
        "Font": {"Size": 36, "Typeface": "Bold"},
        "ColorAndOpacity": {"R": 0.2, "G": 0.6, "B": 1.0, "A": 1.0}
    }
},
"Dependencies": []
```

**Analysis - What to Update:**

| Extracted | C++ | Action | Reason |
|-----------|-----|--------|--------|
| Has `Slot` on children | Missing `Slot` on children | ✅ **Add** if box sizing needed | Controls layout in parent |
| `0.20000000298023224` | `0.2` | ❌ **Keep 0.2** | Same value, different precision |
| Has `TeamANameText` in Design | Missing | ✅ **Add** entry | Ensures font styling |
| Has font dependency | Empty `[]` | ✅ **Add** to array | Documents font usage |
| Uses `Position`+`Size` | Uses `Position`+`Size` | ❌ **Keep same** | Already matches |

**Correct Update (Design section only):**
```cpp
"Design": {
    "TeamANameText": {
        "Font": {"Size": 14, "Typeface": "Bold"},
        "ColorAndOpacity": {"R": 1.0, "G": 1.0, "B": 1.0, "A": 1.0}
    },
    "TeamAScoreText": {
        "Font": {"Size": 36, "Typeface": "Bold"},
        "ColorAndOpacity": {"R": 0.2, "G": 0.6, "B": 1.0, "A": 1.0},  // Keep readable precision
        "Justification": "Center"  // Keep existing field
    },
    // ... keep all other existing entries
},
"Dependencies": [
    "/Engine/EngineFonts/Roboto.Roboto"  // Add font dependency
]
```

**Key Takeaways:**
1. **Add missing Design entries** - but keep existing fields
2. **Keep readable float values** - `0.2` equals `0.20000000298023224`
3. **Keep existing slot format** - don't switch from Position+Size to Offsets
4. **Add to Dependencies** - don't replace existing array
5. **Never remove** - `BindingType`, `Text`, `Justification`, etc.

### Real-World Example: Button with Full Styles

**Extracted JSON shows complete button states:**
```json
"QuickJoinButton": {
    "Style": {
        "Normal": {
            "TintColor": {"R": 0.30000001192092896, "G": 0.5, "B": 0.30000001192092896, "A": 1}
        },
        "Hovered": {
            "TintColor": {"R": 0.72426801919937134, "G": 0.72426801919937134, "B": 0.72426801919937134, "A": 1}
        },
        "Pressed": {
            "TintColor": {"R": 0.38426598906517029, "G": 0.38426598906517029, "B": 0.38426598906517029, "A": 1}
        }
    },
    "IsFocusable": true
}
```

**Original C++ has only Normal:**
```cpp
"QuickJoinButton": {
    "Style": {
        "Normal": {"TintColor": {"R": 0.3, "G": 0.5, "B": 0.3, "A": 1.0}}
    }
}
```

**Correct Update - Add states, keep readable values:**
```cpp
"QuickJoinButton": {
    "Style": {
        "Normal": {"TintColor": {"R": 0.3, "G": 0.5, "B": 0.3, "A": 1.0}},
        "Hovered": {"TintColor": {"R": 0.72, "G": 0.72, "B": 0.72, "A": 1.0}},
        "Pressed": {"TintColor": {"R": 0.38, "G": 0.38, "B": 0.38, "A": 1.0}}
    },
    "IsFocusable": true
}
```

**Notes:**
- Keep `0.3` instead of `0.30000001192092896` (equivalent)
- Round `0.72426801919937134` to `0.72` (close enough for visual)
- Add `IsFocusable: true` for keyboard navigation

### Real-World Example: Nested UserWidget Class Path

**Extracted JSON uses Blueprint path:**
```json
{
    "Type": "UserWidget",
    "Name": "QuickTeamA",
    "WidgetClass": "/Game/UI/Widgets/WBP_MF_QuickTeamPanel.WBP_MF_QuickTeamPanel_C",
    "Slot": {"Padding": {"Right": 20}, "HAlign": "Fill", "VAlign": "Fill", "Size": {"Rule": "Auto"}}
}
```

**Original C++ uses Script path:**
```cpp
{
    "Type": "UserWidget",
    "Name": "QuickTeamA",
    "WidgetClass": "/Script/P_MiniFootball.MF_QuickTeamPanel",
    "Slot": {"Padding": {"Right": 20}}
}
```

**Correct Update - Keep Script path, add Slot fields:**
```cpp
{
    "Type": "UserWidget",
    "Name": "QuickTeamA",
    "WidgetClass": "/Script/P_MiniFootball.MF_QuickTeamPanel",
    "Slot": {"Padding": {"Right": 20}, "HAlign": "Fill", "VAlign": "Fill", "Size": {"Rule": "Auto"}}
}
```

**Notes:**
- **Keep `/Script/...` path** - MWCS resolves both, but Script path is more stable
- Add the Slot fields if sizing is needed

### Real-World Example: Border with Design

**Extracted JSON:**
```json
"Design": {
    "PanelBorder": {
        "BrushColor": {
            "R": 0.15000000596046448,
            "G": 0.15000000596046448,
            "B": 0.15000000596046448,
            "A": 0.85000002384185791
        },
        "Padding": {"Left": 8, "Top": 6, "Right": 8, "Bottom": 6}
    }
}
```

**Original C++ (same values, readable):**
```cpp
"Design": {
    "PanelBorder": {
        "BrushColor": {"R": 0.15, "G": 0.15, "B": 0.15, "A": 0.85},
        "Padding": {"Left": 8, "Top": 6, "Right": 8, "Bottom": 6}
    }
}
```

**Correct Action: NO CHANGE NEEDED**

The C++ values are equivalent - `0.15` equals `0.15000000596046448`. If values match (accounting for float precision), no update is required.

### Real-World Example: Box Slots with Size.Rule

**Extracted shows Size.Rule on all children:**
```json
{
    "Type": "VerticalBox",
    "Name": "TeamABox",
    "Slot": {"HAlign": "Fill", "VAlign": "Fill", "Size": {"Rule": "Fill", "Value": 1}},
    "Children": [
        {"Type": "TextBlock", "Name": "TeamANameText", "Slot": {"HAlign": "Center", "VAlign": "Fill", "Size": {"Rule": "Auto"}}},
        {"Type": "TextBlock", "Name": "TeamAScoreText", "Slot": {"HAlign": "Center", "VAlign": "Fill", "Size": {"Rule": "Auto"}}}
    ]
}
```

**Original C++ has no Slot on children:**
```cpp
{
    "Type": "VerticalBox",
    "Name": "TeamABox",
    "Children": [
        {"Type": "TextBlock", "Name": "TeamANameText", "BindingType": "Optional", "Text": "TEAM A"},
        {"Type": "TextBlock", "Name": "TeamAScoreText", "BindingType": "Required", "Text": "0"}
    ]
}
```

**Correct Update - Add Slot but keep other fields:**
```cpp
{
    "Type": "VerticalBox",
    "Name": "TeamABox",
    "Slot": {"HAlign": "Fill", "VAlign": "Fill", "Size": {"Rule": "Fill", "Value": 1}},
    "Children": [
        {"Type": "TextBlock", "Name": "TeamANameText", "BindingType": "Optional", "Text": "TEAM A", 
         "Slot": {"HAlign": "Center", "VAlign": "Fill", "Size": {"Rule": "Auto"}}},
        {"Type": "TextBlock", "Name": "TeamAScoreText", "BindingType": "Required", "Text": "0",
         "Slot": {"HAlign": "Center", "VAlign": "Fill", "Size": {"Rule": "Auto"}}}
    ]
}
```

**Notes:**
- **Keep `BindingType`** - not extracted but required for C++ binding
- **Keep `Text`** - default text for preview
- **Add `Slot`** - for proper box sizing behavior

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

Troubleshooting tip:

- If logs say an asset was created/repaired but you don’t see the `.uasset`, the report is the source of truth for unsupported widget types or validation failures.

## Supported widget types

MWCS supports an explicit mapping from node `Type` → UMG class:

- Containers: `CanvasPanel`, `VerticalBox`, `HorizontalBox`, `Overlay`, `Border`, `ScrollBox`, `WidgetSwitcher`
- Controls: `Button`, `TextBlock`, `Image`, `Spacer`, `Throbber`, `MultiLineEditableTextBox`
- Nested: `UserWidget` (with `WidgetClass`)

Unsupported types are reported as build/validation errors.

## Author

Punal Manalan
