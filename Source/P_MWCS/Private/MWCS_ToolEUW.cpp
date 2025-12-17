#include "MWCS_ToolEUW.h"

#include "MWCS_Report.h"
#include "MWCS_Service.h"
#include "MWCS_Settings.h"

#include "Modules/ModuleManager.h"

#include "Components/Button.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/TextBlock.h"

#include "EditorUtilityLibrary.h"
#include "WidgetBlueprint.h"

#include "Blueprint/WidgetBlueprintGeneratedClass.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ContentWidget.h"
#include "Components/PanelWidget.h"

#include "Blueprint/UserWidget.h"

#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/Throbber.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"

#include "Components/Widget.h"

#include "UObject/UnrealType.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SWindow.h"

#include "ISettingsModule.h"

namespace
{
    FString BuildSettingsSummaryString()
    {
        const UMWCS_Settings *Settings = UMWCS_Settings::Get();
        if (!Settings)
        {
            return TEXT("MWCS Settings: <unavailable>");
        }

        return FString::Printf(TEXT("Output=%s | ToolEUW=%s/%s | Allowlist=%d | SpecProvider=%s"),
                               *Settings->OutputRootPath,
                               *Settings->ToolEuwOutputPath,
                               *Settings->ToolEuwAssetName,
                               Settings->SpecProviderClasses.Num(),
                               *Settings->ToolEuwSpecProviderClass.ToString());
    }
} // namespace

void UMWCS_ToolEUW::NativeConstruct()
{
    Super::NativeConstruct();

    if (OutputLog)
    {
        OutputLog->SetIsReadOnly(true);
    }

    RefreshSettingsSummary();

    if (Btn_OpenSettings)
    {
        Btn_OpenSettings->OnClicked.Clear();
        Btn_OpenSettings->OnClicked.AddDynamic(this, &UMWCS_ToolEUW::HandleOpenSettingsClicked);
    }

    if (Btn_Validate)
    {
        Btn_Validate->OnClicked.Clear();
        Btn_Validate->OnClicked.AddDynamic(this, &UMWCS_ToolEUW::HandleValidateClicked);
    }

    if (Btn_CreateMissing)
    {
        Btn_CreateMissing->OnClicked.Clear();
        Btn_CreateMissing->OnClicked.AddDynamic(this, &UMWCS_ToolEUW::HandleCreateMissingClicked);
    }

    if (Btn_Repair)
    {
        Btn_Repair->OnClicked.Clear();
        Btn_Repair->OnClicked.AddDynamic(this, &UMWCS_ToolEUW::HandleRepairClicked);
    }

    if (Btn_ForceRecreate)
    {
        Btn_ForceRecreate->OnClicked.Clear();
        Btn_ForceRecreate->OnClicked.AddDynamic(this, &UMWCS_ToolEUW::HandleForceRecreateClicked);
    }

    if (Btn_GenerateToolEUW)
    {
        Btn_GenerateToolEUW->OnClicked.Clear();
        Btn_GenerateToolEUW->OnClicked.AddDynamic(this, &UMWCS_ToolEUW::HandleGenerateToolEuwClicked);
    }

    if (Btn_ExtractSelectedWBP)
    {
        Btn_ExtractSelectedWBP->OnClicked.Clear();
        Btn_ExtractSelectedWBP->OnClicked.AddDynamic(this, &UMWCS_ToolEUW::HandleExtractSelectedWbpClicked);
    }

    AppendLine(TEXT("MWCS Tool EUW ready."));
}

static FString MWCS_NormalizeWidgetType(const UWidget *Widget)
{
    if (!Widget)
    {
        return TEXT("<null>");
    }

    // Prefer MWCS-supported base widget type names so the exported spec can be re-built.
    if (Widget->IsA<UCanvasPanel>())
        return TEXT("CanvasPanel");
    if (Widget->IsA<UVerticalBox>())
        return TEXT("VerticalBox");
    if (Widget->IsA<UHorizontalBox>())
        return TEXT("HorizontalBox");
    if (Widget->IsA<UOverlay>())
        return TEXT("Overlay");
    if (Widget->IsA<UBorder>())
        return TEXT("Border");
    if (Widget->IsA<UButton>())
        return TEXT("Button");
    if (Widget->IsA<UTextBlock>())
        return TEXT("TextBlock");
    if (Widget->IsA<UImage>())
        return TEXT("Image");
    if (Widget->IsA<USpacer>())
        return TEXT("Spacer");
    if (Widget->IsA<UMultiLineEditableTextBox>())
        return TEXT("MultiLineEditableTextBox");
    if (Widget->IsA<UScrollBox>())
        return TEXT("ScrollBox");
    if (Widget->IsA<UThrobber>())
        return TEXT("Throbber");
    if (Widget->IsA<UComboBoxString>())
        return TEXT("ComboBoxString");
    if (Widget->IsA<UWidgetSwitcher>())
        return TEXT("WidgetSwitcher");
    if (Widget->IsA<UUserWidget>())
        return TEXT("UserWidget");

    FString Type = Widget->GetClass()->GetName();
    if (Type.StartsWith(TEXT("U")))
    {
        Type.RightChopInline(1);
    }

    if (Type.EndsWith(TEXT("Widget")))
    {
        Type.LeftChopInline(6);
    }

    return Type;
}

