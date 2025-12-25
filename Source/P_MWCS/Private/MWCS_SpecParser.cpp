#include "MWCS_SpecParser.h"

#include "MWCS_Settings.h"
#include "MWCS_Utilities.h"
using namespace MWCS_Utilities;

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static bool ReadStringField(const TSharedPtr<FJsonObject> &Obj, const TCHAR *Field, FString &Out)
{
    if (!Obj.IsValid())
    {
        return false;
    }
    return Obj->TryGetStringField(Field, Out);
}

static bool ReadIntField(const TSharedPtr<FJsonObject> &Obj, const TCHAR *Field, int32 &Out)
{
    if (!Obj.IsValid())
    {
        return false;
    }
    double Number = 0.0;
    if (!Obj->TryGetNumberField(Field, Number))
    {
        return false;
    }
    Out = static_cast<int32>(Number);
    return true;
}

static bool ReadVersionField(const TSharedPtr<FJsonObject> &Obj, const TCHAR *Field, FString &Out)
{
    if (!Obj.IsValid())
    {
        return false;
    }

    FString VersionString;
    if (Obj->TryGetStringField(Field, VersionString))
    {
        Out = VersionString;
        return true;
    }

    double VersionNumber = 0.0;
    if (Obj->TryGetNumberField(Field, VersionNumber))
    {
        // Preserve integer-ish values without trailing .0 where possible.
        const int64 AsInt = static_cast<int64>(VersionNumber);
        if (FMath::IsNearlyEqual(VersionNumber, static_cast<double>(AsInt)))
        {
            Out = FString::Printf(TEXT("%lld"), AsInt);
        }
        else
        {
            Out = FString::SanitizeFloat(VersionNumber);
        }
        return true;
    }

    return false;
}

static bool TryParseSizeObject(const TSharedPtr<FJsonObject> &Obj, const TCHAR *Field, FVector2D &Out)
{
    if (!Obj.IsValid())
    {
        return false;
    }

    const TSharedPtr<FJsonObject> *SizeObjPtr = nullptr;
    if (!Obj->TryGetObjectField(Field, SizeObjPtr) || !SizeObjPtr || !SizeObjPtr->IsValid())
    {
        return false;
    }

    const TSharedPtr<FJsonObject> SizeObj = *SizeObjPtr;

    double W = 0.0;
    double H = 0.0;
    const bool bHasW = SizeObj->TryGetNumberField(TEXT("Width"), W) || SizeObj->TryGetNumberField(TEXT("X"), W);
    const bool bHasH = SizeObj->TryGetNumberField(TEXT("Height"), H) || SizeObj->TryGetNumberField(TEXT("Y"), H);
    if (!bHasW || !bHasH)
    {
        return false;
    }

    Out = FVector2D(static_cast<float>(W), static_cast<float>(H));
    return true;
}

static bool TryParseIntFieldFlexible(const TSharedPtr<FJsonObject> &Obj, const TCHAR *Field, int32 &Out)
{
    if (!Obj.IsValid())
    {
        return false;
    }

    // Number
    double Number = 0.0;
    if (Obj->TryGetNumberField(Field, Number))
    {
        Out = static_cast<int32>(Number);
        return true;
    }

    // String
    FString Str;
    if (Obj->TryGetStringField(Field, Str))
    {
        int32 Parsed = 0;
        if (LexTryParseString(Parsed, *Str))
        {
            Out = Parsed;
            return true;
        }
    }

    return false;
}

