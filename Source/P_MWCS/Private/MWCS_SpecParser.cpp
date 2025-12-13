#include "MWCS_SpecParser.h"

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

static void AddIssue(FMWCS_Report &Report, EMWCS_IssueSeverity Severity, const FString &Code, const FString &Message, const FString &Context)
{
    FMWCS_Issue Issue;
    Issue.Severity = Severity;
    Issue.Code = Code;
    Issue.Message = Message;
    Issue.Context = Context;
    Report.Issues.Add(MoveTemp(Issue));
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

    ParseStringArray(*BindingsObj, TEXT("Required"), OutBindings.Required);
    ParseStringArray(*BindingsObj, TEXT("Optional"), OutBindings.Optional);
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

    bool bIsVariable = true;
    NodeObj->TryGetBoolField(TEXT("IsVariable"), bIsVariable);
    OutNode.bIsVariable = bIsVariable;

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

    return true;
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
    int32 Version = 0;

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
    if (!ReadIntField(RootObj, TEXT("Version"), Version))
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

    FMWCS_HierarchyNode RootNode;
    if (!ParseHierarchyNode(*HierarchyObj, RootNode))
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Spec.InvalidHierarchy"), TEXT("Hierarchy root node is invalid."), Context);
        return false;
    }

    OutSpec.BlueprintName = FName(*BlueprintName);
    OutSpec.ParentClassPath = ParentClass;
    OutSpec.Version = Version;
    OutSpec.HierarchyRoot = MoveTemp(RootNode);
    OutSpec.Bindings = MoveTemp(Bindings);
    return true;
}
