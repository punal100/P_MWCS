// MWCS_Utilities.h
// Shared utility functions for MWCS to avoid Unity Build conflicts
// These functions were previously defined in anonymous namespaces in multiple .cpp files

#pragma once

#include "CoreMinimal.h"
#include "MWCS_Types.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"

namespace MWCS_Utilities
{
    // Add an issue to the report
    inline void AddIssue(FMWCS_Report& Report, EMWCS_IssueSeverity Severity, const FString& Code, const FString& Message, const FString& Context)
    {
        FMWCS_Issue Issue;
        Issue.Severity = Severity;
        Issue.Code = Code;
        Issue.Message = Message;
        Issue.Context = Context;
        Report.Issues.Add(MoveTemp(Issue));
    }

    // Ensure a package path is valid and normalized
    inline bool EnsureValidPackagePath(const FString& Path, FString& OutNormalized)
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

    // Find asset data by package path and asset name
    inline bool FindAssetData(const FString& PackagePath, const FString& AssetName, FAssetData& OutAssetData)
    {
        const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
        FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        OutAssetData = AssetRegistry.Get().GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
        return OutAssetData.IsValid();
    }
}