static bool MWCS_TryGetBoolPropertyValue(const UObject *Obj, const FName PropertyName, bool &bOutValue)
{
    if (!Obj)
    {
        return false;
    }

    const FProperty *Prop = Obj->GetClass()->FindPropertyByName(PropertyName);
    const FBoolProperty *BoolProp = CastField<FBoolProperty>(Prop);
    if (!BoolProp)
    {
        return false;
    }

    bOutValue = BoolProp->GetPropertyValue_InContainer(Obj);
    return true;
}

static bool MWCS_TryGetBytePropertyValue(const UObject *Obj, const FName PropertyName, uint8 &OutValue)
{
    if (!Obj)
    {
        return false;
    }

    const FProperty *Prop = Obj->GetClass()->FindPropertyByName(PropertyName);
    const FByteProperty *ByteProp = CastField<FByteProperty>(Prop);
    if (!ByteProp)
    {
        return false;
    }

    OutValue = ByteProp->GetPropertyValue_InContainer(Obj);
    return true;
}

static void MWCS_SetArray2(TSharedPtr<FJsonObject> &Obj, const TCHAR *Field, const FVector2D &V)
{
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(V.X));
    Arr.Add(MakeShared<FJsonValueNumber>(V.Y));
    Obj->SetArrayField(Field, Arr);
}

static void MWCS_SetMarginObject4(TSharedPtr<FJsonObject> &Obj, const TCHAR *Field, const FMargin &M)
{
    TSharedPtr<FJsonObject> MarginObj = MakeShared<FJsonObject>();
    MarginObj->SetNumberField(TEXT("Left"), M.Left);
    MarginObj->SetNumberField(TEXT("Top"), M.Top);
    MarginObj->SetNumberField(TEXT("Right"), M.Right);
    MarginObj->SetNumberField(TEXT("Bottom"), M.Bottom);
    Obj->SetObjectField(Field, MarginObj);
}

static void MWCS_SetVector2Object(TSharedPtr<FJsonObject> &Obj, const TCHAR *Field, const FVector2D &V)
{
    TSharedPtr<FJsonObject> VecObj = MakeShared<FJsonObject>();
    VecObj->SetNumberField(TEXT("X"), V.X);
    VecObj->SetNumberField(TEXT("Y"), V.Y);
    Obj->SetObjectField(Field, VecObj);
}

static void MWCS_SetColorObject(TSharedPtr<FJsonObject> &Obj, const TCHAR *Field, const FLinearColor &C)
{
    TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
    ColorObj->SetNumberField(TEXT("R"), C.R);
    ColorObj->SetNumberField(TEXT("G"), C.G);
    ColorObj->SetNumberField(TEXT("B"), C.B);
    ColorObj->SetNumberField(TEXT("A"), C.A);
    Obj->SetObjectField(Field, ColorObj);
}

static void MWCS_SetPaddingMinimal(TSharedPtr<FJsonObject> &Obj, const TCHAR *Field, const FMargin &M)
{
    if (FMath::IsNearlyZero(M.Left) && FMath::IsNearlyZero(M.Top) && FMath::IsNearlyZero(M.Right) && FMath::IsNearlyZero(M.Bottom))
    {
        return;
    }

    TSharedPtr<FJsonObject> PadObj = MakeShared<FJsonObject>();
    if (!FMath::IsNearlyZero(M.Left))
        PadObj->SetNumberField(TEXT("Left"), M.Left);
    if (!FMath::IsNearlyZero(M.Top))
        PadObj->SetNumberField(TEXT("Top"), M.Top);
    if (!FMath::IsNearlyZero(M.Right))
        PadObj->SetNumberField(TEXT("Right"), M.Right);
    if (!FMath::IsNearlyZero(M.Bottom))
        PadObj->SetNumberField(TEXT("Bottom"), M.Bottom);

    Obj->SetObjectField(Field, PadObj);
}

static FString MWCS_HAlignToString(EHorizontalAlignment Align)
{
    switch (Align)
    {
    case HAlign_Left:
        return TEXT("Left");
    case HAlign_Center:
        return TEXT("Center");
    case HAlign_Right:
        return TEXT("Right");
    case HAlign_Fill:
    default:
        return TEXT("Fill");
    }
}

static FString MWCS_VAlignToString(EVerticalAlignment Align)
{
    switch (Align)
    {
    case VAlign_Top:
        return TEXT("Top");
    case VAlign_Center:
        return TEXT("Center");
    case VAlign_Bottom:
        return TEXT("Bottom");
    case VAlign_Fill:
    default:
        return TEXT("Fill");
    }
}

