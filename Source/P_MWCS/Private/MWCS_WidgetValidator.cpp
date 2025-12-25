#include "MWCS_WidgetValidator.h"

#include "MWCS_Settings.h"
#include "MWCS_Utilities.h"
using namespace MWCS_Utilities;

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/ComboBoxString.h"
#include "Components/ContentWidget.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/Throbber.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Misc/PackageName.h"
#include "Styling/SlateBrush.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"

static FString MWCS_NormalizeWidgetTypeForValidation(const UWidget *Widget)
{
    if (!Widget)
    {
        return TEXT("<null>");
    }

    // Keep in sync with the exporter/builder's supported types.
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

static void MWCS_CollectWidgetChildrenForValidation(UWidget *Widget, TArray<UWidget *> &OutChildren)
{
    OutChildren.Reset();
    if (!Widget)
    {
        return;
    }

    if (UPanelWidget *Panel = Cast<UPanelWidget>(Widget))
    {
        const int32 Count = Panel->GetChildrenCount();
        OutChildren.Reserve(Count);
        for (int32 i = 0; i < Count; ++i)
        {
            if (UWidget *Child = Panel->GetChildAt(i))
            {
                OutChildren.Add(Child);
            }
        }
        return;
    }

    if (UContentWidget *Content = Cast<UContentWidget>(Widget))
    {
        if (UWidget *Child = Content->GetContent())
        {
            OutChildren.Add(Child);
        }
        return;
    }
}

static bool MWCS_NearlyEqual(float A, float B, float Eps = 0.001f)
{
    return FMath::Abs(A - B) <= Eps;
}

static bool MWCS_NearlyEqualVec2(const FVector2D &A, const FVector2D &B, float Eps = 0.001f)
{
    return MWCS_NearlyEqual(A.X, B.X, Eps) && MWCS_NearlyEqual(A.Y, B.Y, Eps);
}

static bool MWCS_NearlyEqualColor(const FLinearColor &A, const FLinearColor &B, float Eps = 0.001f)
{
    return MWCS_NearlyEqual(A.R, B.R, Eps) && MWCS_NearlyEqual(A.G, B.G, Eps) && MWCS_NearlyEqual(A.B, B.B, Eps) && MWCS_NearlyEqual(A.A, B.A, Eps);
}

static bool MWCS_TryParseLinearColor(const TSharedPtr<FJsonObject> &Obj, FLinearColor &Out)
{
    if (!Obj.IsValid())
    {
        return false;
    }

    double R = 0.0, G = 0.0, B = 0.0, A = 1.0;
    if (!Obj->TryGetNumberField(TEXT("R"), R) || !Obj->TryGetNumberField(TEXT("G"), G) || !Obj->TryGetNumberField(TEXT("B"), B))
    {
        return false;
    }
    Obj->TryGetNumberField(TEXT("A"), A);
    Out = FLinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B), static_cast<float>(A));
    return true;
}

static bool MWCS_TryParseVector2D(const TSharedPtr<FJsonObject> &Obj, FVector2D &Out)
{
    if (!Obj.IsValid())
    {
        return false;
    }

    double X = 0.0, Y = 0.0;
    if (!Obj->TryGetNumberField(TEXT("X"), X) || !Obj->TryGetNumberField(TEXT("Y"), Y))
    {
        return false;
    }
    Out = FVector2D(static_cast<float>(X), static_cast<float>(Y));
    return true;
}

static bool MWCS_TryParsePaddingObject(const TSharedPtr<FJsonObject> &Obj, FMargin &Out)
{
    if (!Obj.IsValid())
    {
        return false;
    }

    double L = 0.0, T = 0.0, R = 0.0, B = 0.0;
    const bool bAny =
        Obj->TryGetNumberField(TEXT("Left"), L) |
        Obj->TryGetNumberField(TEXT("Top"), T) |
        Obj->TryGetNumberField(TEXT("Right"), R) |
        Obj->TryGetNumberField(TEXT("Bottom"), B);

    if (!bAny)
    {
        return false;
    }

    Out = FMargin(static_cast<float>(L), static_cast<float>(T), static_cast<float>(R), static_cast<float>(B));
    return true;
}

