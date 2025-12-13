#pragma once

#include "CoreMinimal.h"

struct FMWCS_Bindings
{
    TArray<FName> Required;
    TArray<FName> Optional;
};

struct FMWCS_HierarchyNode
{
    FName Name;
    FName Type;
    bool bIsVariable = true;
    TArray<FMWCS_HierarchyNode> Children;
};

struct FMWCS_WidgetSpec
{
    FName BlueprintName;
    FString ParentClassPath;
    int32 Version = 1;
    FMWCS_HierarchyNode HierarchyRoot;
    FMWCS_Bindings Bindings;
};