static void MWCS_ExportSlotLayout(UWidget *Widget, TSharedPtr<FJsonObject> &NodeObj, bool bIncludeSlotLayout, bool bIncludeCanvasSlot)
{
    if (!bIncludeSlotLayout || !Widget)
    {
        return;
    }

    UPanelSlot *Slot = Widget->Slot;
    if (!Slot)
    {
        return;
    }

    TSharedPtr<FJsonObject> SlotObj = MakeShared<FJsonObject>();
    bool bWroteAnything = false;

    auto WritePaddingHAlignVAlign = [&SlotObj, &bWroteAnything](const FMargin &Padding, EHorizontalAlignment HAlign, EVerticalAlignment VAlign, bool bHasPadding, bool bHasH, bool bHasV)
    {
        if (bHasPadding)
        {
            MWCS_SetPaddingMinimal(SlotObj, TEXT("Padding"), Padding);
            bWroteAnything = true;
        }
        if (bHasH)
        {
            SlotObj->SetStringField(TEXT("HAlign"), MWCS_HAlignToString(HAlign));
            bWroteAnything = true;
        }
        if (bHasV)
        {
            SlotObj->SetStringField(TEXT("VAlign"), MWCS_VAlignToString(VAlign));
            bWroteAnything = true;
        }
    };

    if (UHorizontalBoxSlot *HB = Cast<UHorizontalBoxSlot>(Slot))
    {
        WritePaddingHAlignVAlign(HB->GetPadding(), HB->GetHorizontalAlignment(), HB->GetVerticalAlignment(), true, true, true);

        const FSlateChildSize Size = HB->GetSize();
        if (Size.SizeRule == ESlateSizeRule::Fill)
        {
            SlotObj->SetNumberField(TEXT("Fill"), Size.Value);
            bWroteAnything = true;
        }
        else
        {
            // Explicit Auto size object to be MWCS parser compatible.
            TSharedPtr<FJsonObject> SizeObj = MakeShared<FJsonObject>();
            SizeObj->SetStringField(TEXT("Rule"), TEXT("Auto"));
            SlotObj->SetObjectField(TEXT("Size"), SizeObj);
            bWroteAnything = true;
        }
    }
    else if (UVerticalBoxSlot *VB = Cast<UVerticalBoxSlot>(Slot))
    {
        WritePaddingHAlignVAlign(VB->GetPadding(), VB->GetHorizontalAlignment(), VB->GetVerticalAlignment(), true, true, true);

        const FSlateChildSize Size = VB->GetSize();
        if (Size.SizeRule == ESlateSizeRule::Fill)
        {
            SlotObj->SetNumberField(TEXT("Fill"), Size.Value);
            bWroteAnything = true;
        }
        else
        {
            TSharedPtr<FJsonObject> SizeObj = MakeShared<FJsonObject>();
            SizeObj->SetStringField(TEXT("Rule"), TEXT("Auto"));
            SlotObj->SetObjectField(TEXT("Size"), SizeObj);
            bWroteAnything = true;
        }
    }
    else if (UOverlaySlot *OV = Cast<UOverlaySlot>(Slot))
    {
        WritePaddingHAlignVAlign(OV->GetPadding(), OV->GetHorizontalAlignment(), OV->GetVerticalAlignment(), true, true, true);
    }
    else if (UBorderSlot *BS = Cast<UBorderSlot>(Slot))
    {
        WritePaddingHAlignVAlign(BS->GetPadding(), BS->GetHorizontalAlignment(), BS->GetVerticalAlignment(), true, true, true);
    }
    else if (UScrollBoxSlot *SS = Cast<UScrollBoxSlot>(Slot))
    {
        WritePaddingHAlignVAlign(SS->GetPadding(), SS->GetHorizontalAlignment(), SS->GetVerticalAlignment(), true, true, true);

        const FSlateChildSize Size = SS->GetSize();
        if (Size.SizeRule == ESlateSizeRule::Fill)
        {
            SlotObj->SetNumberField(TEXT("Fill"), Size.Value);
            bWroteAnything = true;
        }
        else
        {
            TSharedPtr<FJsonObject> SizeObj = MakeShared<FJsonObject>();
            SizeObj->SetStringField(TEXT("Rule"), TEXT("Auto"));
            SlotObj->SetObjectField(TEXT("Size"), SizeObj);
            bWroteAnything = true;
        }
    }

    if (bIncludeCanvasSlot)
    {
        if (UCanvasPanelSlot *CS = Cast<UCanvasPanelSlot>(Slot))
        {
            // Match GetWidgetSpec slot format used in project: Anchors/Position/Size/Alignment
            {
                const FAnchors Anchors = CS->GetAnchors();
                TSharedPtr<FJsonObject> AnchorsObj = MakeShared<FJsonObject>();
                MWCS_SetVector2Object(AnchorsObj, TEXT("Min"), FVector2D(Anchors.Minimum.X, Anchors.Minimum.Y));
                MWCS_SetVector2Object(AnchorsObj, TEXT("Max"), FVector2D(Anchors.Maximum.X, Anchors.Maximum.Y));
                SlotObj->SetObjectField(TEXT("Anchors"), AnchorsObj);
                bWroteAnything = true;
            }

            MWCS_SetVector2Object(SlotObj, TEXT("Position"), CS->GetPosition());
            MWCS_SetVector2Object(SlotObj, TEXT("Size"), CS->GetSize());
            MWCS_SetVector2Object(SlotObj, TEXT("Alignment"), CS->GetAlignment());
            bWroteAnything = true;
        }
    }

    if (bWroteAnything)
    {
        NodeObj->SetObjectField(TEXT("Slot"), SlotObj);
    }
}