static bool ParseDesignerPreview(const TSharedPtr<FJsonObject> &RootObj, FMWCS_DesignerPreview &Out, FMWCS_Report &Report, const FString &Context)
{
    // Defaults
    Out.SizeMode = EMWCS_PreviewSizeMode::FillScreen;
    Out.CustomSize = FVector2D::ZeroVector;
    Out.ZoomLevel = 14;
    Out.bShowGrid = true;

    if (!RootObj.IsValid())
    {
        return true;
    }

    const UMWCS_Settings *Settings = UMWCS_Settings::Get();
    const int32 ZoomMin = Settings ? Settings->DesignerZoomLevelMin : 0;
    const int32 ZoomMax = Settings ? Settings->DesignerZoomLevelMax : 20;

    auto ClampZoom = [&](int32 InZoom)
    {
        const int32 Clamped = FMath::Clamp(InZoom, ZoomMin, ZoomMax);
        if (Clamped != InZoom)
        {
            AddIssue(Report, EMWCS_IssueSeverity::Warning, TEXT("DesignerPreview.ZoomOutOfRange"),
                     FString::Printf(TEXT("ZoomLevel %d clamped to [%d, %d]."), InZoom, ZoomMin, ZoomMax),
                     Context);
        }
        Out.ZoomLevel = Clamped;
    };

    // Root DesignerPreview section.
    const TSharedPtr<FJsonObject> *PreviewObjPtr = nullptr;
    if (RootObj->TryGetObjectField(TEXT("DesignerPreview"), PreviewObjPtr) && PreviewObjPtr && PreviewObjPtr->IsValid())
    {
        const TSharedPtr<FJsonObject> PreviewObj = *PreviewObjPtr;

        FString ModeStr;
        if (PreviewObj->TryGetStringField(TEXT("SizeMode"), ModeStr))
        {
            if (ModeStr.Equals(TEXT("FillScreen"), ESearchCase::IgnoreCase) || ModeStr.Equals(TEXT("Fill Screen"), ESearchCase::IgnoreCase))
                Out.SizeMode = EMWCS_PreviewSizeMode::FillScreen;
            else if (ModeStr.Equals(TEXT("Desired"), ESearchCase::IgnoreCase))
                Out.SizeMode = EMWCS_PreviewSizeMode::Desired;
            else if (ModeStr.Equals(TEXT("DesiredOnScreen"), ESearchCase::IgnoreCase) || ModeStr.Equals(TEXT("Desired On Screen"), ESearchCase::IgnoreCase))
                Out.SizeMode = EMWCS_PreviewSizeMode::DesiredOnScreen;
            else if (ModeStr.Equals(TEXT("Custom"), ESearchCase::IgnoreCase))
                Out.SizeMode = EMWCS_PreviewSizeMode::Custom;
            else
                Out.SizeMode = EMWCS_PreviewSizeMode::FillScreen;
        }

        FVector2D CustomSize;
        if (TryParseSizeObject(PreviewObj, TEXT("CustomSize"), CustomSize))
        {
            Out.CustomSize = CustomSize;
        }

        int32 Zoom = 0;
        if (TryParseIntFieldFlexible(PreviewObj, TEXT("ZoomLevel"), Zoom))
        {
            ClampZoom(Zoom);
        }

        PreviewObj->TryGetBoolField(TEXT("ShowGrid"), Out.bShowGrid);

        // Hard validation for Custom mode
        if (Out.SizeMode == EMWCS_PreviewSizeMode::Custom)
        {
            if (Out.CustomSize.X <= 0.0f || Out.CustomSize.Y <= 0.0f)
            {
                AddIssue(Report, EMWCS_IssueSeverity::Error, TEXT("DesignerPreview.InvalidCustomSize"),
                         TEXT("Custom preview size must be positive when SizeMode is Custom."),
                         Context);
                return false;
            }
        }

        return true;
    }

    return true;
}

static bool ParseBindings(const TSharedPtr<FJsonObject> &RootObj, FMWCS_Bindings &OutBindings)
{
    const TSharedPtr<FJsonObject> *BindingsObj = nullptr;
    if (!RootObj->TryGetObjectField(TEXT("Bindings"), BindingsObj) || !BindingsObj || !BindingsObj->IsValid())
    {
        return false;
    }

    auto ParseStringArray = [](const TSharedPtr<FJsonObject> &Obj, const TCHAR *Field, TArray<FName> &OutArr)
    {
        const TArray<TSharedPtr<FJsonValue>> *Values = nullptr;
        if (!Obj->TryGetArrayField(Field, Values) || !Values)
        {
            return;
        }
        for (const TSharedPtr<FJsonValue> &V : *Values)
        {
            FString S;
            if (V.IsValid() && V->TryGetString(S))
            {
                OutArr.Add(FName(*S));
            }
            else if (V.IsValid() && V->Type == EJson::Object)
            {
                TSharedPtr<FJsonObject> ObjVal = V->AsObject();
                FString Name;
                if (ObjVal.IsValid() && ObjVal->TryGetStringField(TEXT("Name"), Name))
                {
                    OutArr.Add(FName(*Name));
                }
            }
        }
    };

    auto ParseStringArrayWithTypes = [&OutBindings](const TSharedPtr<FJsonObject> &Obj, const TCHAR *Field, TArray<FName> &OutArr)
    {
        const TArray<TSharedPtr<FJsonValue>> *Values = nullptr;
        if (!Obj->TryGetArrayField(Field, Values) || !Values)
        {
            return;
        }
        for (const TSharedPtr<FJsonValue> &V : *Values)
        {
            FString S;
            if (V.IsValid() && V->TryGetString(S))
            {
                OutArr.Add(FName(*S));
                continue;
            }
            if (V.IsValid() && V->Type == EJson::Object)
            {
                TSharedPtr<FJsonObject> ObjVal = V->AsObject();
                FString Name;
                if (ObjVal.IsValid() && ObjVal->TryGetStringField(TEXT("Name"), Name))
                {
                    const FName NameF(*Name);
                    OutArr.Add(NameF);

                    FString Type;
                    if (ObjVal->TryGetStringField(TEXT("Type"), Type) && !Type.IsEmpty())
                    {
                        OutBindings.Types.Add(NameF, Type);
                    }
                }
            }
        }
    };

    // Backwards-compatible parsing (strings or objects). Also capture per-binding type hints when present.
    ParseStringArrayWithTypes(*BindingsObj, TEXT("Required"), OutBindings.Required);
    ParseStringArrayWithTypes(*BindingsObj, TEXT("Optional"), OutBindings.Optional);
    return true;
}

