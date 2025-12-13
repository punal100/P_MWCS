#include "MWCS_ToolWidgetSpecProvider.h"

FString UMWCS_ToolWidgetSpecProvider::GetWidgetSpec()
{
    // Minimal tool EUW designer layout. Button click wiring is handled separately (manual initially).
    return TEXT(R"JSON(
{
	"BlueprintName": "EUW_MWCS_Tool",
	"ParentClass": "/Script/Blutility.EditorUtilityWidget",
	"Version": 1,
	"Bindings": {
		"Required": [],
		"Optional": []
	},
	"Hierarchy": {
		"Type": "CanvasPanel",
		"Name": "RootCanvas",
		"Children": [
			{
				"Type": "VerticalBox",
				"Name": "MainVBox",
				"Children": [
					{ "Type": "TextBlock", "Name": "TitleText" },
					{ "Type": "Button", "Name": "Btn_Validate" },
					{ "Type": "Button", "Name": "Btn_CreateMissing" },
					{ "Type": "Button", "Name": "Btn_Repair" },
					{ "Type": "Button", "Name": "Btn_ForceRecreate" },
					{ "Type": "Button", "Name": "Btn_GenerateToolEUW" },
					{ "Type": "MultiLineEditableTextBox", "Name": "OutputLog" }
				]
			}
		]
	}
}
)JSON");
}
