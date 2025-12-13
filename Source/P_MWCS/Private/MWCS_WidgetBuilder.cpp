#include "MWCS_WidgetBuilder.h"

#include "MWCS_Settings.h"

#include "UObject/SavePackage.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/UserWidget.h"
#include "EditorUtilityWidget.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "EditorUtilityWidgetBlueprintFactory.h"
#include "Editor/UMGEditor/Classes/WidgetBlueprintFactory.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Blueprint/WidgetTree.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/ContentWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/Overlay.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/Throbber.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ScrollBox.h"

#include "Components/BorderSlot.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBoxSlot.h"

static void AddIssue(FMWCS_Report &Report, EMWCS_IssueSeverity Severity, const FString &Code, const FString &Message, const FString &Context)
{
    FMWCS_Issue Issue;
    Issue.Severity = Severity;
    Issue.Code = Code;
    Issue.Message = Message;
    Issue.Context = Context;
    Report.Issues.Add(MoveTemp(Issue));
}

static UClass *ResolveParentClass(const FString &ParentClassPath)
{
    FSoftClassPath SCP(ParentClassPath);
    UClass *Loaded = SCP.TryLoadClass<UUserWidget>();
    return Loaded;
}

static UClass *ResolveEuwParentClass(const FString &ParentClassPath)
{
    FSoftClassPath SCP(ParentClassPath);
    UClass *Loaded = SCP.TryLoadClass<UEditorUtilityWidget>();
    return Loaded;
}

static void GetToolEuwContractNames(TSet<FName> &Out)
{
    Out.Reset();
    Out.Add(TEXT("TitleText"));
    Out.Add(TEXT("SettingsSummaryText"));
    Out.Add(TEXT("Btn_Validate"));
    Out.Add(TEXT("Btn_CreateMissing"));
    Out.Add(TEXT("Btn_Repair"));
    Out.Add(TEXT("Btn_ForceRecreate"));
    Out.Add(TEXT("Btn_GenerateToolEUW"));
    Out.Add(TEXT("Btn_OpenSettings"));
    Out.Add(TEXT("OutputLog"));
}

static bool EnsureValidPackagePath(const FString &Path, FString &OutNormalized)
{
    OutNormalized = Path;
    if (OutNormalized.IsEmpty())
    {
        return false;
    }
    if (!OutNormalized.StartsWith(TEXT("/")))
    {
        OutNormalized = FString::Printf(TEXT("/%s"), *OutNormalized);
    }
    return FPackageName::IsValidLongPackageName(OutNormalized);
}

static bool FindAssetData(const FString &PackagePath, const FString &AssetName, FAssetData &OutAssetData)
{
    const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
    FAssetRegistryModule &AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    OutAssetData = AssetRegistry.Get().GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
    return OutAssetData.IsValid();
}

static bool DeleteAssetIfExists(const FString &PackagePath, const FString &AssetName, FMWCS_Report &Report, const FString &Context)
{
    FAssetData Existing;
    if (!FindAssetData(PackagePath, AssetName, Existing))
    {
        return true;
    }
    TArray<FAssetData> ToDelete;
    ToDelete.Add(Existing);
    const int32 Deleted = ObjectTools::DeleteAssets(ToDelete, /*bShowConfirmation*/ false);
    if (Deleted <= 0)
    {
        // ForceRecreate is intended to end up with a deterministic asset.
        // Asset deletion can fail in headless/editor contexts when the asset/package is still referenced or loaded.
        // Treat this as a warning and let the caller proceed with an in-place deterministic rebuild.
        AddIssue(Report, EMWCS_IssueSeverity::Warning, TEXT("Builder.DeleteFailed"), TEXT("Failed to delete existing asset for ForceRecreate; will rebuild in place."), Context);
        return false;
    }
    return true;
}

static UClass *TryResolveUserWidgetClassFromNode(const FMWCS_HierarchyNode &Node)
{
    if (Node.WidgetClassPath.IsEmpty())
    {
        return nullptr;
    }

    // Native classes in specs commonly use "/Script/Module.ClassName".
    // FSoftClassPath::TryLoadClass can be flaky for native script paths depending on load order,
    // so resolve the UClass by name first.
    if (Node.WidgetClassPath.StartsWith(TEXT("/Script/")))
    {
        FString ClassName;
        if (Node.WidgetClassPath.Split(TEXT("."), nullptr, &ClassName) && !ClassName.IsEmpty())
        {
            if (UClass *ByName = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::None))
            {
                // Prefer an existing generated widget blueprint class when available.
                // Convention: /Script/Module.ClassName -> /Game/.../WBP_ClassName
                const UMWCS_Settings *Settings = UMWCS_Settings::Get();
                if (Settings && !Settings->OutputRootPath.IsEmpty())
                {
                    const FString AssetName = FString::Printf(TEXT("WBP_%s"), *ClassName);
                    FAssetData Found;
                    if (FindAssetData(Settings->OutputRootPath, AssetName, Found))
                    {
                        if (UWidgetBlueprint *WB = Cast<UWidgetBlueprint>(Found.GetAsset()))
                        {
                            if (WB->GeneratedClass)
                            {
                                return WB->GeneratedClass;
                            }
                        }
                    }
                }

                return ByName;
            }
        }
        // Fall through to soft-load attempt below.
    }

    // Try direct class load (blueprint generated class path).
    {
        FSoftClassPath SCP(Node.WidgetClassPath);
        if (UClass *Loaded = SCP.TryLoadClass<UWidget>())
        {
            return Loaded;
        }
    }

    return nullptr;
}