static bool ParseHierarchyNode(const TSharedPtr<FJsonObject> &NodeObj, FMWCS_HierarchyNode &OutNode)
{
    FString Type;
    if (!NodeObj.IsValid() || !NodeObj->TryGetStringField(TEXT("Type"), Type))
    {
        return false;
    }
    OutNode.Type = FName(*Type);

    FString Name;
    if (NodeObj->TryGetStringField(TEXT("Name"), Name))
    {
        OutNode.Name = FName(*Name);
    }
    else
    {
        OutNode.Name = NAME_None;
    }

    // IMPORTANT:
    // If IsVariable is omitted in JSON, preserve the struct default (true).
    // Many specs (e.g. P_MiniFootball) rely on the default so BindWidget validation works.
    bool bIsVariable = false;
    if (NodeObj->TryGetBoolField(TEXT("IsVariable"), bIsVariable))
    {
        OutNode.bIsVariable = bIsVariable;
    }

    FString Text;
    if (NodeObj->TryGetStringField(TEXT("Text"), Text))
    {
        OutNode.Text = Text;
    }

    FString WidgetClassPath;
    if (NodeObj->TryGetStringField(TEXT("WidgetClass"), WidgetClassPath))
    {
        OutNode.WidgetClassPath = WidgetClassPath;
    }

    double FontSize = 0.0;
    if (NodeObj->TryGetNumberField(TEXT("FontSize"), FontSize))
    {
        OutNode.FontSize = FMath::Max(0, static_cast<int32>(FontSize));
    }

    FString Justification;
    if (NodeObj->TryGetStringField(TEXT("Justification"), Justification))
    {
        OutNode.Justification = Justification;
    }

    // Optional slot metadata
    const TSharedPtr<FJsonObject> *SlotObjPtr = nullptr;
    if (NodeObj->TryGetObjectField(TEXT("Slot"), SlotObjPtr) && SlotObjPtr && SlotObjPtr->IsValid())
    {
        const TSharedPtr<FJsonObject> SlotObj = *SlotObjPtr;

        auto TryReadVec2 = [](const TSharedPtr<FJsonObject> &Obj, const TCHAR *Field, FVector2D &Out) -> bool
        {
            if (!Obj.IsValid())
            {
                return false;
            }
            const TSharedPtr<FJsonObject> *VecObjPtr = nullptr;
            if (Obj->TryGetObjectField(Field, VecObjPtr) && VecObjPtr && VecObjPtr->IsValid())
            {
                double X = 0.0, Y = 0.0;
                if ((*VecObjPtr)->TryGetNumberField(TEXT("X"), X) && (*VecObjPtr)->TryGetNumberField(TEXT("Y"), Y))
                {
                    Out = FVector2D(static_cast<float>(X), static_cast<float>(Y));
                    return true;
                }
            }
            const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
            if (Obj->TryGetArrayField(Field, Arr) && Arr && Arr->Num() == 2)
            {
                Out = FVector2D(static_cast<float>((*Arr)[0]->AsNumber()), static_cast<float>((*Arr)[1]->AsNumber()));
                return true;
            }
            return false;
        };

        // Padding
        const TArray<TSharedPtr<FJsonValue>> *PaddingArr = nullptr;
        if (SlotObj->TryGetArrayField(TEXT("Padding"), PaddingArr) && PaddingArr && PaddingArr->Num() == 4)
        {
            auto NumAt = [&PaddingArr](int32 Index, double DefaultValue)
            {
                if (!PaddingArr->IsValidIndex(Index) || !(*PaddingArr)[Index].IsValid())
                {
                    return DefaultValue;
                }
                if ((*PaddingArr)[Index]->Type == EJson::Number)
                {
                    return (*PaddingArr)[Index]->AsNumber();
                }
                return DefaultValue;
            };
            OutNode.bHasSlotPadding = true;
            OutNode.SlotPadding = FMargin(
                static_cast<float>(NumAt(0, 0.0)),
                static_cast<float>(NumAt(1, 0.0)),
                static_cast<float>(NumAt(2, 0.0)),
                static_cast<float>(NumAt(3, 0.0)));
        }
        else
        {
            const TSharedPtr<FJsonObject> *PaddingObjPtr = nullptr;
            if (SlotObj->TryGetObjectField(TEXT("Padding"), PaddingObjPtr) && PaddingObjPtr && PaddingObjPtr->IsValid())
            {
                double L = 0, T = 0, R = 0, B = 0;
                (*PaddingObjPtr)->TryGetNumberField(TEXT("Left"), L);
                (*PaddingObjPtr)->TryGetNumberField(TEXT("Top"), T);
                (*PaddingObjPtr)->TryGetNumberField(TEXT("Right"), R);
                (*PaddingObjPtr)->TryGetNumberField(TEXT("Bottom"), B);
                OutNode.bHasSlotPadding = true;
                OutNode.SlotPadding = FMargin(static_cast<float>(L), static_cast<float>(T), static_cast<float>(R), static_cast<float>(B));
            }
        }

        auto ParseHAlign = [](const FString &S, bool &bOutHas, EHorizontalAlignment &Out)
        {
            if (S.IsEmpty())
            {
                return;
            }
            bOutHas = true;
            if (S.Equals(TEXT("Left"), ESearchCase::IgnoreCase))
                Out = HAlign_Left;
            else if (S.Equals(TEXT("Center"), ESearchCase::IgnoreCase))
                Out = HAlign_Center;
            else if (S.Equals(TEXT("Right"), ESearchCase::IgnoreCase))
                Out = HAlign_Right;
            else if (S.Equals(TEXT("Fill"), ESearchCase::IgnoreCase))
                Out = HAlign_Fill;
            else
                bOutHas = false;
        };

        auto ParseVAlign = [](const FString &S, bool &bOutHas, EVerticalAlignment &Out)
        {
            if (S.IsEmpty())
            {
                return;
            }
            bOutHas = true;
            if (S.Equals(TEXT("Top"), ESearchCase::IgnoreCase))
                Out = VAlign_Top;
            else if (S.Equals(TEXT("Center"), ESearchCase::IgnoreCase))
                Out = VAlign_Center;
            else if (S.Equals(TEXT("Bottom"), ESearchCase::IgnoreCase))
                Out = VAlign_Bottom;
            else if (S.Equals(TEXT("Fill"), ESearchCase::IgnoreCase))
                Out = VAlign_Fill;
            else
                bOutHas = false;
        };

        FString HAlignStr;
        if (SlotObj->TryGetStringField(TEXT("HAlign"), HAlignStr))
        {
            ParseHAlign(HAlignStr, OutNode.bHasSlotHAlign, OutNode.SlotHAlign);
        }

        FString VAlignStr;
        if (SlotObj->TryGetStringField(TEXT("VAlign"), VAlignStr))
        {
            ParseVAlign(VAlignStr, OutNode.bHasSlotVAlign, OutNode.SlotVAlign);
        }

        // Size (HorizontalBox/VerticalBox slots)
        // Supported forms:
        //  - "Fill": 1.0
        //  - "Size": 1.0            (treated as Fill)
        //  - "Size": { "Rule": "Fill|Auto", "Value": 1.0 }
        double FillValue = 0.0;
        if (SlotObj->TryGetNumberField(TEXT("Fill"), FillValue))
        {
            OutNode.bHasSlotSize = true;
            OutNode.SlotSizeRule = ESlateSizeRule::Fill;
            OutNode.SlotSizeValue = static_cast<float>(FillValue);
        }
        else
        {
            double SizeNumber = 0.0;
            if (SlotObj->TryGetNumberField(TEXT("Size"), SizeNumber))
            {
                OutNode.bHasSlotSize = true;
                OutNode.SlotSizeRule = ESlateSizeRule::Fill;
                OutNode.SlotSizeValue = static_cast<float>(SizeNumber);
            }
            else
            {
                const TSharedPtr<FJsonObject> *SizeObjPtr = nullptr;
                if (SlotObj->TryGetObjectField(TEXT("Size"), SizeObjPtr) && SizeObjPtr && SizeObjPtr->IsValid())
                {
                    FString RuleStr;
                    (*SizeObjPtr)->TryGetStringField(TEXT("Rule"), RuleStr);

                    double Value = 1.0;
                    (*SizeObjPtr)->TryGetNumberField(TEXT("Value"), Value);

                    OutNode.bHasSlotSize = true;
                    if (RuleStr.Equals(TEXT("Auto"), ESearchCase::IgnoreCase) || RuleStr.Equals(TEXT("Automatic"), ESearchCase::IgnoreCase))
                    {
                        OutNode.SlotSizeRule = ESlateSizeRule::Automatic;
                        OutNode.SlotSizeValue = 1.0f;
                    }
                    else
                    {
                        OutNode.SlotSizeRule = ESlateSizeRule::Fill;
                        OutNode.SlotSizeValue = static_cast<float>(Value);
                    }
                }
            }
        }
        // Canvas slot
        const TSharedPtr<FJsonObject> *CanvasObjPtr = nullptr;
        if (SlotObj->TryGetObjectField(TEXT("Canvas"), CanvasObjPtr) && CanvasObjPtr && CanvasObjPtr->IsValid())
        {
            const TSharedPtr<FJsonObject> CanvasObj = *CanvasObjPtr;

            const TSharedPtr<FJsonObject> *AnchorsObjPtr = nullptr;
            if (CanvasObj->TryGetObjectField(TEXT("Anchors"), AnchorsObjPtr) && AnchorsObjPtr && AnchorsObjPtr->IsValid())
            {
                const TArray<TSharedPtr<FJsonValue>> *MinArr = nullptr;
                const TArray<TSharedPtr<FJsonValue>> *MaxArr = nullptr;

                if ((*AnchorsObjPtr)->TryGetArrayField(TEXT("Min"), MinArr) && MinArr && MinArr->Num() == 2 &&
                    (*AnchorsObjPtr)->TryGetArrayField(TEXT("Max"), MaxArr) && MaxArr && MaxArr->Num() == 2)
                {
                    OutNode.bHasCanvasAnchors = true;
                    OutNode.CanvasAnchorsMin = FVector2D(static_cast<float>((*MinArr)[0]->AsNumber()), static_cast<float>((*MinArr)[1]->AsNumber()));
                    OutNode.CanvasAnchorsMax = FVector2D(static_cast<float>((*MaxArr)[0]->AsNumber()), static_cast<float>((*MaxArr)[1]->AsNumber()));
                }
            }

            const TArray<TSharedPtr<FJsonValue>> *OffsetsArr = nullptr;
            if (CanvasObj->TryGetArrayField(TEXT("Offsets"), OffsetsArr) && OffsetsArr && OffsetsArr->Num() == 4)
            {
                OutNode.bHasCanvasOffsets = true;
                OutNode.CanvasOffsets = FMargin(
                    static_cast<float>((*OffsetsArr)[0]->AsNumber()),
                    static_cast<float>((*OffsetsArr)[1]->AsNumber()),
                    static_cast<float>((*OffsetsArr)[2]->AsNumber()),
                    static_cast<float>((*OffsetsArr)[3]->AsNumber()));
            }

            const TArray<TSharedPtr<FJsonValue>> *AlignmentArr = nullptr;
            if (CanvasObj->TryGetArrayField(TEXT("Alignment"), AlignmentArr) && AlignmentArr && AlignmentArr->Num() == 2)
            {
                OutNode.bHasCanvasAlignment = true;
                OutNode.CanvasAlignment = FVector2D(static_cast<float>((*AlignmentArr)[0]->AsNumber()), static_cast<float>((*AlignmentArr)[1]->AsNumber()));
            }

            bool bAutoSize = false;
            if (CanvasObj->TryGetBoolField(TEXT("AutoSize"), bAutoSize))
            {
                OutNode.bHasCanvasAutoSize = true;
                OutNode.bCanvasAutoSize = bAutoSize;
            }

            double ZOrder = 0.0;
            if (CanvasObj->TryGetNumberField(TEXT("ZOrder"), ZOrder))
            {
                OutNode.bHasCanvasZOrder = true;
                OutNode.CanvasZOrder = static_cast<int32>(ZOrder);
            }
        }
        else
        {
            // Legacy/alternate CanvasPanel slot schema: allow canvas properties at Slot root.
            // Common in P_MiniFootball specs: Anchors/Position/Size/Alignment/Offsets/AutoSize/ZOrder.

            // Anchors: { Min:{X,Y}, Max:{X,Y} }
            const TSharedPtr<FJsonObject> *AnchorsObjPtr = nullptr;
            if (SlotObj->TryGetObjectField(TEXT("Anchors"), AnchorsObjPtr) && AnchorsObjPtr && AnchorsObjPtr->IsValid())
            {
                FVector2D Min, Max;
                if (TryReadVec2(*AnchorsObjPtr, TEXT("Min"), Min) && TryReadVec2(*AnchorsObjPtr, TEXT("Max"), Max))
                {
                    OutNode.bHasCanvasAnchors = true;
                    OutNode.CanvasAnchorsMin = Min;
                    OutNode.CanvasAnchorsMax = Max;
                }
            }

            // Offsets: {Left,Top,Right,Bottom} or [L,T,R,B]
            if (!OutNode.bHasCanvasOffsets)
            {
                const TSharedPtr<FJsonObject> *OffsetsObjPtr = nullptr;
                const TArray<TSharedPtr<FJsonValue>> *OffsetsArr = nullptr;
                if (SlotObj->TryGetArrayField(TEXT("Offsets"), OffsetsArr) && OffsetsArr && OffsetsArr->Num() == 4)
                {
                    OutNode.bHasCanvasOffsets = true;
                    OutNode.CanvasOffsets = FMargin(
                        static_cast<float>((*OffsetsArr)[0]->AsNumber()),
                        static_cast<float>((*OffsetsArr)[1]->AsNumber()),
                        static_cast<float>((*OffsetsArr)[2]->AsNumber()),
                        static_cast<float>((*OffsetsArr)[3]->AsNumber()));
                }
                else if (SlotObj->TryGetObjectField(TEXT("Offsets"), OffsetsObjPtr) && OffsetsObjPtr && OffsetsObjPtr->IsValid())
                {
                    double L = 0, T = 0, R = 0, B = 0;
                    (*OffsetsObjPtr)->TryGetNumberField(TEXT("Left"), L);
                    (*OffsetsObjPtr)->TryGetNumberField(TEXT("Top"), T);
                    (*OffsetsObjPtr)->TryGetNumberField(TEXT("Right"), R);
                    (*OffsetsObjPtr)->TryGetNumberField(TEXT("Bottom"), B);
                    OutNode.bHasCanvasOffsets = true;
                    OutNode.CanvasOffsets = FMargin(static_cast<float>(L), static_cast<float>(T), static_cast<float>(R), static_cast<float>(B));
                }
            }

            // Position + Size -> Offsets (when not already specified)
            if (!OutNode.bHasCanvasOffsets)
            {
                FVector2D Pos;
                FVector2D Size;
                const bool bHasPos = TryReadVec2(SlotObj, TEXT("Position"), Pos);
                const bool bHasSize = TryReadVec2(SlotObj, TEXT("Size"), Size);
                if (bHasPos)
                {
                    OutNode.bHasCanvasOffsets = true;
                    OutNode.CanvasOffsets = FMargin(Pos.X, Pos.Y, bHasSize ? Size.X : 0.0f, bHasSize ? Size.Y : 0.0f);
                }
            }

            // Alignment
            FVector2D Align;
            if (TryReadVec2(SlotObj, TEXT("Alignment"), Align))
            {
                OutNode.bHasCanvasAlignment = true;
                OutNode.CanvasAlignment = Align;
            }

            // AutoSize
            bool bAutoSize = false;
            if (SlotObj->TryGetBoolField(TEXT("AutoSize"), bAutoSize))
            {
                OutNode.bHasCanvasAutoSize = true;
                OutNode.bCanvasAutoSize = bAutoSize;
            }

            // ZOrder
            double ZOrder = 0.0;
            if (SlotObj->TryGetNumberField(TEXT("ZOrder"), ZOrder))
            {
                OutNode.bHasCanvasZOrder = true;
                OutNode.CanvasZOrder = static_cast<int32>(ZOrder);
            }
        }
    }

    // Parse Properties section for specialized widgets
    const TSharedPtr<FJsonObject> *PropsPtr = nullptr;
    const bool bHasProps = NodeObj->TryGetObjectField(TEXT("Properties"), PropsPtr) && PropsPtr && PropsPtr->IsValid();

    // ScrollBox: Orientation, ScrollBarVisibility
    if (OutNode.Type == TEXT("ScrollBox"))
    {
        FString OrientStr;
        // Check Properties first, then inline
        if ((bHasProps && (*PropsPtr)->TryGetStringField(TEXT("Orientation"), OrientStr)) || 
            NodeObj->TryGetStringField(TEXT("Orientation"), OrientStr))
        {
            OutNode.bHasOrientation = true;
            OutNode.Orientation = OrientStr.Equals(TEXT("Horizontal"), ESearchCase::IgnoreCase) ? EOrientation::Orient_Horizontal : EOrientation::Orient_Vertical;
        }

        FString VisStr;
        if ((bHasProps && (*PropsPtr)->TryGetStringField(TEXT("ScrollBarVisibility"), VisStr)) ||
            NodeObj->TryGetStringField(TEXT("ScrollBarVisibility"), VisStr))
        {
            OutNode.bHasScrollBarVisibility = true;
            if (VisStr.Equals(TEXT("Collapsed"), ESearchCase::IgnoreCase)) OutNode.ScrollBarVisibility = ESlateVisibility::Collapsed;
            else if (VisStr.Equals(TEXT("Hidden"), ESearchCase::IgnoreCase)) OutNode.ScrollBarVisibility = ESlateVisibility::Hidden;
            else if (VisStr.Equals(TEXT("HitTestInvisible"), ESearchCase::IgnoreCase)) OutNode.ScrollBarVisibility = ESlateVisibility::HitTestInvisible;
            else if (VisStr.Equals(TEXT("SelfHitTestInvisible"), ESearchCase::IgnoreCase)) OutNode.ScrollBarVisibility = ESlateVisibility::SelfHitTestInvisible;
            else OutNode.ScrollBarVisibility = ESlateVisibility::Visible; 
        }
    }
    // Spacer: Size
    else if (OutNode.Type == TEXT("Spacer"))
    {
        // Check Properties.Size
        const TSharedPtr<FJsonObject> *SizeObjPtr = nullptr;
        if (bHasProps && (*PropsPtr)->TryGetObjectField(TEXT("Size"), SizeObjPtr) && SizeObjPtr && SizeObjPtr->IsValid())
        {
            double SizeX = 0.0, SizeY = 0.0;
            (*SizeObjPtr)->TryGetNumberField(TEXT("X"), SizeX);
            (*SizeObjPtr)->TryGetNumberField(TEXT("Y"), SizeY);
            OutNode.bHasSpacerSize = true;
            OutNode.SpacerSize = FVector2D(SizeX, SizeY);
        }
    }

    const TArray<TSharedPtr<FJsonValue>> *Children = nullptr;
    if (NodeObj->TryGetArrayField(TEXT("Children"), Children) && Children)
    {
        for (const TSharedPtr<FJsonValue> &ChildVal : *Children)
        {
            if (!ChildVal.IsValid() || ChildVal->Type != EJson::Object)
            {
                continue;
            }
            FMWCS_HierarchyNode ChildNode;
            if (ParseHierarchyNode(ChildVal->AsObject(), ChildNode))
            {
                OutNode.Children.Add(MoveTemp(ChildNode));
            }
        }
    }

    // Apply Container Properties Macro (Spacing / SizeToContent)
    if (OutNode.Type == TEXT("VerticalBox") || OutNode.Type == TEXT("HorizontalBox"))
    {
        // Reuse PropsPtr and bHasProps from earlier in the function
        if (bHasProps)
        {
            // Spacing
            double Spacing = 0.0;
            if ((*PropsPtr)->TryGetNumberField(TEXT("Spacing"), Spacing) && Spacing > 0.0)
            {
                // Apply as padding to children (index > 0)
                // VBox: Top, HBox: Left
                bool bIsVBox = OutNode.Type == TEXT("VerticalBox");
                
                for (int32 i = 1; i < OutNode.Children.Num(); ++i)
                {
                    FMWCS_HierarchyNode &Child = OutNode.Children[i];
                    
                    if (!Child.bHasSlotPadding)
                    {
                        Child.bHasSlotPadding = true;
                        Child.SlotPadding = FMargin(0);
                    }
                    
                    if (bIsVBox)
                        Child.SlotPadding.Top = static_cast<float>(Spacing);
                    else
                        Child.SlotPadding.Left = static_cast<float>(Spacing);
                }
            }
            
            // SizeToContent
            bool bSizeToContent = false;
            if ((*PropsPtr)->TryGetBoolField(TEXT("SizeToContent"), bSizeToContent) && bSizeToContent)
            {
                // Force all children to Auto size
                for (FMWCS_HierarchyNode &Child : OutNode.Children)
                {
                    Child.bHasSlotSize = true;
                    Child.SlotSizeRule = ESlateSizeRule::Automatic;
                    Child.SlotSizeValue = 1.0f;
                }
            }
        }
    }

    return true;
}