static void MWCS_TryAddDependency(TSet<FString> &OutDeps, const UObject *Obj)
{
    if (!Obj)
    {
        return;
    }

    const FString Path = Obj->GetPathName();
    if (!Path.IsEmpty())
    {
        OutDeps.Add(Path);
    }
}

static bool MWCS_TryGetBoolPropertyByName(UObject *Obj, const TCHAR *PropName, bool &OutValue)
{
    if (!Obj)
    {
        return false;
    }

    if (FProperty *Prop = Obj->GetClass()->FindPropertyByName(FName(PropName)))
    {
        if (FBoolProperty *BoolProp = CastField<FBoolProperty>(Prop))
        {
            OutValue = BoolProp->GetPropertyValue_InContainer(Obj);
            return true;
        }
    }

    return false;
}

static bool MWCS_TryGetIntPropertyByName(UObject *Obj, const TCHAR *PropName, int32 &OutValue)
{
    if (!Obj)
    {
        return false;
    }

    if (FProperty *Prop = Obj->GetClass()->FindPropertyByName(FName(PropName)))
    {
        if (FIntProperty *IntProp = CastField<FIntProperty>(Prop))
        {
            OutValue = IntProp->GetPropertyValue_InContainer(Obj);
            return true;
        }
    }

    return false;
}

static void MWCS_ExportDesignerPreview(UWidgetBlueprint *WidgetBlueprint, TSharedPtr<FJsonObject> &Root)
{
    if (!WidgetBlueprint || !Root.IsValid())
    {
        return;
    }

    TSharedPtr<FJsonObject> Preview = MakeShared<FJsonObject>();

    // Persistent tier: DesignSizeMode + DesignTimeSize (when Custom)
    FString SizeModeStr = TEXT("FillScreen");
    FVector2D CustomSize = FVector2D::ZeroVector;

    if (UWidgetBlueprintGeneratedClass *GeneratedClass = Cast<UWidgetBlueprintGeneratedClass>(WidgetBlueprint->GeneratedClass))
    {
        if (UUserWidget *CDO = Cast<UUserWidget>(GeneratedClass->GetDefaultObject()))
        {
#if WITH_EDITORONLY_DATA
            switch (CDO->DesignSizeMode)
            {
            case EDesignPreviewSizeMode::Desired:
                SizeModeStr = TEXT("Desired");
                break;
            case EDesignPreviewSizeMode::DesiredOnScreen:
                SizeModeStr = TEXT("DesiredOnScreen");
                break;
            case EDesignPreviewSizeMode::Custom:
                SizeModeStr = TEXT("Custom");
                CustomSize = CDO->DesignTimeSize;
                break;
            case EDesignPreviewSizeMode::FillScreen:
            default:
                SizeModeStr = TEXT("FillScreen");
                break;
            }
#endif
        }
    }

    Preview->SetStringField(TEXT("SizeMode"), SizeModeStr);
    if (SizeModeStr.Equals(TEXT("Custom"), ESearchCase::IgnoreCase) && CustomSize.X > 0.0f && CustomSize.Y > 0.0f)
    {
        TSharedPtr<FJsonObject> CustomSizeObj = MakeShared<FJsonObject>();
        CustomSizeObj->SetNumberField(TEXT("X"), CustomSize.X);
        CustomSizeObj->SetNumberField(TEXT("Y"), CustomSize.Y);
        Preview->SetObjectField(TEXT("CustomSize"), CustomSizeObj);
    }

    // Best-effort tier: ZoomLevel + ShowGrid (may not exist on all engine versions)
    int32 ZoomLevel = 14;
    bool bShowGrid = true;

    (void)MWCS_TryGetIntPropertyByName(WidgetBlueprint, TEXT("ZoomLevel"), ZoomLevel);
    (void)MWCS_TryGetIntPropertyByName(WidgetBlueprint, TEXT("DesignerZoomLevel"), ZoomLevel);
    (void)MWCS_TryGetIntPropertyByName(WidgetBlueprint, TEXT("PreviewZoomLevel"), ZoomLevel);

    (void)MWCS_TryGetBoolPropertyByName(WidgetBlueprint, TEXT("bShowDesignGrid"), bShowGrid);
    (void)MWCS_TryGetBoolPropertyByName(WidgetBlueprint, TEXT("bShowGrid"), bShowGrid);
    (void)MWCS_TryGetBoolPropertyByName(WidgetBlueprint, TEXT("bShowDesignerGrid"), bShowGrid);

    Preview->SetNumberField(TEXT("ZoomLevel"), ZoomLevel);
    Preview->SetBoolField(TEXT("ShowGrid"), bShowGrid);

    Root->SetObjectField(TEXT("DesignerPreview"), Preview);
}

static bool MWCS_ExportInlineProperties(UWidget *Widget, TSharedPtr<FJsonObject> &NodeObj, bool bIncludePropertiesSection)
{
    if (!bIncludePropertiesSection || !Widget)
    {
        return false;
    }

    // Best-effort: only emit when we have something concrete.
    TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
    bool bHasAny = false;

    // Example: Border has padding (but in project specs this tends to live in Design; keep Properties minimal).
    // Add more here only when we can confidently match your GetWidgetSpec conventions.

    if (bHasAny)
    {
        NodeObj->SetObjectField(TEXT("Properties"), PropsObj);
    }
    return bHasAny;
}