static UClass *ResolveWidgetClassFromBindingType(const FString &BindingType);

static UWidget *ConstructWidget(UWidgetTree *Tree, const FMWCS_HierarchyNode &Node, const TMap<FName, FString> &BindingTypes, FMWCS_Report &Report, const FString &Context)
{
    UClass *WidgetClass = nullptr;

    const FName Type = Node.Type;
    const FName Name = Node.Name;

    if (Type == TEXT("CanvasPanel"))
        WidgetClass = UCanvasPanel::StaticClass();
    else if (Type == TEXT("VerticalBox"))
        WidgetClass = UVerticalBox::StaticClass();
    else if (Type == TEXT("HorizontalBox"))
        WidgetClass = UHorizontalBox::StaticClass();
    else if (Type == TEXT("Overlay"))
        WidgetClass = UOverlay::StaticClass();
    else if (Type == TEXT("Border"))
        WidgetClass = UBorder::StaticClass();
    else if (Type == TEXT("Button"))
        WidgetClass = UButton::StaticClass();
    else if (Type == TEXT("TextBlock"))
        WidgetClass = UTextBlock::StaticClass();
    else if (Type == TEXT("Image"))
        WidgetClass = UImage::StaticClass();
    else if (Type == TEXT("Spacer"))
        WidgetClass = USpacer::StaticClass();
    else if (Type == TEXT("MultiLineEditableTextBox"))
        WidgetClass = UMultiLineEditableTextBox::StaticClass();
    else if (Type == TEXT("ScrollBox"))
        WidgetClass = UScrollBox::StaticClass();
    else if (Type == TEXT("Throbber"))
        WidgetClass = UThrobber::StaticClass();
    else if (Type == TEXT("WidgetSwitcher"))
        WidgetClass = UWidgetSwitcher::StaticClass();
    else if (Type == TEXT("UserWidget"))
    {
        WidgetClass = TryResolveUserWidgetClassFromNode(Node);
        if (!WidgetClass && Name != NAME_None)
        {
            if (const FString *BT = BindingTypes.Find(Name))
            {
                WidgetClass = ResolveWidgetClassFromBindingType(*BT);
            }
        }
        if (!WidgetClass)
        {
            WidgetClass = UUserWidget::StaticClass();
        }
    }

    if (!WidgetClass)
    {
        AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.UnsupportedWidget"), FString::Printf(TEXT("Unsupported widget type: %s"), *Type.ToString()), Context);
        return nullptr;
    }

    const FName WidgetName = (Name == NAME_None) ? MakeUniqueObjectName(Tree, WidgetClass) : Name;
    return Tree->ConstructWidget<UWidget>(WidgetClass, WidgetName);
}

static UClass *GetWidgetClassForType(const FName Type)
{
    if (Type == TEXT("CanvasPanel"))
        return UCanvasPanel::StaticClass();
    if (Type == TEXT("VerticalBox"))
        return UVerticalBox::StaticClass();
    if (Type == TEXT("HorizontalBox"))
        return UHorizontalBox::StaticClass();
    if (Type == TEXT("Overlay"))
        return UOverlay::StaticClass();
    if (Type == TEXT("Border"))
        return UBorder::StaticClass();
    if (Type == TEXT("Button"))
        return UButton::StaticClass();
    if (Type == TEXT("TextBlock"))
        return UTextBlock::StaticClass();
    if (Type == TEXT("Image"))
        return UImage::StaticClass();
    if (Type == TEXT("Spacer"))
        return USpacer::StaticClass();
    if (Type == TEXT("MultiLineEditableTextBox"))
        return UMultiLineEditableTextBox::StaticClass();
    if (Type == TEXT("ScrollBox"))
        return UScrollBox::StaticClass();
    if (Type == TEXT("Throbber"))
        return UThrobber::StaticClass();
    if (Type == TEXT("WidgetSwitcher"))
        return UWidgetSwitcher::StaticClass();
    if (Type == TEXT("UserWidget"))
        return UUserWidget::StaticClass();
    return nullptr;
}

static UClass *ResolveWidgetClassFromBindingType(const FString &BindingType)
{
    if (BindingType.IsEmpty())
    {
        return nullptr;
    }

    // If it's a fully qualified path, try soft-load.
    if (BindingType.Contains(TEXT("/")))
    {
        FSoftClassPath SCP(BindingType);
        return SCP.TryLoadClass<UWidget>();
    }

    // Common case: "UTextBlock" or "UMF_MatchInfo".
    const FString WithoutLeadingU = BindingType.StartsWith(TEXT("U")) ? BindingType.Mid(1) : BindingType;

    if (UClass *ByExact = FindFirstObject<UClass>(*BindingType, EFindFirstObjectOptions::None))
    {
        return ByExact;
    }
    if (UClass *ByStripped = FindFirstObject<UClass>(*WithoutLeadingU, EFindFirstObjectOptions::None))
    {
        return ByStripped;
    }

    return nullptr;
}