static void MWCS_ValidateDesignForWidget(const FName WidgetName,
                                         const TSharedPtr<FJsonObject> &DesignObj,
                                         UWidget *Widget,
                                         FMWCS_Report &Report,
                                         const FString &Context)
{
    if (!Widget || !DesignObj.IsValid())
    {
        return;
    }

    const FString WidgetCtx = FString::Printf(TEXT("%s::%s"), *Context, *WidgetName.ToString());

    if (UButton *Button = Cast<UButton>(Widget))
    {
        bool bExpectedFocusable = Button->GetIsFocusable();
        if (DesignObj->TryGetBoolField(TEXT("IsFocusable"), bExpectedFocusable))
        {
            const bool bActualFocusable = Button->GetIsFocusable();
            if (bActualFocusable != bExpectedFocusable)
            {
                AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Validator.Design.Button.IsFocusableMismatch"),
                         FString::Printf(TEXT("IsFocusable mismatch (expected=%s actual=%s)."), bExpectedFocusable ? TEXT("true") : TEXT("false"), bActualFocusable ? TEXT("true") : TEXT("false")),
                         WidgetCtx);
            }
        }

        const TSharedPtr<FJsonObject> *StyleObjPtr = nullptr;
        if (DesignObj->TryGetObjectField(TEXT("Style"), StyleObjPtr) && StyleObjPtr && StyleObjPtr->IsValid())
        {
            const FButtonStyle Style = Button->GetStyle();

            auto ValidateStateTint = [&](const TCHAR *StateName, const FSlateBrush &Brush)
            {
                const TSharedPtr<FJsonObject> *StateObjPtr = nullptr;
                if (!(*StyleObjPtr)->TryGetObjectField(StateName, StateObjPtr) || !StateObjPtr || !StateObjPtr->IsValid())
                {
                    return;
                }
                const TSharedPtr<FJsonObject> *TintObjPtr = nullptr;
                if (!(*StateObjPtr)->TryGetObjectField(TEXT("TintColor"), TintObjPtr) || !TintObjPtr || !TintObjPtr->IsValid())
                {
                    return;
                }
                FLinearColor ExpectedTint;
                if (!MWCS_TryParseLinearColor(*TintObjPtr, ExpectedTint))
                {
                    return;
                }

                const FLinearColor ActualTint = Brush.TintColor.GetSpecifiedColor();
                if (!MWCS_NearlyEqualColor(ActualTint, ExpectedTint))
                {
                    AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Validator.Design.Button.StyleTintMismatch"),
                             FString::Printf(TEXT("%s.TintColor mismatch."), StateName),
                             WidgetCtx);
                }
            };

            ValidateStateTint(TEXT("Normal"), Style.Normal);
            ValidateStateTint(TEXT("Hovered"), Style.Hovered);
            ValidateStateTint(TEXT("Pressed"), Style.Pressed);
        }
        return;
    }

    if (UImage *Img = Cast<UImage>(Widget))
    {
        const TSharedPtr<FJsonObject> *SizeObjPtr = nullptr;
        if (DesignObj->TryGetObjectField(TEXT("Size"), SizeObjPtr) && SizeObjPtr && SizeObjPtr->IsValid())
        {
            FVector2D ExpectedSize;
            if (MWCS_TryParseVector2D(*SizeObjPtr, ExpectedSize))
            {
                const FVector2D ActualSize = Img->GetBrush().ImageSize;
                if (!MWCS_NearlyEqualVec2(ActualSize, ExpectedSize))
                {
                    AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Validator.Design.Image.SizeMismatch"), TEXT("ImageSize mismatch."), WidgetCtx);
                }
            }
        }

        const TSharedPtr<FJsonObject> *ColorObjPtr = nullptr;
        if (DesignObj->TryGetObjectField(TEXT("ColorAndOpacity"), ColorObjPtr) && ColorObjPtr && ColorObjPtr->IsValid())
        {
            FLinearColor Expected;
            if (MWCS_TryParseLinearColor(*ColorObjPtr, Expected))
            {
                const FLinearColor Actual = Img->GetColorAndOpacity();
                if (!MWCS_NearlyEqualColor(Actual, Expected))
                {
                    AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Validator.Design.Image.ColorMismatch"), TEXT("ColorAndOpacity mismatch."), WidgetCtx);
                }
            }
        }
        return;
    }

    if (UTextBlock *TB = Cast<UTextBlock>(Widget))
    {
        const TSharedPtr<FJsonObject> *FontObjPtr = nullptr;
        if (DesignObj->TryGetObjectField(TEXT("Font"), FontObjPtr) && FontObjPtr && FontObjPtr->IsValid())
        {
            const FSlateFontInfo ActualFont = TB->GetFont();

            double ExpectedSizeD = 0.0;
            if ((*FontObjPtr)->TryGetNumberField(TEXT("Size"), ExpectedSizeD))
            {
                const int32 ExpectedSize = FMath::Max(0, static_cast<int32>(ExpectedSizeD));
                if (ActualFont.Size != ExpectedSize)
                {
                    AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Validator.Design.TextBlock.FontSizeMismatch"), TEXT("Font.Size mismatch."), WidgetCtx);
                }
            }

            FString ExpectedTypeface;
            if ((*FontObjPtr)->TryGetStringField(TEXT("Typeface"), ExpectedTypeface) && !ExpectedTypeface.IsEmpty())
            {
                if (ActualFont.TypefaceFontName != FName(*ExpectedTypeface))
                {
                    AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Validator.Design.TextBlock.TypefaceMismatch"), TEXT("Font.Typeface mismatch."), WidgetCtx);
                }
            }
        }

        const TSharedPtr<FJsonObject> *ColorObjPtr = nullptr;
        if (DesignObj->TryGetObjectField(TEXT("ColorAndOpacity"), ColorObjPtr) && ColorObjPtr && ColorObjPtr->IsValid())
        {
            FLinearColor Expected;
            if (MWCS_TryParseLinearColor(*ColorObjPtr, Expected))
            {
                const FLinearColor Actual = TB->GetColorAndOpacity().GetSpecifiedColor();
                if (!MWCS_NearlyEqualColor(Actual, Expected))
                {
                    AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Validator.Design.TextBlock.ColorMismatch"), TEXT("ColorAndOpacity mismatch."), WidgetCtx);
                }
            }
        }
        return;
    }

    if (UBorder *Border = Cast<UBorder>(Widget))
    {
        const TSharedPtr<FJsonObject> *ColorObjPtr = nullptr;
        if (DesignObj->TryGetObjectField(TEXT("BrushColor"), ColorObjPtr) && ColorObjPtr && ColorObjPtr->IsValid())
        {
            FLinearColor Expected;
            if (MWCS_TryParseLinearColor(*ColorObjPtr, Expected))
            {
                const FLinearColor Actual = Border->GetBrushColor();
                if (!MWCS_NearlyEqualColor(Actual, Expected))
                {
                    AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Validator.Design.Border.BrushColorMismatch"), TEXT("BrushColor mismatch."), WidgetCtx);
                }
            }
        }

        const TSharedPtr<FJsonObject> *PaddingObjPtr = nullptr;
        if (DesignObj->TryGetObjectField(TEXT("Padding"), PaddingObjPtr) && PaddingObjPtr && PaddingObjPtr->IsValid())
        {
            FMargin Expected;
            if (MWCS_TryParsePaddingObject(*PaddingObjPtr, Expected))
            {
                const FMargin Actual = Border->GetPadding();
                if (!MWCS_NearlyEqual(Actual.Left, Expected.Left) || !MWCS_NearlyEqual(Actual.Top, Expected.Top) || !MWCS_NearlyEqual(Actual.Right, Expected.Right) || !MWCS_NearlyEqual(Actual.Bottom, Expected.Bottom))
                {
                    AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Validator.Design.Border.PaddingMismatch"), TEXT("Padding mismatch."), WidgetCtx);
                }
            }
        }
        return;
    }
}