static void ParseDependencies(const TSharedPtr<FJsonObject> &RootObj, TArray<FString> &OutDeps)
{
    OutDeps.Reset();

    if (!RootObj.IsValid())
    {
        return;
    }

    const TArray<TSharedPtr<FJsonValue>> *DepsArr = nullptr;
    if (!RootObj->TryGetArrayField(TEXT("Dependencies"), DepsArr) || !DepsArr)
    {
        return;
    }

    for (const TSharedPtr<FJsonValue> &V : *DepsArr)
    {
        FString S;
        if (V.IsValid() && V->TryGetString(S) && !S.IsEmpty())
        {
            OutDeps.Add(S);
        }
    }
}

static void ParseDesign(const TSharedPtr<FJsonObject> &RootObj, TMap<FName, TSharedPtr<FJsonObject>> &OutDesign)
{
    OutDesign.Reset();

    if (!RootObj.IsValid())
    {
        return;
    }

    const TSharedPtr<FJsonObject> *DesignObjPtr = nullptr;
    if (!RootObj->TryGetObjectField(TEXT("Design"), DesignObjPtr) || !DesignObjPtr || !DesignObjPtr->IsValid())
    {
        return;
    }

    const TSharedPtr<FJsonObject> DesignObj = *DesignObjPtr;
    for (const TPair<FString, TSharedPtr<FJsonValue>> &KV : DesignObj->Values)
    {
        if (KV.Key.IsEmpty() || !KV.Value.IsValid() || KV.Value->Type != EJson::Object)
        {
            continue;
        }

        TSharedPtr<FJsonObject> EntryObj = KV.Value->AsObject();
        if (!EntryObj.IsValid())
        {
            continue;
        }

        // Support both:
        //  - "Design": { "WidgetName": { ... } }
        //  - "Design": { "WidgetName": { "Properties": { ... } } }
        const TSharedPtr<FJsonObject> *PropsObjPtr = nullptr;
        if (EntryObj->TryGetObjectField(TEXT("Properties"), PropsObjPtr) && PropsObjPtr && PropsObjPtr->IsValid())
        {
            EntryObj = *PropsObjPtr;
        }

        OutDesign.Add(FName(*KV.Key), EntryObj);
    }
}