static void EnsureWidgetVariable(UWidget *Widget, bool bIsVariable)
{
    if (Widget)
    {
        Widget->bIsVariable = bIsVariable;
    }
}

static void ApplySlotMeta(UWidget *Child, const FMWCS_HierarchyNode &Node)
{
    if (!Child)
    {
        return;
    }

    UPanelSlot *Slot = Child->Slot;
    if (!Slot)
    {
        return;
    }

    // Canvas slot
    if (UCanvasPanelSlot *CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
    {
        if (Node.bHasCanvasAnchors)
        {
            CanvasSlot->SetAnchors(FAnchors(Node.CanvasAnchorsMin.X, Node.CanvasAnchorsMin.Y, Node.CanvasAnchorsMax.X, Node.CanvasAnchorsMax.Y));
        }
        if (Node.bHasCanvasOffsets)
        {
            CanvasSlot->SetOffsets(Node.CanvasOffsets);
        }
        if (Node.bHasCanvasAlignment)
        {
            CanvasSlot->SetAlignment(Node.CanvasAlignment);
        }
        if (Node.bHasCanvasAutoSize)
        {
            CanvasSlot->SetAutoSize(Node.bCanvasAutoSize);
        }
        if (Node.bHasCanvasZOrder)
        {
            CanvasSlot->SetZOrder(Node.CanvasZOrder);
        }
    }

    auto ApplyCommon = [&Node](UPanelSlot *AnySlot)
    {
        if (!AnySlot)
        {
            return;
        }

        if (UVerticalBoxSlot *V = Cast<UVerticalBoxSlot>(AnySlot))
        {
            if (Node.bHasSlotPadding)
                V->SetPadding(Node.SlotPadding);
            if (Node.bHasSlotHAlign)
                V->SetHorizontalAlignment(Node.SlotHAlign);
            if (Node.bHasSlotVAlign)
                V->SetVerticalAlignment(Node.SlotVAlign);
            if (Node.bHasSlotSize)
            {
                FSlateChildSize Size;
                Size.SizeRule = Node.SlotSizeRule;
                Size.Value = Node.SlotSizeValue;
                V->SetSize(Size);
            }
            return;
        }
        if (UHorizontalBoxSlot *H = Cast<UHorizontalBoxSlot>(AnySlot))
        {
            if (Node.bHasSlotPadding)
                H->SetPadding(Node.SlotPadding);
            if (Node.bHasSlotHAlign)
                H->SetHorizontalAlignment(Node.SlotHAlign);
            if (Node.bHasSlotVAlign)
                H->SetVerticalAlignment(Node.SlotVAlign);
            if (Node.bHasSlotSize)
            {
                FSlateChildSize Size;
                Size.SizeRule = Node.SlotSizeRule;
                Size.Value = Node.SlotSizeValue;
                H->SetSize(Size);
            }
            return;
        }
        if (UOverlaySlot *O = Cast<UOverlaySlot>(AnySlot))
        {
            if (Node.bHasSlotPadding)
                O->SetPadding(Node.SlotPadding);
            if (Node.bHasSlotHAlign)
                O->SetHorizontalAlignment(Node.SlotHAlign);
            if (Node.bHasSlotVAlign)
                O->SetVerticalAlignment(Node.SlotVAlign);
            return;
        }
        if (UBorderSlot *B = Cast<UBorderSlot>(AnySlot))
        {
            if (Node.bHasSlotPadding)
                B->SetPadding(Node.SlotPadding);
            if (Node.bHasSlotHAlign)
                B->SetHorizontalAlignment(Node.SlotHAlign);
            if (Node.bHasSlotVAlign)
                B->SetVerticalAlignment(Node.SlotVAlign);
            return;
        }
        if (UButtonSlot *Btn = Cast<UButtonSlot>(AnySlot))
        {
            if (Node.bHasSlotPadding)
                Btn->SetPadding(Node.SlotPadding);
            if (Node.bHasSlotHAlign)
                Btn->SetHorizontalAlignment(Node.SlotHAlign);
            if (Node.bHasSlotVAlign)
                Btn->SetVerticalAlignment(Node.SlotVAlign);
            return;
        }
    };

    ApplyCommon(Slot);
}

static void ApplyTextMeta(UTextBlock *TB, const FMWCS_HierarchyNode &Node)
{
    if (!TB)
    {
        return;
    }

    if (!Node.Text.IsEmpty())
    {
        TB->SetText(FText::FromString(Node.Text));
    }

    if (Node.FontSize > 0)
    {
        FSlateFontInfo Font = TB->GetFont();
        Font.Size = Node.FontSize;
        TB->SetFont(Font);
    }

    if (!Node.Justification.IsEmpty())
    {
        if (Node.Justification.Equals(TEXT("Center"), ESearchCase::IgnoreCase))
        {
            TB->SetJustification(ETextJustify::Center);
        }
        else if (Node.Justification.Equals(TEXT("Right"), ESearchCase::IgnoreCase))
        {
            TB->SetJustification(ETextJustify::Right);
        }
        else
        {
            TB->SetJustification(ETextJustify::Left);
        }
    }
}

static bool BuildNode(
    UWidgetTree *Tree,
    const FMWCS_HierarchyNode &Node,
    UWidget *Parent,
    UClass *BlueprintParentClass,
    const TSet<FName> &RequiredVars,
    const TSet<FName> &ForceVariableNames,
    const TMap<FName, FString> &BindingTypes,
    bool bStrictNaming,
    TSet<FName> &InOutSpecNames,
    FMWCS_Report &Report,
    const FString &Context)
{
    if (!Tree)
    {
        return false;
    }

    if (Node.Name != NAME_None)
    {
        if (InOutSpecNames.Contains(Node.Name))
        {
            AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.DuplicateName"), FString::Printf(TEXT("Duplicate widget Name in spec: %s"), *Node.Name.ToString()), Context);
            return false;
        }
        InOutSpecNames.Add(Node.Name);
    }

    UWidget *Current = nullptr;
    if (Parent == nullptr)
    {
        Current = Tree->RootWidget;
        if (!Current)
        {
            Current = ConstructWidget(Tree, Node, BindingTypes, Report, Context);
            if (!Current)
            {
                return false;
            }
            Tree->RootWidget = Current;
        }
        else if (bStrictNaming && Node.Name != NAME_None && Current->GetFName() != Node.Name)
        {
            AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.RootNameMismatch"), FString::Printf(TEXT("Root widget name mismatch. Expected %s, found %s"), *Node.Name.ToString(), *Current->GetName()), Context);
            return false;
        }
    }
    else
    {
        if (Node.Name != NAME_None)
        {
            Current = Tree->FindWidget(Node.Name);
        }
        if (!Current)
        {
            Current = ConstructWidget(Tree, Node, BindingTypes, Report, Context);
            if (!Current)
            {
                return false;
            }

            if (UPanelWidget *Panel = Cast<UPanelWidget>(Parent))
            {
                Panel->AddChild(Current);
            }
            else if (UContentWidget *Content = Cast<UContentWidget>(Parent))
            {
                if (Content->GetContent() == nullptr)
                {
                    Content->SetContent(Current);

                    // Most button labels should be centered by default.
                    if (!Node.bHasSlotHAlign && !Node.bHasSlotVAlign)
                    {
                        if (Cast<UButton>(Parent))
                        {
                            if (UButtonSlot *BtnSlot = Cast<UButtonSlot>(Current->Slot))
                            {
                                BtnSlot->SetHorizontalAlignment(HAlign_Center);
                                BtnSlot->SetVerticalAlignment(VAlign_Center);
                            }
                        }
                    }
                }
                else
                {
                    AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.ContentAlreadySet"), FString::Printf(TEXT("Parent %s already has content."), *Parent->GetName()), Context);
                    return false;
                }
            }
            else
            {
                AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.NonContainerParent"), FString::Printf(TEXT("Parent %s cannot contain children."), *Parent->GetName()), Context);
                return false;
            }
        }
    }

    if (Node.Type != TEXT("UserWidget"))
    {
        if (UClass *ExpectedClass = GetWidgetClassForType(Node.Type))
        {
            if (!Current->IsA(ExpectedClass))
            {
                AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.TypeMismatch"), FString::Printf(TEXT("Widget %s is %s but spec expects %s"), *Current->GetName(), *Current->GetClass()->GetName(), *ExpectedClass->GetName()), Context);
                return false;
            }
        }
    }
    else
    {
        // For UserWidget nodes, validate that the instantiated widget matches either the explicit WidgetClassPath
        // or the binding type (when WidgetClassPath is not provided).
        UClass *ExpectedUserWidgetClass = TryResolveUserWidgetClassFromNode(Node);
        if (!ExpectedUserWidgetClass && Node.Name != NAME_None)
        {
            if (const FString *BT = BindingTypes.Find(Node.Name))
            {
                ExpectedUserWidgetClass = ResolveWidgetClassFromBindingType(*BT);
            }
        }

        if (ExpectedUserWidgetClass && !Current->IsA(ExpectedUserWidgetClass))
        {
            AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.TypeMismatch"),
                     FString::Printf(TEXT("Widget %s is %s but spec expects %s"), *Current->GetName(), *Current->GetClass()->GetName(), *ExpectedUserWidgetClass->GetName()),
                     Context);
            return false;
        }
    }

    const bool bForceVariable = (Node.Name != NAME_None) && ForceVariableNames.Contains(Node.Name);

    const bool bBoundBySpec = (Node.Name != NAME_None) && RequiredVars.Contains(Node.Name);
    if (bBoundBySpec && BlueprintParentClass)
    {
        // Guardrail: if the parent class already declares a property with this name, ensure it can
        // accept the widget instance we're building. This avoids the UMG compiler attempting to
        // create a conflicting property ("already exists" internal compiler errors).
        if (FProperty *ExistingProp = BlueprintParentClass->FindPropertyByName(Node.Name))
        {
            if (FObjectPropertyBase *ObjProp = CastField<FObjectPropertyBase>(ExistingProp))
            {
                if (UClass *PropClass = ObjProp->PropertyClass)
                {
                    if (PropClass->IsChildOf(UWidget::StaticClass()) && !Current->IsA(PropClass))
                    {
                        AddIssue(
                            Report,
                            EMWCS_IssueSeverity::Error,
                            TEXT("Builder.BindWidgetTypeMismatch"),
                            FString::Printf(
                                TEXT("BindWidget name %s exists on parent class as %s, but spec builds %s. Fix the C++ property type or the spec widget type."),
                                *Node.Name.ToString(),
                                *PropClass->GetName(),
                                *Current->GetClass()->GetName()),
                            Context);
                        return false;
                    }
                }
            }
        }
    }

    // Widgets referenced by BindWidget/BindWidgetOptional must be variables for the binding system
    // to pick them up during compilation.
    EnsureWidgetVariable(Current, bForceVariable || bBoundBySpec || Node.bIsVariable);

    // Apply slot/layout metadata when available.
    ApplySlotMeta(Current, Node);

    if (Node.Type == TEXT("TextBlock"))
    {
        if (UTextBlock *TB = Cast<UTextBlock>(Current))
        {
            ApplyTextMeta(TB, Node);
        }
    }

    for (const FMWCS_HierarchyNode &Child : Node.Children)
    {
        if (!BuildNode(Tree, Child, Current, BlueprintParentClass, RequiredVars, ForceVariableNames, BindingTypes, bStrictNaming, InOutSpecNames, Report, Context))
        {
            return false;
        }
    }

    return true;
}

