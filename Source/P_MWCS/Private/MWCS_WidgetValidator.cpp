#include "MWCS_WidgetValidator.h"

#include "MWCS_Settings.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Misc/PackageName.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"

static void AddIssue(FMWCS_Report &Report, EMWCS_IssueSeverity Severity, const FString &Code, const FString &Message, const FString &Context)
{
    FMWCS_Issue Issue;
    Issue.Severity = Severity;
    Issue.Code = Code;
    Issue.Message = Message;
    Issue.Context = Context;
    Report.Issues.Add(MoveTemp(Issue));
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

    if (!BP->GeneratedClass)
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Warning, TEXT("Validator.NoGeneratedClass"), TEXT("Blueprint has no GeneratedClass (compile may be required)."), Context);
    }

    return !InOutReport.HasErrors();
}