bool FMWCS_SpecParser::ParseSpecJson(const FString &JsonString, FMWCS_WidgetSpec &OutSpec, FMWCS_Report &InOutReport, const FString &Context)
{
    TSharedPtr<FJsonObject> RootObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Spec.InvalidJson"), TEXT("Failed to parse JSON."), Context);
        return false;
    }

    FString BlueprintName;
    FString ParentClass;
    FString Version;

    if (!ReadStringField(RootObj, TEXT("BlueprintName"), BlueprintName))
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Spec.MissingBlueprintName"), TEXT("Missing required field: BlueprintName"), Context);
        return false;
    }
    if (!ReadStringField(RootObj, TEXT("ParentClass"), ParentClass))
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Spec.MissingParentClass"), TEXT("Missing required field: ParentClass"), Context);
        return false;
    }

    {
        bool bIsToolEUW = false;
        RootObj->TryGetBoolField(TEXT("IsToolEUW"), bIsToolEUW);
        OutSpec.bIsToolEUW = bIsToolEUW;
    }
    if (!ReadVersionField(RootObj, TEXT("Version"), Version))
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Spec.MissingVersion"), TEXT("Missing required field: Version"), Context);
        return false;
    }

    const TSharedPtr<FJsonObject> *HierarchyObj = nullptr;
    if (!RootObj->TryGetObjectField(TEXT("Hierarchy"), HierarchyObj) || !HierarchyObj || !HierarchyObj->IsValid())
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Spec.MissingHierarchy"), TEXT("Missing required field: Hierarchy"), Context);
        return false;
    }

    FMWCS_Bindings Bindings;
    if (!ParseBindings(RootObj, Bindings))
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Warning, TEXT("Spec.MissingBindings"), TEXT("Bindings missing or invalid; continuing."), Context);
    }

    // Support both:
    //   - "Hierarchy": { "Type": ..., "Name": ..., "Children": [...] }
    //   - "Hierarchy": { "Root": { "Type": ..., ... } }
    const TSharedPtr<FJsonObject> *HierarchyRootObj = nullptr;
    if ((*HierarchyObj)->HasTypedField<EJson::Object>(TEXT("Root")) && (*HierarchyObj)->TryGetObjectField(TEXT("Root"), HierarchyRootObj) && HierarchyRootObj && HierarchyRootObj->IsValid())
    {
        // ok
    }
    else
    {
        HierarchyRootObj = HierarchyObj;
    }

    FMWCS_HierarchyNode RootNode;
    if (!ParseHierarchyNode(*HierarchyRootObj, RootNode))
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Spec.InvalidHierarchy"), TEXT("Hierarchy root node is invalid."), Context);
        return false;
    }

    OutSpec.BlueprintName = FName(*BlueprintName);
    OutSpec.ParentClassPath = ParentClass;
    OutSpec.Version = Version;
    if (!ParseDesignerPreview(RootObj, OutSpec.DesignerPreview, InOutReport, Context))
    {
        return false;
    }
    OutSpec.HierarchyRoot = MoveTemp(RootNode);
    OutSpec.Bindings = MoveTemp(Bindings);

    ParseDesign(RootObj, OutSpec.Design);
    ParseDependencies(RootObj, OutSpec.Dependencies);
    return true;
}