static bool TrySetBoolPropertyByName(UObject *Obj, const TCHAR *PropName, bool bValue)
{
    if (!Obj)
    {
        return false;
    }

    FProperty *Prop = Obj->GetClass()->FindPropertyByName(FName(PropName));
    if (FBoolProperty *BoolProp = CastField<FBoolProperty>(Prop))
    {
        BoolProp->SetPropertyValue_InContainer(Obj, bValue);
        return true;
    }
    return false;
}

static bool TrySetIntPropertyByName(UObject *Obj, const TCHAR *PropName, int32 Value)
{
    if (!Obj)
    {
        return false;
    }

    FProperty *Prop = Obj->GetClass()->FindPropertyByName(FName(PropName));
    if (FIntProperty *IntProp = CastField<FIntProperty>(Prop))
    {
        IntProp->SetPropertyValue_InContainer(Obj, Value);
        return true;
    }
    return false;
}

static bool ApplyDesignerPreviewMetadata(UWidgetBlueprint *Blueprint, const FMWCS_WidgetSpec &Spec, FMWCS_Report &Report, const FString &Context)
{
#if WITH_EDITORONLY_DATA
    if (!Blueprint || Spec.bIsToolEUW)
    {
        return true;
    }

    UWidgetBlueprintGeneratedClass *GeneratedClass = Cast<UWidgetBlueprintGeneratedClass>(Blueprint->GeneratedClass);
    if (!GeneratedClass)
    {
        AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.DesignerPreview.NoGeneratedClass"), TEXT("No GeneratedClass available to apply designer preview settings."), Context);
        return false;
    }

    UUserWidget *CDO = Cast<UUserWidget>(GeneratedClass->GetDefaultObject());
    if (!CDO)
    {
        AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.DesignerPreview.NoCDO"), TEXT("GeneratedClass default object is not a UUserWidget; cannot apply designer preview settings."), Context);
        return false;
    }

    // --- Persistent tier: SizeMode + CustomSize (hard fail) ---
    switch (Spec.DesignerPreview.SizeMode)
    {
    case EMWCS_PreviewSizeMode::Custom:
    {
        const FVector2D Size = Spec.DesignerPreview.CustomSize;
        if (Size.X <= 0.0f || Size.Y <= 0.0f)
        {
            AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.DesignerPreview.InvalidCustomSize"), TEXT("Custom preview size is invalid; cannot apply designer preview sizing."), Context);
            return false;
        }
        CDO->DesignSizeMode = EDesignPreviewSizeMode::Custom;
        CDO->DesignTimeSize = Size;
        break;
    }
    case EMWCS_PreviewSizeMode::Desired:
        CDO->DesignSizeMode = EDesignPreviewSizeMode::Desired;
        break;
    case EMWCS_PreviewSizeMode::DesiredOnScreen:
        CDO->DesignSizeMode = EDesignPreviewSizeMode::DesiredOnScreen;
        break;
    case EMWCS_PreviewSizeMode::FillScreen:
    default:
        CDO->DesignSizeMode = EDesignPreviewSizeMode::FillScreen;
        break;
    }

    // --- Best-effort tier: ZoomLevel + ShowGrid (best-effort only) ---
    // These fields are not guaranteed to exist per-asset across UE versions.
    {
        bool bZoomSet = false;
        // Try common candidates on the WidgetBlueprint asset itself.
        bZoomSet |= TrySetIntPropertyByName(Blueprint, TEXT("ZoomLevel"), Spec.DesignerPreview.ZoomLevel);
        bZoomSet |= TrySetIntPropertyByName(Blueprint, TEXT("DesignerZoomLevel"), Spec.DesignerPreview.ZoomLevel);
        bZoomSet |= TrySetIntPropertyByName(Blueprint, TEXT("PreviewZoomLevel"), Spec.DesignerPreview.ZoomLevel);
        (void)bZoomSet;

        bool bGridSet = false;
        bGridSet |= TrySetBoolPropertyByName(Blueprint, TEXT("bShowDesignGrid"), Spec.DesignerPreview.bShowGrid);
        bGridSet |= TrySetBoolPropertyByName(Blueprint, TEXT("bShowGrid"), Spec.DesignerPreview.bShowGrid);
        bGridSet |= TrySetBoolPropertyByName(Blueprint, TEXT("bShowDesignerGrid"), Spec.DesignerPreview.bShowGrid);
        (void)bGridSet;
    }

    return true;
#else
    (void)Blueprint;
    (void)Spec;
    (void)Report;
    (void)Context;
    return true;
#endif
}

