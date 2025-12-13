#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "MWCS_Commandlets.generated.h"

UCLASS()
class UMWCS_ValidateWidgetsCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString &Params) override;
};

UCLASS()
class UMWCS_CreateWidgetsCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString &Params) override;
};