static void MWCS_ExportDesignEntry(UWidget *Widget,
                                   TSharedPtr<FJsonObject> &OutDesignMap,
                                   TSet<FString> &OutDependencies,
                                   bool bIncludeDesignSection)
{
    if (!bIncludeDesignSection || !Widget || !OutDesignMap.IsValid())
    {
        return;
    }

    TSharedPtr<FJsonObject> DesignObj = MakeShared<FJsonObject>();
    bool bHasAny = false;

    const FString Key = Widget->GetFName().ToString();

    if (UButton *Button = Cast<UButton>(Widget))
    {
        // Match project style: Style.Normal/Hovered/Pressed.TintColor as {R,G,B,A}
        const FButtonStyle &Style = Button->GetStyle();
        TSharedPtr<FJsonObject> StyleObj = MakeShared<FJsonObject>();

        auto AddBrushTint = [&StyleObj, &bHasAny, &OutDependencies](const TCHAR *StateName, const FSlateBrush &Brush)
        {
            TSharedPtr<FJsonObject> StateObj = MakeShared<FJsonObject>();

            // TintColor is FSlateColor; use the specified color if available.
            const FLinearColor Tint = Brush.TintColor.GetSpecifiedColor();
            TSharedPtr<FJsonObject> TintObj = MakeShared<FJsonObject>();
            TintObj->SetNumberField(TEXT("R"), Tint.R);
            TintObj->SetNumberField(TEXT("G"), Tint.G);
            TintObj->SetNumberField(TEXT("B"), Tint.B);
            TintObj->SetNumberField(TEXT("A"), Tint.A);
            StateObj->SetObjectField(TEXT("TintColor"), TintObj);

            StyleObj->SetObjectField(StateName, StateObj);
            bHasAny = true;

            MWCS_TryAddDependency(OutDependencies, Brush.GetResourceObject());
        };

        AddBrushTint(TEXT("Normal"), Style.Normal);
        AddBrushTint(TEXT("Hovered"), Style.Hovered);
        AddBrushTint(TEXT("Pressed"), Style.Pressed);

        if (bHasAny)
        {
            DesignObj->SetObjectField(TEXT("Style"), StyleObj);
        }

        // Also match spec field
        DesignObj->SetBoolField(TEXT("IsFocusable"), Button->GetIsFocusable());
        bHasAny = true;
    }
    else if (UImage *Img = Cast<UImage>(Widget))
    {
        const FSlateBrush &Brush = Img->GetBrush();
        MWCS_SetVector2Object(DesignObj, TEXT("Size"), FVector2D(Brush.ImageSize.X, Brush.ImageSize.Y));
        MWCS_SetColorObject(DesignObj, TEXT("ColorAndOpacity"), Img->GetColorAndOpacity());
        bHasAny = true;

        MWCS_TryAddDependency(OutDependencies, Brush.GetResourceObject());
    }
    else if (UTextBlock *TB = Cast<UTextBlock>(Widget))
    {
        const FSlateFontInfo &Font = TB->GetFont();
        if (Font.Size > 0)
        {
            TSharedPtr<FJsonObject> FontObj = MakeShared<FJsonObject>();
            FontObj->SetNumberField(TEXT("Size"), Font.Size);
            if (!Font.TypefaceFontName.IsNone())
            {
                FontObj->SetStringField(TEXT("Typeface"), Font.TypefaceFontName.ToString());
            }
            DesignObj->SetObjectField(TEXT("Font"), FontObj);
            bHasAny = true;
        }

        // UTextBlock color is SlateColor; pull specified color.
        const FLinearColor C = TB->GetColorAndOpacity().GetSpecifiedColor();
        MWCS_SetColorObject(DesignObj, TEXT("ColorAndOpacity"), C);
        bHasAny = true;

        MWCS_TryAddDependency(OutDependencies, Font.FontObject);
    }
    else if (UBorder *Border = Cast<UBorder>(Widget))
    {
        // Commonly used for panels
        MWCS_SetColorObject(DesignObj, TEXT("BrushColor"), Border->GetBrushColor());
        MWCS_SetPaddingMinimal(DesignObj, TEXT("Padding"), Border->GetPadding());
        bHasAny = true;
        MWCS_TryAddDependency(OutDependencies, Border->Background.GetResourceObject());
    }

    if (bHasAny)
    {
        OutDesignMap->SetObjectField(Key, DesignObj);
    }
}

static void MWCS_ExportTextProps(UWidget *Widget, TSharedPtr<FJsonObject> &NodeObj, bool bIncludeTextProperties)
{
    if (!bIncludeTextProperties || !Widget)
    {
        return;
    }

    if (UTextBlock *TB = Cast<UTextBlock>(Widget))
    {
        const FString Text = TB->GetText().ToString();
        if (!Text.IsEmpty())
        {
            NodeObj->SetStringField(TEXT("Text"), Text);
        }

        const int32 FontSize = TB->GetFont().Size;
        if (FontSize > 0)
        {
            NodeObj->SetNumberField(TEXT("FontSize"), FontSize);
        }

        uint8 JustValue = 0;
        if (MWCS_TryGetBytePropertyValue(TB, TEXT("Justification"), JustValue))
        {
            const ETextJustify::Type Just = static_cast<ETextJustify::Type>(JustValue);
            switch (Just)
            {
            case ETextJustify::Left:
                NodeObj->SetStringField(TEXT("Justification"), TEXT("Left"));
                break;
            case ETextJustify::Center:
                NodeObj->SetStringField(TEXT("Justification"), TEXT("Center"));
                break;
            case ETextJustify::Right:
                NodeObj->SetStringField(TEXT("Justification"), TEXT("Right"));
                break;
            default:
                break;
            }
        }
    }
}