static bool CompileAndSave(UWidgetBlueprint *Blueprint, const FMWCS_WidgetSpec &Spec, FMWCS_Report &Report, const FString &Context)
{
    if (!Blueprint)
    {
        return false;
    }

    // Ensure Blueprint compilation reflects the current WidgetTree structure.
    // Without this, the UMG compiler can keep stale widget variable/binding state and emit
    // spurious "required widget binding ... was not found" warnings.
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    // Only stamp preview metadata when compilation succeeded (avoid saving partially-updated preview fields).
    if (Blueprint->Status != BS_Error)
    {
        if (!ApplyDesignerPreviewMetadata(Blueprint, Spec, Report, Context))
        {
            return false;
        }
    }
    else
    {
        AddIssue(Report, EMWCS_IssueSeverity::Warning, TEXT("Builder.CompileFailed"), TEXT("Blueprint compilation failed; skipping designer preview sizing."), Context);
    }

    UPackage *Package = Blueprint->GetOutermost();
    if (!Package)
    {
        AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.SaveNoPackage"), TEXT("Blueprint has no outermost package."), Context);
        return false;
    }

    Package->SetDirtyFlag(true);

    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_None;
    const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    if (!UPackage::SavePackage(Package, Blueprint, *Filename, Args))
    {
        AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.SaveFailed"), TEXT("Failed to save package."), Context);
        return false;
    }
    return true;
}

