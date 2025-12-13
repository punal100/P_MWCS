#include "MWCS_Report.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

int32 FMWCS_Report::NumErrors() const
{
    int32 Count = 0;
    for (const FMWCS_Issue &Issue : Issues)
    {
        if (Issue.Severity == EMWCS_IssueSeverity::Error)
        {
            ++Count;
        }
    }
    return Count;
}

int32 FMWCS_Report::NumWarnings() const
{
    int32 Count = 0;
    for (const FMWCS_Issue &Issue : Issues)
    {
        if (Issue.Severity == EMWCS_IssueSeverity::Warning)
        {
            ++Count;
        }
    }
    return Count;
}

bool FMWCS_Report::HasErrors() const
{
    return NumErrors() > 0;
}

bool FMWCS_Report::HasWarnings() const
{
    return NumWarnings() > 0;
}

static FString SeverityToString(EMWCS_IssueSeverity Severity)
{
    switch (Severity)
    {
    case EMWCS_IssueSeverity::Info:
        return TEXT("Info");
    case EMWCS_IssueSeverity::Warning:
        return TEXT("Warning");
    case EMWCS_IssueSeverity::Error:
        return TEXT("Error");
    default:
        return TEXT("Info");
    }
}

FString MWCS_ReportJson::ToJsonString(const FMWCS_Report &Report)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("SpecsProcessed"), Report.SpecsProcessed);
    Root->SetNumberField(TEXT("AssetsCreated"), Report.AssetsCreated);
    Root->SetNumberField(TEXT("AssetsRepaired"), Report.AssetsRepaired);
    Root->SetNumberField(TEXT("AssetsRecreated"), Report.AssetsRecreated);
    Root->SetNumberField(TEXT("Errors"), Report.NumErrors());
    Root->SetNumberField(TEXT("Warnings"), Report.NumWarnings());

    TArray<TSharedPtr<FJsonValue>> Issues;
    Issues.Reserve(Report.Issues.Num());
    for (const FMWCS_Issue &Issue : Report.Issues)
    {
        TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("Severity"), SeverityToString(Issue.Severity));
        Obj->SetStringField(TEXT("Code"), Issue.Code);
        Obj->SetStringField(TEXT("Message"), Issue.Message);
        Obj->SetStringField(TEXT("Context"), Issue.Context);
        Issues.Add(MakeShared<FJsonValueObject>(Obj));
    }
    Root->SetArrayField(TEXT("Issues"), Issues);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Root, Writer);
    return Out;
}