static void MWCS_ExportIsVariable(UWidget *Widget, TSharedPtr<FJsonObject> &NodeObj, bool bIncludeIsVariable)
{
    if (!bIncludeIsVariable || !Widget)
    {
        return;
    }

    bool bIsVar = true;
    if (MWCS_TryGetBoolPropertyValue(Widget, TEXT("bIsVariable"), bIsVar))
    {
        NodeObj->SetBoolField(TEXT("IsVariable"), bIsVar);
    }
}

static void MWCS_ExportUserWidgetClassPath(UWidget *Widget, TSharedPtr<FJsonObject> &NodeObj, bool bIncludeWidgetClassPaths)
{
    if (!bIncludeWidgetClassPaths || !Widget)
    {
        return;
    }

    if (Widget->IsA<UUserWidget>())
    {
        // When Type is UserWidget, MWCS builder uses WidgetClass to resolve the class.
        NodeObj->SetStringField(TEXT("WidgetClass"), Widget->GetClass()->GetPathName());
    }
}

static TSharedPtr<FJsonObject> MWCS_ExportWidgetRecursive(UWidget *Widget,
                                                          TSharedPtr<FJsonObject> &OutDesignMap,
                                                          TSet<FString> &OutDependencies,
                                                          bool bIncludeSlotLayout,
                                                          bool bIncludeCanvasSlot,
                                                          bool bIncludeTextProperties,
                                                          bool bIncludeWidgetClassPaths,
                                                          bool bIncludeIsVariable,
                                                          bool bIncludePropertiesSection,
                                                          bool bIncludeDesignSection)
{
    if (!Widget)
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("Type"), MWCS_NormalizeWidgetType(Widget));
    Obj->SetStringField(TEXT("Name"), Widget->GetFName().ToString());

    MWCS_ExportInlineProperties(Widget, Obj, bIncludePropertiesSection);
    MWCS_ExportDesignEntry(Widget, OutDesignMap, OutDependencies, bIncludeDesignSection);

    MWCS_ExportIsVariable(Widget, Obj, bIncludeIsVariable);
    MWCS_ExportTextProps(Widget, Obj, bIncludeTextProperties);
    MWCS_ExportSlotLayout(Widget, Obj, bIncludeSlotLayout, bIncludeCanvasSlot);

    if (MWCS_NormalizeWidgetType(Widget) == TEXT("UserWidget"))
    {
        MWCS_ExportUserWidgetClassPath(Widget, Obj, bIncludeWidgetClassPaths);
    }

    TArray<TSharedPtr<FJsonValue>> ChildValues;

    if (UPanelWidget *Panel = Cast<UPanelWidget>(Widget))
    {
        const int32 Count = Panel->GetChildrenCount();
        for (int32 i = 0; i < Count; ++i)
        {
            if (UWidget *Child = Panel->GetChildAt(i))
            {
                if (TSharedPtr<FJsonObject> ChildObj = MWCS_ExportWidgetRecursive(Child,
                                                                                  OutDesignMap,
                                                                                  OutDependencies,
                                                                                  bIncludeSlotLayout,
                                                                                  bIncludeCanvasSlot,
                                                                                  bIncludeTextProperties,
                                                                                  bIncludeWidgetClassPaths,
                                                                                  bIncludeIsVariable,
                                                                                  bIncludePropertiesSection,
                                                                                  bIncludeDesignSection))
                {
                    ChildValues.Add(MakeShared<FJsonValueObject>(ChildObj));
                }
            }
        }
    }
    else if (UContentWidget *Content = Cast<UContentWidget>(Widget))
    {
        if (UWidget *Child = Content->GetContent())
        {
            if (TSharedPtr<FJsonObject> ChildObj = MWCS_ExportWidgetRecursive(Child,
                                                                              OutDesignMap,
                                                                              OutDependencies,
                                                                              bIncludeSlotLayout,
                                                                              bIncludeCanvasSlot,
                                                                              bIncludeTextProperties,
                                                                              bIncludeWidgetClassPaths,
                                                                              bIncludeIsVariable,
                                                                              bIncludePropertiesSection,
                                                                              bIncludeDesignSection))
            {
                ChildValues.Add(MakeShared<FJsonValueObject>(ChildObj));
            }
        }
    }

    if (ChildValues.Num() > 0)
    {
        Obj->SetArrayField(TEXT("Children"), ChildValues);
    }

    return Obj;
}

