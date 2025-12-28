// Copyright Punal Manalan. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "MWCS_TransparentButton.generated.h"

/**
 * A custom button widget that defaults to transparent styling.
 * Used by A_WCG to generate HTML-styled transparent buttons for UMG.
 * Works in shipping builds (not Editor-only).
 */
UCLASS()
class P_MWCS_RUNTIME_API UMWCS_TransparentButton : public UButton
{
	GENERATED_BODY()

public:
	UMWCS_TransparentButton();

	/** Apply transparent style to the button (no visible background) */
	UFUNCTION(BlueprintCallable, Category = "MWCS|Button")
	void SetTransparentStyle();

	/** Tint color applied on hover state (subtle visual feedback) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MWCS|Button")
	FLinearColor HoverTint;

	/** Tint color applied on pressed state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MWCS|Button")
	FLinearColor PressedTint;

	/** If true, shows subtle visual feedback on hover/press */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MWCS|Button")
	bool bShowHoverFeedback;

protected:
	virtual void SynchronizeProperties() override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif
};
