#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "MWCS_ToolWidgetSpecProvider.generated.h"

UCLASS()
class UMWCS_ToolWidgetSpecProvider : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, BlueprintPure)
    static FString GetWidgetSpec();
};