static void MWCS_ValidateHierarchyRecursive(const FMWCS_HierarchyNode &Expected,
                                            UWidget *Actual,
                                            FMWCS_Report &Report,
                                            const FString &Context)
{
    if (!Actual)
    {
        AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Validator.Hierarchy.MissingWidget"), TEXT("Actual widget is null while validating hierarchy."), Context);
        return;
    }

    const FString ActualType = MWCS_NormalizeWidgetTypeForValidation(Actual);
    const FString ExpectedType = Expected.Type.ToString();
    if (!ExpectedType.IsEmpty() && !ActualType.Equals(ExpectedType, ESearchCase::IgnoreCase))
    {
        AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Validator.Hierarchy.TypeMismatch"),
                 FString::Printf(TEXT("Type mismatch (expected=%s actual=%s)."), *ExpectedType, *ActualType),
                 Context);
    }

    if (Expected.Name != NAME_None && Actual->GetFName() != Expected.Name)
    {
        AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Validator.Hierarchy.NameMismatch"),
                 FString::Printf(TEXT("Name mismatch (expected=%s actual=%s)."), *Expected.Name.ToString(), *Actual->GetFName().ToString()),
                 Context);
    }

    // For nested user widgets, validate class path if present in spec.
    if (!Expected.WidgetClassPath.IsEmpty() && ActualType.Equals(TEXT("UserWidget"), ESearchCase::IgnoreCase))
    {
        UClass *ExpectedClass = StaticLoadClass(UUserWidget::StaticClass(), nullptr, *Expected.WidgetClassPath);
        UClass *ActualClass = Actual->GetClass();
        if (ExpectedClass)
        {
            // Generated Widget Blueprints will have a /Game/..._C class, but should still derive from
            // the spec provider's native class (/Script/...). Accept derived classes.
            if (!ActualClass || !ActualClass->IsChildOf(ExpectedClass))
            {
                const FString ActualClassPath = ActualClass ? ActualClass->GetPathName() : TEXT("<null>");
                AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Validator.Hierarchy.UserWidgetClassMismatch"),
                         FString::Printf(TEXT("WidgetClass mismatch (expected base=%s actual=%s)."), *ExpectedClass->GetPathName(), *ActualClassPath),
                         Context);
            }
        }
        else
        {
            // Fallback: if the expected class cannot be loaded, keep the strict string comparison.
            const FString ActualClassPath = ActualClass ? ActualClass->GetPathName() : TEXT("<null>");
            if (!ActualClassPath.Equals(Expected.WidgetClassPath, ESearchCase::IgnoreCase))
            {
                AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Validator.Hierarchy.UserWidgetClassMismatch"),
                         FString::Printf(TEXT("WidgetClass mismatch (expected=%s actual=%s)."), *Expected.WidgetClassPath, *ActualClassPath),
                         Context);
            }
        }
    }

    TArray<UWidget *> ActualChildren;
    MWCS_CollectWidgetChildrenForValidation(Actual, ActualChildren);

    const int32 ExpectedCount = Expected.Children.Num();
    const int32 ActualCount = ActualChildren.Num();
    if (ExpectedCount != ActualCount)
    {
        AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Validator.Hierarchy.ChildrenCountMismatch"),
                 FString::Printf(TEXT("Children count mismatch (expected=%d actual=%d)."), ExpectedCount, ActualCount),
                 Context);
    }

    const int32 CountToCompare = FMath::Min(ExpectedCount, ActualCount);
    for (int32 i = 0; i < CountToCompare; ++i)
    {
        const FMWCS_HierarchyNode &ExpectedChild = Expected.Children[i];
        UWidget *ActualChild = ActualChildren[i];
        const FString ChildCtx = FString::Printf(TEXT("%s/Child[%d]"), *Context, i);
        MWCS_ValidateHierarchyRecursive(ExpectedChild, ActualChild, Report, ChildCtx);
    }
}