static void ValidateBuiltBindings(UWidgetBlueprint *Blueprint, const FMWCS_WidgetSpec &Spec, FMWCS_Report &Report, const FString &Context)
{
    if (!Blueprint || !Blueprint->WidgetTree)
    {
        return;
    }

    auto ValidateOne = [&](const FName Name, const TCHAR *SeverityLabel)
    {
        if (Name == NAME_None)
        {
            return;
        }

        UWidget *Found = Blueprint->WidgetTree->FindWidget(Name);
        if (!Found)
        {
            AddIssue(
                Report,
                EMWCS_IssueSeverity::Warning,
                TEXT("Builder.BindingWidgetMissing"),
                FString::Printf(TEXT("%s binding widget '%s' was not found in built WidgetTree."), SeverityLabel, *Name.ToString()),
                Context);
            return;
        }

        if (!Found->bIsVariable)
        {
            AddIssue(
                Report,
                EMWCS_IssueSeverity::Warning,
                TEXT("Builder.BindingWidgetNotVariable"),
                FString::Printf(TEXT("%s binding widget '%s' exists but is not marked as variable (bIsVariable=false)."), SeverityLabel, *Name.ToString()),
                Context);
        }

        if (const FString *BT = Spec.Bindings.Types.Find(Name))
        {
            if (UClass *Expected = ResolveWidgetClassFromBindingType(*BT))
            {
                if (!Found->IsA(Expected))
                {
                    AddIssue(
                        Report,
                        EMWCS_IssueSeverity::Warning,
                        TEXT("Builder.BindingWidgetTypeMismatch"),
                        FString::Printf(TEXT("%s binding widget '%s' is %s but bindings expect %s."), SeverityLabel, *Name.ToString(), *Found->GetClass()->GetName(), *Expected->GetName()),
                        Context);
                }
            }
        }
    };

    for (const FName &N : Spec.Bindings.Required)
    {
        ValidateOne(N, TEXT("Required"));
    }
    for (const FName &N : Spec.Bindings.Optional)
    {
        ValidateOne(N, TEXT("Optional"));
    }
}