static void MWCS_ShowJsonOutputWindow(const FString &Title, const FString &JsonText)
{
    if (!FSlateApplication::IsInitialized())
    {
        return;
    }

    TSharedRef<SWindow> Window = SNew(SWindow)
                                     .Title(FText::FromString(Title))
                                     .ClientSize(FVector2D(980.0f, 720.0f))
                                     .SupportsMaximize(true)
                                     .SupportsMinimize(true);

    Window->SetContent(
        SNew(SBorder)
            .Padding(8.0f)
                [SNew(SMultiLineEditableTextBox)
                     .IsReadOnly(true)
                     .Text(FText::FromString(JsonText))
                     .SelectAllTextWhenFocused(true)
                     .AllowContextMenu(true)]);

    FSlateApplication::Get().AddWindow(Window);
}

bool UMWCS_ToolEUW::ExportWidgetBlueprintHierarchyToJson(UWidgetBlueprint *WidgetBlueprint,
                                                         bool bWriteToFile,
                                                         bool bCopyToClipboard,
                                                         bool bShowOutputWindow,
                                                         bool bIncludeSourceAssetField,
                                                         bool bIncludeSlotLayout,
                                                         bool bIncludeCanvasSlot,
                                                         bool bIncludeTextProperties,
                                                         bool bIncludeWidgetClassPaths,
                                                         bool bIncludeIsVariable,
                                                         bool bIncludePropertiesSection,
                                                         bool bIncludeDesignSection)
{
    if (!WidgetBlueprint)
    {
        AppendLine(TEXT("[Extract] No WidgetBlueprint provided."));
        return false;
    }

    if (!WidgetBlueprint->WidgetTree || !WidgetBlueprint->WidgetTree->RootWidget)
    {
        AppendLine(FString::Printf(TEXT("[Extract] WidgetBlueprint '%s' has no WidgetTree/RootWidget."), *WidgetBlueprint->GetName()));
        return false;
    }

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("BlueprintName"), WidgetBlueprint->GetName());
    Root->SetStringField(TEXT("Version"), TEXT("1.0.0"));
    if (bIncludeSourceAssetField)
    {
        // Not part of GetWidgetSpec in project, but still useful for traceability when enabled.
        Root->SetStringField(TEXT("SourceAsset"), WidgetBlueprint->GetPathName());
    }

    if (WidgetBlueprint->ParentClass)
    {
        Root->SetStringField(TEXT("ParentClass"), WidgetBlueprint->ParentClass->GetPathName());
    }

    // Match GetWidgetSpec: DesignerPreview exists at root.
    // Extract from the blueprint when possible instead of hardcoding.
    MWCS_ExportDesignerPreview(WidgetBlueprint, Root);

    // Design entries are stored at the root keyed by widget name.
    TSharedPtr<FJsonObject> DesignMap;
    if (bIncludeDesignSection)
    {
        DesignMap = MakeShared<FJsonObject>();
        Root->SetObjectField(TEXT("Design"), DesignMap);
    }

    // Dependencies gathered from brushes/fonts during export.
    TSet<FString> DependencySet;

    if (TSharedPtr<FJsonObject> HierarchyObj = MWCS_ExportWidgetRecursive(WidgetBlueprint->WidgetTree->RootWidget,
                                                                          DesignMap,
                                                                          DependencySet,
                                                                          bIncludeSlotLayout,
                                                                          bIncludeCanvasSlot,
                                                                          bIncludeTextProperties,
                                                                          bIncludeWidgetClassPaths,
                                                                          bIncludeIsVariable,
                                                                          bIncludePropertiesSection,
                                                                          bIncludeDesignSection))
    {
        // Wrap root node to match expected schema: { "Hierarchy": { "Root": { ... } } }
        TSharedPtr<FJsonObject> HierarchyWrapper = MakeShared<FJsonObject>();
        HierarchyWrapper->SetObjectField(TEXT("Root"), HierarchyObj);
        Root->SetObjectField(TEXT("Hierarchy"), HierarchyWrapper);
    }

    // Dependencies array (best-effort).
    {
        TArray<TSharedPtr<FJsonValue>> DepsArr;
        for (const FString &Dep : DependencySet)
        {
            DepsArr.Add(MakeShared<FJsonValueString>(Dep));
        }
        Root->SetArrayField(TEXT("Dependencies"), DepsArr);
    }

    FString JsonOut;
    {
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonOut);
        if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
        {
            AppendLine(TEXT("[Extract] Failed to serialize JSON."));
            return false;
        }
    }

    if (bShowOutputWindow)
    {
        MWCS_ShowJsonOutputWindow(FString::Printf(TEXT("MWCS Extract: %s"), *WidgetBlueprint->GetName()), JsonOut);
        AppendLine(TEXT("[Extract] Opened JSON output window."));
    }

    FString OutputPath;
    if (bWriteToFile)
    {
        const FString OutputDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MWCS"), TEXT("ExtractedSpecs"));
        IFileManager::Get().MakeDirectory(*OutputDir, true);

        OutputPath = FPaths::Combine(OutputDir, WidgetBlueprint->GetName() + TEXT(".json"));
        if (!FFileHelper::SaveStringToFile(JsonOut, *OutputPath))
        {
            AppendLine(FString::Printf(TEXT("[Extract] Failed to write: %s"), *OutputPath));
            return false;
        }
        AppendLine(FString::Printf(TEXT("[Extract] Wrote extracted spec JSON: %s"), *OutputPath));
    }

    if (bCopyToClipboard)
    {
        FPlatformApplicationMisc::ClipboardCopy(*JsonOut);
        AppendLine(TEXT("[Extract] Copied JSON to clipboard."));
    }

    if (!bWriteToFile && !bCopyToClipboard)
    {
        AppendLine(TEXT("[Extract] Exported JSON built (no outputs enabled)."));
    }

    AppendLine(FString::Printf(TEXT("[Extract] Options: Write=%s Clipboard=%s Window=%s Slot=%s Canvas=%s Text=%s UserWidgetClass=%s IsVariable=%s Properties=%s Design=%s"),
                               bWriteToFile ? TEXT("true") : TEXT("false"),
                               bCopyToClipboard ? TEXT("true") : TEXT("false"),
                               bShowOutputWindow ? TEXT("true") : TEXT("false"),
                               bIncludeSlotLayout ? TEXT("true") : TEXT("false"),
                               bIncludeCanvasSlot ? TEXT("true") : TEXT("false"),
                               bIncludeTextProperties ? TEXT("true") : TEXT("false"),
                               bIncludeWidgetClassPaths ? TEXT("true") : TEXT("false"),
                               bIncludeIsVariable ? TEXT("true") : TEXT("false"),
                               bIncludePropertiesSection ? TEXT("true") : TEXT("false"),
                               bIncludeDesignSection ? TEXT("true") : TEXT("false")));
    return true;
}