static void MWCS_ValidateDesignerPreview(UWidgetBlueprint *BP, const FMWCS_WidgetSpec &Spec, FMWCS_Report &Report, const FString &Context)
{
#if WITH_EDITORONLY_DATA
    if (!BP || Spec.bIsToolEUW)
    {
        return;
    }

    UWidgetBlueprintGeneratedClass *GeneratedClass = Cast<UWidgetBlueprintGeneratedClass>(BP->GeneratedClass);
    if (!GeneratedClass)
    {
        AddIssue(Report, EMWCS_IssueSeverity::Warning, TEXT("Validator.DesignerPreview.NoGeneratedClass"), TEXT("No GeneratedClass; cannot validate designer preview sizing."), Context);
        return;
    }

    UUserWidget *CDO = Cast<UUserWidget>(GeneratedClass->GetDefaultObject());
    if (!CDO)
    {
        AddIssue(Report, EMWCS_IssueSeverity::Warning, TEXT("Validator.DesignerPreview.NoCDO"), TEXT("GeneratedClass CDO is not a UUserWidget; cannot validate designer preview sizing."), Context);
        return;
    }

    EDesignPreviewSizeMode ExpectedMode = EDesignPreviewSizeMode::FillScreen;
    switch (Spec.DesignerPreview.SizeMode)
    {
    case EMWCS_PreviewSizeMode::Custom:
        ExpectedMode = EDesignPreviewSizeMode::Custom;
        break;
    case EMWCS_PreviewSizeMode::Desired:
        ExpectedMode = EDesignPreviewSizeMode::Desired;
        break;
    case EMWCS_PreviewSizeMode::DesiredOnScreen:
        ExpectedMode = EDesignPreviewSizeMode::DesiredOnScreen;
        break;
    case EMWCS_PreviewSizeMode::FillScreen:
    default:
        ExpectedMode = EDesignPreviewSizeMode::FillScreen;
        break;
    }

    if (CDO->DesignSizeMode != ExpectedMode)
    {
        AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Validator.DesignerPreview.SizeModeMismatch"), TEXT("Designer preview SizeMode mismatch."), Context);
    }

    if (ExpectedMode == EDesignPreviewSizeMode::Custom)
    {
        const FVector2D ExpectedSize = Spec.DesignerPreview.CustomSize;
        const FVector2D ActualSize = CDO->DesignTimeSize;
        if (!MWCS_NearlyEqualVec2(ExpectedSize, ActualSize))
        {
            AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Validator.DesignerPreview.CustomSizeMismatch"), TEXT("Designer preview CustomSize mismatch."), Context);
        }
    }
#else
    (void)BP;
    (void)Spec;
    (void)Report;
    (void)Context;
#endif
}

