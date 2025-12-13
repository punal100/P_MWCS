#pragma once

#include "CoreMinimal.h"

UENUM()
enum class EMWCS_BuildMode : uint8
{
    CreateMissing,
    Repair,
    ForceRecreate
};

UENUM()
enum class EMWCS_IssueSeverity : uint8
{
    Info,
    Warning,
    Error
};