void UMWCS_ToolEUW::RefreshSettingsSummary()
{
    if (!SettingsSummaryText)
    {
        return;
    }

    SettingsSummaryText->SetText(FText::FromString(BuildSettingsSummaryString()));
}

void UMWCS_ToolEUW::AppendLine(const FString &Line)
{
    if (!OutputLog)
    {
        return;
    }

    FString Existing = OutputLog->GetText().ToString();
    if (!Existing.IsEmpty())
    {
        Existing += TEXT("\n");
    }
    Existing += Line;
    OutputLog->SetText(FText::FromString(Existing));
}

void UMWCS_ToolEUW::AppendReport(const FString &Title, const FMWCS_Report &Report)
{
    AppendLine(FString::Printf(TEXT("[%s] Specs=%d Created=%d Repaired=%d Recreated=%d Errors=%d Warnings=%d"),
                               *Title,
                               Report.SpecsProcessed,
                               Report.AssetsCreated,
                               Report.AssetsRepaired,
                               Report.AssetsRecreated,
                               Report.NumErrors(),
                               Report.NumWarnings()));

    for (const FMWCS_Issue &Issue : Report.Issues)
    {
        AppendLine(FString::Printf(TEXT("- %s: %s (%s)"), *Issue.Code, *Issue.Message, *Issue.Context));
    }
}

void UMWCS_ToolEUW::HandleOpenSettingsClicked()
{
    const UMWCS_Settings *Settings = UMWCS_Settings::Get();
    if (!Settings)
    {
        AppendLine(TEXT("[Settings] Could not resolve UMWCS_Settings"));
        return;
    }

    ISettingsModule *SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings"));
    if (!SettingsModule)
    {
        AppendLine(TEXT("[Settings] Settings module not available"));
        return;
    }

    SettingsModule->ShowViewer(Settings->GetContainerName(), Settings->GetCategoryName(), Settings->GetSectionName());
}

void UMWCS_ToolEUW::HandleValidateClicked()
{
    RefreshSettingsSummary();
    AppendReport(TEXT("Validate"), FMWCS_Service::Get().ValidateAll());
}

void UMWCS_ToolEUW::HandleCreateMissingClicked()
{
    RefreshSettingsSummary();
    AppendReport(TEXT("CreateMissing"), FMWCS_Service::Get().BuildAll(EMWCS_BuildMode::CreateMissing));
}

void UMWCS_ToolEUW::HandleRepairClicked()
{
    RefreshSettingsSummary();
    AppendReport(TEXT("Repair"), FMWCS_Service::Get().BuildAll(EMWCS_BuildMode::Repair));
}

void UMWCS_ToolEUW::HandleForceRecreateClicked()
{
    RefreshSettingsSummary();
    AppendReport(TEXT("ForceRecreate"), FMWCS_Service::Get().BuildAll(EMWCS_BuildMode::ForceRecreate));
}

void UMWCS_ToolEUW::HandleGenerateToolEuwClicked()
{
    RefreshSettingsSummary();
    AppendReport(TEXT("ToolEUW"), FMWCS_Service::Get().GenerateOrRepairToolEuw());
}

void UMWCS_ToolEUW::HandleExtractSelectedWbpClicked()
{
    RefreshSettingsSummary();

    const TArray<UObject *> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
    if (SelectedAssets.Num() == 0)
    {
        AppendLine(TEXT("[Extract] No assets selected. Select a Widget Blueprint in the Content Browser."));
        return;
    }

    for (UObject *Asset : SelectedAssets)
    {
        if (UWidgetBlueprint *WidgetBP = Cast<UWidgetBlueprint>(Asset))
        {
            ExportWidgetBlueprintHierarchyToJson(WidgetBP);
            return;
        }
    }

    AppendLine(TEXT("[Extract] Selection contains no Widget Blueprint. Select a WBP_* asset and try again."));
}
