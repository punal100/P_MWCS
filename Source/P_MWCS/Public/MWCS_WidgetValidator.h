#pragma once

#include "CoreMinimal.h"

#include "MWCS_Report.h"
#include "MWCS_WidgetSpec.h"

class FMWCS_WidgetValidator
{
public:
    static bool ValidateSpecAsset(const FMWCS_WidgetSpec &Spec, FMWCS_Report &InOutReport);
};
