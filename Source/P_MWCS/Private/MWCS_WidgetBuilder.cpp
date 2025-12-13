#include "MWCS_WidgetBuilder.h"

#include "MWCS_Settings.h"

#include "UObject/SavePackage.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "EditorUtilityWidget.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "EditorUtilityWidgetBlueprintFactory.h"
#include "Editor/UMGEditor/Classes/WidgetBlueprintFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Blueprint/WidgetTree.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/Overlay.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

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
        AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.DeleteFailed"), TEXT("Failed to delete existing asset for ForceRecreate."), Context);
        return false;
    }
    return true;
}

static UWidget *ConstructWidget(UWidgetTree *Tree, const FName Type, const FName Name, FMWCS_Report &Report, const FString &Context)
{
    UClass *WidgetClass = nullptr;
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

    if (!WidgetClass)
    {
        AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.UnsupportedWidget"), FString::Printf(TEXT("Unsupported widget type: %s"), *Type.ToString()), Context);
        return nullptr;
    }

    const FName WidgetName = (Name == NAME_None) ? MakeUniqueObjectName(Tree, WidgetClass) : Name;
    return Tree->ConstructWidget<UWidget>(WidgetClass, WidgetName);
}

static void EnsureWidgetVariable(UWidget *Widget, bool bIsVariable)
{
    if (Widget)
    {
        Widget->bIsVariable = bIsVariable;
    }
}

static bool BuildNode(UWidgetTree *Tree, const FMWCS_HierarchyNode &Node, UWidget *Parent, const TSet<FName> &RequiredVars, FMWCS_Report &Report, const FString &Context)
{
    if (!Tree)
    {
        return false;
    }

    UWidget *Current = nullptr;
    if (Parent == nullptr)
    {
        Current = Tree->RootWidget;
        if (!Current)
        {
            Current = ConstructWidget(Tree, Node.Type, Node.Name, Report, Context);
            if (!Current)
            {
                return false;
            }
            Tree->RootWidget = Current;
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
            Current = ConstructWidget(Tree, Node.Type, Node.Name, Report, Context);
            if (!Current)
            {
                return false;
            }

            UPanelWidget *Panel = Cast<UPanelWidget>(Parent);
            if (!Panel)
            {
                AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.NonPanelParent"), FString::Printf(TEXT("Parent %s is not a panel widget."), *Parent->GetName()), Context);
                return false;
            }
            Panel->AddChild(Current);
        }
    }

    const bool bRequiredVar = (Node.Name != NAME_None) && RequiredVars.Contains(Node.Name);
    EnsureWidgetVariable(Current, bRequiredVar ? true : Node.bIsVariable);

    for (const FMWCS_HierarchyNode &Child : Node.Children)
    {
        if (!BuildNode(Tree, Child, Current, RequiredVars, Report, Context))
        {
            return false;
        }
    }

    return true;
}

static bool CompileAndSave(UWidgetBlueprint *Blueprint, FMWCS_Report &Report, const FString &Context)
{
    if (!Blueprint)
    {
        return false;
    }

    FKismetEditorUtilities::CompileBlueprint(Blueprint);

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

static bool CreateOrUpdateInternal(const FString &PackagePath, const FString &AssetName, UClass *AssetClass, UFactory *Factory, const FMWCS_WidgetSpec &Spec, EMWCS_BuildMode Mode, FMWCS_Report &Report, const FString &Context)
{
    if (!Factory || !AssetClass)
    {
        AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("Builder.FactoryMissing"), TEXT("Asset factory or class missing."), Context);
        return false;
    }

    if (Mode == EMWCS_BuildMode::ForceRecreate)
    {
        if (!DeleteAssetIfExists(PackagePath, AssetName, Report, Context))
        {
            return false;
        }
    }

    FAssetData Existing;
    const bool bExists = FindAssetData(PackagePath, AssetName, Existing);
    if (bExists && Mode == EMWCS_BuildMode::CreateMissing)
    {
        // CreateMissing should not modify existing assets.
        return true;
    }

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
        Report.AssetsCreated++;
    }

    if (!Blueprint->WidgetTree)
    {
        Blueprint->WidgetTree = NewObject<UWidgetTree>(Blueprint, TEXT("WidgetTree"), RF_Transactional);
    }

    TSet<FName> RequiredVarNames;
    for (const FName &N : Spec.Bindings.Required)
    {
        RequiredVarNames.Add(N);
    }

    if (!BuildNode(Blueprint->WidgetTree, Spec.HierarchyRoot, /*Parent*/ nullptr, RequiredVarNames, Report, Context))
    {
        return false;
    }

    if (!CompileAndSave(Blueprint, Report, Context))
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

    UWidgetBlueprintFactory *Factory = NewObject<UWidgetBlueprintFactory>();
    Factory->ParentClass = ParentClass;

    return CreateOrUpdateInternal(PackagePath, AssetName, UWidgetBlueprint::StaticClass(), Factory, Spec, Mode, InOutReport, Context);
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

    UEditorUtilityWidgetBlueprintFactory *Factory = NewObject<UEditorUtilityWidgetBlueprintFactory>();
    Factory->ParentClass = UEditorUtilityWidget::StaticClass();

    return CreateOrUpdateInternal(PackagePath, AssetName, UEditorUtilityWidgetBlueprint::StaticClass(), Factory, Spec, Mode, InOutReport, Context);
}
