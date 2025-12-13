#pragma once

#include "CoreMinimal.h"

#include "Layout/Margin.h"
#include "Components/SlateWrapperTypes.h"

struct FMWCS_Bindings
{
    TArray<FName> Required;
    TArray<FName> Optional;
    // Optional: best-effort type info for bindings, e.g. "UTextBlock", "UMF_MatchInfo".
    TMap<FName, FString> Types;
};

struct FMWCS_HierarchyNode
{
    FName Name;
    FName Type;
    bool bIsVariable = true;
    FString Text;

    // Optional: for Type == "UserWidget". Can be a native class path (/Script/Module.Class)
    // or a blueprint generated class path (/Game/..._C).
    FString WidgetClassPath;

    // Optional text styling (primarily for TextBlock)
    int32 FontSize = 0;    // <=0 means unset
    FString Justification; // "Left" | "Center" | "Right"

    // Optional slot/layout metadata (applied to the slot created by the parent container)
    bool bHasSlotPadding = false;
    FMargin SlotPadding;

    bool bHasSlotHAlign = false;
    EHorizontalAlignment SlotHAlign = HAlign_Fill;

    bool bHasSlotVAlign = false;
    EVerticalAlignment SlotVAlign = VAlign_Fill;

    bool bHasSlotSize = false;
    ESlateSizeRule::Type SlotSizeRule = ESlateSizeRule::Automatic;
    float SlotSizeValue = 1.0f;

    // Optional CanvasPanel slot metadata
    bool bHasCanvasAnchors = false;
    FVector2D CanvasAnchorsMin = FVector2D::ZeroVector;
    FVector2D CanvasAnchorsMax = FVector2D::UnitVector;

    bool bHasCanvasOffsets = false;
    FMargin CanvasOffsets;

    bool bHasCanvasAlignment = false;
    FVector2D CanvasAlignment = FVector2D(0.0f, 0.0f);

    bool bHasCanvasAutoSize = false;
    bool bCanvasAutoSize = false;

    bool bHasCanvasZOrder = false;
    int32 CanvasZOrder = 0;

    TArray<FMWCS_HierarchyNode> Children;
};

enum class EMWCS_PreviewSizeMode : uint8
{
    FillScreen,
    Desired,
    DesiredOnScreen,
    Custom,
};

struct FMWCS_DesignerPreview
{
    // Persistent tier
    EMWCS_PreviewSizeMode SizeMode = EMWCS_PreviewSizeMode::FillScreen;
    FVector2D CustomSize = FVector2D::ZeroVector;

    // Best-effort tier
    int32 ZoomLevel = 14;
    bool bShowGrid = true;
};

struct FMWCS_WidgetSpec
{
    FName BlueprintName;
    FString ParentClassPath;
    FString Version;
    bool bIsToolEUW = false;
    FMWCS_DesignerPreview DesignerPreview;
    FMWCS_HierarchyNode HierarchyRoot;
    FMWCS_Bindings Bindings;
};