static bool CollectRequiredWidgetNames(const FMWCS_HierarchyNode &Node, TSet<FName> &OutNames)
{
    if (Node.Name != NAME_None)
    {
        OutNames.Add(Node.Name);
    }
    for (const FMWCS_HierarchyNode &Child : Node.Children)
    {
        CollectRequiredWidgetNames(Child, OutNames);
    }
    return true;
}

bool FMWCS_WidgetValidator::ValidateSpecAsset(const FMWCS_WidgetSpec &Spec, FMWCS_Report &InOutReport)
{
    const UMWCS_Settings *Settings = UMWCS_Settings::Get();
    if (!Settings)
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Settings.Missing"), TEXT("MWCS settings not available."), TEXT("Validator"));
        return false;
    }

    FString PackagePath;
    if (!EnsureValidPackagePath(Settings->OutputRootPath, PackagePath))
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Validator.InvalidOutputPath"), TEXT("OutputRootPath is invalid."), Settings->OutputRootPath);
        return false;
    }

    const FString AssetName = Spec.BlueprintName.ToString();
    const FString Context = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);

    FAssetData AssetData;
    if (!FindAssetData(PackagePath, AssetName, AssetData))
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Validator.MissingAsset"), TEXT("Widget Blueprint asset does not exist."), Context);
        return false;
    }

    UWidgetBlueprint *BP = Cast<UWidgetBlueprint>(AssetData.GetAsset());
    if (!BP)
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Validator.WrongType"), TEXT("Asset is not a Widget Blueprint."), Context);
        return false;
    }

    UClass *ExpectedParent = FSoftClassPath(Spec.ParentClassPath).TryLoadClass<UUserWidget>();
    if (!ExpectedParent)
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Validator.ParentLoadFailed"), TEXT("Failed to load expected ParentClass."), Spec.ParentClassPath);
        return false;
    }

    if (BP->ParentClass != ExpectedParent)
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Validator.ParentMismatch"), TEXT("Blueprint ParentClass does not match spec."), Context);
    }

    if (!BP->WidgetTree)
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Validator.MissingWidgetTree"), TEXT("Blueprint has no WidgetTree."), Context);
        return false;
    }

    // Validate designer preview sizing (supported parity set).
    MWCS_ValidateDesignerPreview(BP, Spec, InOutReport, Context);

    // Validate hierarchy structure/types (supported parity set).
    if (BP->WidgetTree->RootWidget)
    {
        MWCS_ValidateHierarchyRecursive(Spec.HierarchyRoot, BP->WidgetTree->RootWidget, InOutReport, Context + TEXT("::Hierarchy"));
    }
    else
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Validator.MissingRootWidget"), TEXT("Blueprint WidgetTree has no RootWidget."), Context);
    }

    TSet<FName> RequiredNames;
    CollectRequiredWidgetNames(Spec.HierarchyRoot, RequiredNames);
    for (const FName &BindName : Spec.Bindings.Required)
    {
        RequiredNames.Add(BindName);
    }

    for (const FName &Name : RequiredNames)
    {
        if (Name == NAME_None)
        {
            continue;
        }
        if (!BP->WidgetTree->FindWidget(Name))
        {
            AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Validator.MissingWidget"), FString::Printf(TEXT("Missing widget: %s"), *Name.ToString()), Context);
        }
    }

    // Validate supported Design section (per-widget keyed by widget name).
    for (const TPair<FName, TSharedPtr<FJsonObject>> &KV : Spec.Design)
    {
        if (KV.Key == NAME_None)
        {
            continue;
        }

        UWidget *Widget = BP->WidgetTree->FindWidget(KV.Key);
        if (!Widget)
        {
            AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Validator.Design.MissingWidget"), FString::Printf(TEXT("Design entry references missing widget: %s"), *KV.Key.ToString()), Context);
            continue;
        }

        MWCS_ValidateDesignForWidget(KV.Key, KV.Value, Widget, InOutReport, Context);
    }

    // Validate Dependencies (best-effort): ensure each dependency path resolves.
    if (Spec.Dependencies.Num() > 0)
    {
        FAssetRegistryModule &AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        for (const FString &Dep : Spec.Dependencies)
        {
            if (Dep.IsEmpty())
            {
                continue;
            }

            const FSoftObjectPath ObjPath(Dep);
            if (!ObjPath.IsValid())
            {
                AddIssue(InOutReport, EMWCS_IssueSeverity::Warning, TEXT("Validator.Dependencies.InvalidPath"), FString::Printf(TEXT("Dependency path is not a valid object path: %s"), *Dep), Context);
                continue;
            }

            const FAssetData DepAsset = AssetRegistry.Get().GetAssetByObjectPath(ObjPath);
            if (!DepAsset.IsValid())
            {
                AddIssue(InOutReport, EMWCS_IssueSeverity::Warning, TEXT("Validator.Dependencies.MissingAsset"), FString::Printf(TEXT("Dependency asset not found: %s"), *Dep), Context);
            }
        }
    }

    if (!BP->GeneratedClass)
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Warning, TEXT("Validator.NoGeneratedClass"), TEXT("Blueprint has no GeneratedClass (compile may be required)."), Context);
    }

    return !InOutReport.HasErrors();
}
