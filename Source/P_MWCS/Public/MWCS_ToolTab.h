#pragma once

#include "CoreMinimal.h"

class FMWCS_ToolTab
{
public:
    static const FName TabName;

    static void Register();
    static void Unregister();
    static void Open();
};
