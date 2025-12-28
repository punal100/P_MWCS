// Copyright Punal Manalan. All Rights Reserved.

#include "MWCS_TransparentButton.h"

UMWCS_TransparentButton::UMWCS_TransparentButton()
	: HoverTint(1.f, 1.f, 1.f, 0.1f)
	, PressedTint(1.f, 1.f, 1.f, 0.2f)
	, bShowHoverFeedback(true)
{
	// Apply transparent style immediately on construction
	SetTransparentStyle();
}

void UMWCS_TransparentButton::SetTransparentStyle()
{
	FButtonStyle Style = GetStyle();

	// Set all states to NoDrawType for fully transparent background
	Style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
	Style.Normal.TintColor = FSlateColor(FLinearColor::Transparent);

	Style.Disabled.DrawAs = ESlateBrushDrawType::NoDrawType;
	Style.Disabled.TintColor = FSlateColor(FLinearColor::Transparent);

	if (bShowHoverFeedback)
	{
		// Use RoundedBox for subtle hover/press feedback
		Style.Hovered.DrawAs = ESlateBrushDrawType::RoundedBox;
		Style.Hovered.TintColor = FSlateColor(HoverTint);

		Style.Pressed.DrawAs = ESlateBrushDrawType::RoundedBox;
		Style.Pressed.TintColor = FSlateColor(PressedTint);
	}
	else
	{
		Style.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.Hovered.TintColor = FSlateColor(FLinearColor::Transparent);

		Style.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.Pressed.TintColor = FSlateColor(FLinearColor::Transparent);
	}

	SetStyle(Style);
}

void UMWCS_TransparentButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// Re-apply transparent style when properties change in editor
	SetTransparentStyle();
}

#if WITH_EDITOR
const FText UMWCS_TransparentButton::GetPaletteCategory()
{
	return NSLOCTEXT("MWCS", "TransparentButtonCategory", "MWCS");
}
#endif