static bool CreateOrUpdateInternal(const FString &PackagePath, const FString &AssetName, UClass *AssetClass, UFactory *Factory, UClass *ParentClass, const FMWCS_WidgetSpec &Spec, EMWCS_BuildMode Mode, FMWCS_Report &Report, const FString &Context)
{
    if (!Factory || !AssetClass)
    {
        AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.FactoryMissing"), TEXT("Asset factory or class missing."), Context);
        return false;
    }

    auto ModeToString = [](EMWCS_BuildMode InMode) -> const TCHAR *
    {
        switch (InMode)
        {
        case EMWCS_BuildMode::CreateMissing:
            return TEXT("CreateMissing");
        case EMWCS_BuildMode::Repair:
            return TEXT("Repair");
        case EMWCS_BuildMode::ForceRecreate:
            return TEXT("ForceRecreate");
        default:
            return TEXT("Unknown");
        }
    };

    if (Mode == EMWCS_BuildMode::ForceRecreate)
    {
        // ForceRecreate is implemented as an in-place deterministic rebuild.
        // Asset deletion frequently fails in headless/editor commandlets (loaded packages/references),
        // and those failures emit engine warnings that break CI when using -FailOnWarnings.
        UE_LOG(LogTemp, Display, TEXT("MWCS: %s -> ForceRecreate requested for %s"), ModeToString(Mode), *Context);
    }

    FAssetData Existing;
    const bool bExists = FindAssetData(PackagePath, AssetName, Existing);
    if (bExists && Mode == EMWCS_BuildMode::CreateMissing)
    {
        UE_LOG(LogTemp, Display, TEXT("MWCS: %s -> SKIP (asset already exists): %s"), ModeToString(Mode), *Context);
        // CreateMissing should not modify existing assets.
        return true;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("MWCS: %s -> %s asset (already exists: %s): %s"),
        ModeToString(Mode),
        bExists ? TEXT("UPDATE") : TEXT("CREATE"),
        bExists ? TEXT("true") : TEXT("false"),
        *Context);

    UWidgetBlueprint *Blueprint = nullptr;
    if (bExists)
    {
        Blueprint = Cast<UWidgetBlueprint>(Existing.GetAsset());
        if (!Blueprint)
        {
            AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.AssetWrongType"), TEXT("Existing asset is not a widget blueprint."), Context);
            return false;
        }
    }
    else
    {
        FAssetToolsModule &AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
        UObject *NewAsset = AssetTools.Get().CreateAsset(AssetName, PackagePath, AssetClass, Factory);
        Blueprint = Cast<UWidgetBlueprint>(NewAsset);
        if (!Blueprint)
        {
            AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.CreateFailed"), TEXT("Failed to create widget blueprint asset."), Context);
            return false;
        }

        // The factory can compile during creation. To avoid first-run BindWidget warnings when the widget tree
        // hasn't been built yet, we may intentionally create with a safe parent (UUserWidget) and then
        // reparent here before our own build/compile pass.
        if (ParentClass && Blueprint->ParentClass != ParentClass)
        {
            Blueprint->ParentClass = ParentClass;
        }

        Report.AssetsCreated++;
    }

    if (!Blueprint->WidgetTree)
    {
        Blueprint->WidgetTree = NewObject<UWidgetTree>(Blueprint, UWidgetTree::StaticClass(), TEXT("WidgetTree"), RF_Transactional | RF_DefaultSubObject);
    }

    // Repair/ForceRecreate must be deterministic: rebuild the widget tree and remove any stale member variables.
    // NOTE: In ForceRecreate, asset deletion + asset registry latency can still make the asset appear to "exist"
    // briefly. Rebuilding unconditionally in ForceRecreate ensures we never compile against a stale/empty tree.
    // Freshly-created WidgetBlueprints can also have a non-standard WidgetTree subobject naming/flagging on the
    // first run; rebuilding here makes first-run compilation behave like subsequent runs.
    if (Mode == EMWCS_BuildMode::Repair || Mode == EMWCS_BuildMode::ForceRecreate || !bExists)
    {
        // IMPORTANT:
        // Keep the WidgetTree subobject named exactly "WidgetTree" and flagged as a default subobject.
        // Some UMG compile/validation paths assume this name/flagging; using a unique name can cause
        // the compiler to validate against an old/empty tree, producing spurious BindWidget warnings.
        if (Blueprint->WidgetTree)
        {
            const FName OldName = MakeUniqueObjectName(Blueprint, UWidgetTree::StaticClass(), TEXT("MWCS_OldWidgetTree"));
            Blueprint->WidgetTree->Rename(*OldName.ToString(), Blueprint, REN_DontCreateRedirectors | REN_NonTransactional);
        }

        Blueprint->WidgetTree = NewObject<UWidgetTree>(Blueprint, UWidgetTree::StaticClass(), TEXT("WidgetTree"), RF_Transactional | RF_DefaultSubObject);

        if ((Mode == EMWCS_BuildMode::Repair || Mode == EMWCS_BuildMode::ForceRecreate) && !Spec.bIsToolEUW)
        {
            // Remove any member variables that collide with C++ BindWidget properties.
            // (These can be left behind from earlier generations and cause compiler internal errors.)
            for (const FName &N : Spec.Bindings.Required)
            {
                FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, N);
            }
            for (const FName &N : Spec.Bindings.Optional)
            {
                FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, N);
            }
        }
    }

    TSet<FName> RequiredVarNames;
    for (const FName &N : Spec.Bindings.Required)
    {
        RequiredVarNames.Add(N);
    }
    for (const FName &N : Spec.Bindings.Optional)
    {
        RequiredVarNames.Add(N);
    }

    TSet<FName> ForceVariableNames;
    const bool bStrictNaming = Spec.bIsToolEUW;
    if (Spec.bIsToolEUW)
    {
        GetToolEuwContractNames(ForceVariableNames);
    }

    TSet<FName> SeenSpecNames;

    if (!BuildNode(Blueprint->WidgetTree, Spec.HierarchyRoot, /*Parent*/ nullptr, ParentClass, RequiredVarNames, ForceVariableNames, Spec.Bindings.Types, bStrictNaming, SeenSpecNames, Report, Context))
    {
        return false;
    }

    ValidateBuiltBindings(Blueprint, Spec, Report, Context);

    if (!CompileAndSave(Blueprint, Spec, Report, Context))
    {
        return false;
    }

    if (!bExists)
    {
        return true;
    }

    if (Mode == EMWCS_BuildMode::Repair)
    {
        Report.AssetsRepaired++;
    }
    else if (Mode == EMWCS_BuildMode::ForceRecreate)
    {
        Report.AssetsRecreated++;
    }

    return true;
}

