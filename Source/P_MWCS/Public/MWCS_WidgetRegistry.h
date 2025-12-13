#pragma once

#include "CoreMinimal.h"

#include "MWCS_Report.h"
#include "MWCS_WidgetSpec.h"

class FMWCS_WidgetRegistry
{
public:
    static bool CollectSpecs(TArray<FMWCS_WidgetSpec> &OutSpecs, FMWCS_Report &InOutReport);
};
