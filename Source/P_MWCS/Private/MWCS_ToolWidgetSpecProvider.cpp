#include "MWCS_ToolWidgetSpecProvider.h"

FString UMWCS_ToolWidgetSpecProvider::GetWidgetSpec()
{
	// Tool EUW designer layout (strict-native wired via UMWCS_ToolEUW; no Blueprint graphs, no Python).
	return TEXT(R"JSON(
{
	"BlueprintName": "EUW_MWCS_Tool",
	"ParentClass": "/Script/P_MWCS.MWCS_ToolEUW",
	"IsToolEUW": true,
	"Version": 1,
	"Bindings": {
		"Required": [],
		"Optional": []
	},
	"Hierarchy": {
		"Type": "VerticalBox",
		"Name": "RootVBox",
		"Children": [
			{
				"Type": "HorizontalBox",
				"Name": "HeaderRow",
				"Slot": { "Padding": [8, 8, 8, 4] },
				"Children": [
					{ "Type": "TextBlock", "Name": "TitleText", "Text": "MWCS \u2014 Modular Widget Creation System", "FontSize": 18, "Slot": { "VAlign": "Center" } },
					{ "Type": "Spacer", "Name": "HeaderSpacer", "Slot": { "Fill": 1 } },
					{ "Type": "Button", "Name": "Btn_OpenSettings", "Slot": { "VAlign": "Center" }, "Children": [ { "Type": "TextBlock", "Name": "Txt_OpenSettings", "Text": "Settings", "FontSize": 13, "Justification": "Center", "Slot": { "HAlign": "Center", "VAlign": "Center", "Padding": [10, 4, 10, 4] } } ] }
				]
			},
			{
				"Type": "HorizontalBox",
				"Name": "ActionRow",
				"Slot": { "Padding": [8, 4, 8, 8] },
				"Children": [
					{ "Type": "Button", "Name": "Btn_Validate", "Slot": { "Padding": [2, 0, 2, 0] }, "Children": [ { "Type": "TextBlock", "Name": "Txt_Validate", "Text": "Validate", "FontSize": 13, "Justification": "Center", "Slot": { "HAlign": "Center", "VAlign": "Center", "Padding": [10, 4, 10, 4] } } ] },
					{ "Type": "Button", "Name": "Btn_CreateMissing", "Slot": { "Padding": [2, 0, 2, 0] }, "Children": [ { "Type": "TextBlock", "Name": "Txt_CreateMissing", "Text": "Create Missing", "FontSize": 13, "Justification": "Center", "Slot": { "HAlign": "Center", "VAlign": "Center", "Padding": [10, 4, 10, 4] } } ] },
					{ "Type": "Button", "Name": "Btn_Repair", "Slot": { "Padding": [2, 0, 2, 0] }, "Children": [ { "Type": "TextBlock", "Name": "Txt_Repair", "Text": "Repair", "FontSize": 13, "Justification": "Center", "Slot": { "HAlign": "Center", "VAlign": "Center", "Padding": [10, 4, 10, 4] } } ] },
					{ "Type": "Button", "Name": "Btn_ForceRecreate", "Slot": { "Padding": [2, 0, 2, 0] }, "Children": [ { "Type": "TextBlock", "Name": "Txt_ForceRecreate", "Text": "Force Recreate", "FontSize": 13, "Justification": "Center", "Slot": { "HAlign": "Center", "VAlign": "Center", "Padding": [10, 4, 10, 4] } } ] },
					{ "Type": "Button", "Name": "Btn_GenerateToolEUW", "Slot": { "Padding": [2, 0, 2, 0] }, "Children": [ { "Type": "TextBlock", "Name": "Txt_GenerateToolEUW", "Text": "Generate/Repair Tool EUW", "FontSize": 13, "Justification": "Center", "Slot": { "HAlign": "Center", "VAlign": "Center", "Padding": [10, 4, 10, 4] } } ] },
					{ "Type": "Button", "Name": "Btn_ExtractSelectedWBP", "Slot": { "Padding": [2, 0, 2, 0] }, "Children": [ { "Type": "TextBlock", "Name": "Txt_ExtractSelectedWBP", "Text": "Extract Selected WBP", "FontSize": 13, "Justification": "Center", "Slot": { "HAlign": "Center", "VAlign": "Center", "Padding": [10, 4, 10, 4] } } ] }
				]
			},
			{ "Type": "TextBlock", "Name": "SettingsSummaryText", "Text": "", "FontSize": 12, "Slot": { "Padding": [8, 0, 8, 8] } },
			{ "Type": "MultiLineEditableTextBox", "Name": "OutputLog", "Slot": { "Padding": [8, 0, 8, 8], "Size": { "Rule": "Fill", "Value": 1 } } }
		]
	}
}
)JSON");
}