bool FMWCS_WidgetBuilder::CreateOrUpdateFromSpec(const FMWCS_WidgetSpec &Spec, EMWCS_BuildMode Mode, FMWCS_Report &InOutReport)
{
    const UMWCS_Settings *Settings = UMWCS_Settings::Get();
    if (!Settings)
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Settings.Missing"), TEXT("MWCS settings not available."), TEXT("Builder"));
        return false;
    }

    FString PackagePath;
    if (!EnsureValidPackagePath(Settings->OutputRootPath, PackagePath))
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Builder.InvalidOutputPath"), TEXT("OutputRootPath is not a valid long package path."), Settings->OutputRootPath);
        return false;
    }

    const FString AssetName = Spec.BlueprintName.ToString();
    const FString Context = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);

    UClass *ParentClass = ResolveParentClass(Spec.ParentClassPath);
    if (!ParentClass)
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Builder.ParentLoadFailed"), TEXT("Failed to load ParentClass."), Spec.ParentClassPath);
        return false;
    }

    // IMPORTANT: Keep the factory alive across ForceRecreate deletion/GC.
    // DeleteAssets can trigger GC, and a raw UObject* factory with no references may be collected,
    // leading to crashes inside AssetTools::CreateAsset.
    TStrongObjectPtr<UWidgetBlueprintFactory> Factory(NewObject<UWidgetBlueprintFactory>());
    // CreateBlueprint() may compile during asset creation. If we create directly with a parent that has
    // required BindWidget properties, the first compile can emit warnings before MWCS has built the tree.
    // Create with a safe base parent and reparent before our explicit compile.
    Factory->ParentClass = UUserWidget::StaticClass();

    return CreateOrUpdateInternal(PackagePath, AssetName, UWidgetBlueprint::StaticClass(), Factory.Get(), ParentClass, Spec, Mode, InOutReport, Context);
}

bool FMWCS_WidgetBuilder::CreateOrUpdateToolEuwFromSpec(const FMWCS_WidgetSpec &Spec, EMWCS_BuildMode Mode, FMWCS_Report &InOutReport)
{
    const UMWCS_Settings *Settings = UMWCS_Settings::Get();
    if (!Settings)
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Settings.Missing"), TEXT("MWCS settings not available."), TEXT("Builder.ToolEUW"));
        return false;
    }

    FString PackagePath;
    if (!EnsureValidPackagePath(Settings->ToolEuwOutputPath, PackagePath))
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Builder.InvalidToolEuwPath"), TEXT("ToolEuwOutputPath is not a valid long package path."), Settings->ToolEuwOutputPath);
        return false;
    }

    const FString AssetName = Settings->ToolEuwAssetName.IsEmpty() ? TEXT("EUW_MWCS_Tool") : Settings->ToolEuwAssetName;
    const FString Context = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);

    // IMPORTANT: Keep the factory alive across ForceRecreate deletion/GC.
    TStrongObjectPtr<UEditorUtilityWidgetBlueprintFactory> Factory(NewObject<UEditorUtilityWidgetBlueprintFactory>());

    UClass *Parent = nullptr;
    if (!Spec.ParentClassPath.IsEmpty())
    {
        Parent = ResolveEuwParentClass(Spec.ParentClassPath);
    }
    if (!Parent)
    {
        if (Spec.bIsToolEUW)
        {
            AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Builder.ToolEUW.ParentLoadFailed"), TEXT("Failed to load Tool EUW ParentClass (strict)."), Spec.ParentClassPath);
            return false;
        }
        Parent = UEditorUtilityWidget::StaticClass();
    }
    Factory->ParentClass = Parent;

    return CreateOrUpdateInternal(PackagePath, AssetName, UEditorUtilityWidgetBlueprint::StaticClass(), Factory.Get(), Parent, Spec, Mode, InOutReport, Context);
}
