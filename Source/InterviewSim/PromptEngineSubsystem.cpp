// Fill out your copyright notice in the Description page of Project Settings.

#include "PromptEngineSubsystem.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Guid.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"


void UPromptEngineSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Nawshin Prompt Engine initialized successfully.")
    );
}

void UPromptEngineSubsystem::Deinitialize()
{
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Nawshin Prompt Engine deinitialized.")
	);

	Super::Deinitialize();
}


void UPromptEngineSubsystem::GenerateQuestionPromptFromFile(
    const FString& FilePath,
    EPromptDocumentType DocumentType)
{
    FString CleanFilePath =
        FilePath.TrimStartAndEnd();

    CleanFilePath.RemoveFromStart(TEXT("\""));
    CleanFilePath.RemoveFromEnd(TEXT("\""));

    if (CleanFilePath.IsEmpty())
    {
        const FString ErrorMessage =
            TEXT("No document file path was provided.");

        UE_LOG(
            LogTemp,
            Error,
            TEXT("%s"),
            *ErrorMessage
        );

        OnPromptGenerationFailed.Broadcast(
            ErrorMessage
        );

        return;
    }

    if (!FPaths::FileExists(CleanFilePath))
    {
        const FString ErrorMessage =
            FString::Printf(
                TEXT(
                    "The selected document does not exist: %s"
                ),
                *CleanFilePath
            );

        UE_LOG(
            LogTemp,
            Error,
            TEXT("%s"),
            *ErrorMessage
        );

        OnPromptGenerationFailed.Broadcast(
            ErrorMessage
        );

        return;
    }

    const FString FileExtension =
        FPaths::GetExtension(
            CleanFilePath,
            false
        ).ToLower();

    if (FileExtension != TEXT("txt") &&
        FileExtension != TEXT("json") &&
        FileExtension != TEXT("pdf"))
    {
        const FString ErrorMessage =
            FString::Printf(
                TEXT(
                    "Unsupported file type .%s. "
                    "Supported formats are TXT, JSON, and PDF."
                ),
                *FileExtension
            );

        UE_LOG(
            LogTemp,
            Error,
            TEXT("%s"),
            *ErrorMessage
        );

        OnPromptGenerationFailed.Broadcast(
            ErrorMessage
        );

        return;
    }

    FString ExtractedDocumentText;

    if (FileExtension == TEXT("pdf"))
    {
        FString PdfErrorMessage;

        if (!ExtractTextFromPdf(
            CleanFilePath,
            ExtractedDocumentText,
            PdfErrorMessage))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("%s"),
                *PdfErrorMessage
            );

            OnPromptGenerationFailed.Broadcast(
                PdfErrorMessage
            );

            return;
        }
    }
    else
    {
        if (!FFileHelper::LoadFileToString(
            ExtractedDocumentText,
            *CleanFilePath))
        {
            const FString ErrorMessage =
                FString::Printf(
                    TEXT(
                        "Failed to read the selected document: %s"
                    ),
                    *CleanFilePath
                );

            UE_LOG(
                LogTemp,
                Error,
                TEXT("%s"),
                *ErrorMessage
            );

            OnPromptGenerationFailed.Broadcast(
                ErrorMessage
            );

            return;
        }
    }

    ExtractedDocumentText =
        ExtractedDocumentText.TrimStartAndEnd();

    if (ExtractedDocumentText.IsEmpty())
    {
        const FString ErrorMessage =
            TEXT(
                "The selected document contains no readable text."
            );

        UE_LOG(
            LogTemp,
            Error,
            TEXT("%s"),
            *ErrorMessage
        );

        OnPromptGenerationFailed.Broadcast(
            ErrorMessage
        );

        return;
    }

    FString TypeValidationError;

    if (!ValidateExpectedDocumentType(
        ExtractedDocumentText,
        DocumentType,
        TypeValidationError))
    {
        OnPromptGenerationFailed.Broadcast(
            TypeValidationError
        );

        return;
    }

    if (FileExtension == TEXT("json"))
    {
        TSharedPtr<FJsonObject> ParsedJsonObject;

        const TSharedRef<TJsonReader<>> JsonReader =
            TJsonReaderFactory<>::Create(
                ExtractedDocumentText
            );

        if (!FJsonSerializer::Deserialize(
            JsonReader,
            ParsedJsonObject) ||
            !ParsedJsonObject.IsValid())
        {
            const FString ErrorMessage =
                TEXT(
                    "The selected JSON file is invalid or malformed."
                );

            UE_LOG(
                LogTemp,
                Error,
                TEXT("%s"),
                *ErrorMessage
            );

            OnPromptGenerationFailed.Broadcast(
                ErrorMessage
            );

            return;
        }
    }

    // ---------------------------------------------------------
    // CONTENT-BASED DOCUMENT TYPE DETECTION
    // ---------------------------------------------------------

    EPromptDocumentType EffectiveDocumentType =
        DocumentType;

    int32 JobCircularScore = 0;
    int32 CVResumeScore = 0;

    const TArray<FString> JobCircularSignals =
    {
        TEXT("application deadline"),
        TEXT("employment status"),
        TEXT("job location"),
        TEXT("compensation & other benefits"),
        TEXT("compensation and other benefits"),
        TEXT("responsibilities & context"),
        TEXT("responsibilities and context"),
        TEXT("additional requirements"),
        TEXT("salary review"),
        TEXT("festival bonus"),
        TEXT("workplace"),
        TEXT("vacancy"),
        TEXT("apply procedure"),
        TEXT("job requirements"),

        // Common structured JSON job-specification keys.
        TEXT("\"job_title\""),
        TEXT("\"job_description\""),
        TEXT("\"required_skills\""),
        TEXT("\"responsibilities\""),
        TEXT("\"qualifications\""),
        TEXT("\"job_requirements\""),
        TEXT("\"required_qualifications\""),

        TEXT("job title:"),
        TEXT("required qualifications:"),
        TEXT("required skills:"),
        TEXT("responsibilities:"),
        TEXT("preferred experience:"),
    };

    const TArray<FString> CVResumeSignals =
    {
        TEXT("professional summary"),
        TEXT("career objective"),
        TEXT("work experience"),
        TEXT("employment history"),
        TEXT("professional experience"),
        TEXT("projects"),
        TEXT("certifications"),
        TEXT("references"),
        TEXT("portfolio"),
        TEXT("linkedin"),
        TEXT("github")
    };

    for (const FString& Signal : JobCircularSignals)
    {
        if (ExtractedDocumentText.Contains(
            Signal,
            ESearchCase::IgnoreCase))
        {
            ++JobCircularScore;
        }
    }

    for (const FString& Signal : CVResumeSignals)
    {
        if (ExtractedDocumentText.Contains(
            Signal,
            ESearchCase::IgnoreCase))
        {
            ++CVResumeScore;
        }
    }

    // Override the UI-supplied document type only when the
    // document content gives strong enough evidence.
    if (JobCircularScore >= 2 &&
        JobCircularScore > CVResumeScore)
    {
        EffectiveDocumentType =
            EPromptDocumentType::JobCircular;
    }
    else if (CVResumeScore >= 2 &&
        CVResumeScore > JobCircularScore)
    {
        EffectiveDocumentType =
            EPromptDocumentType::CVResume;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "Document type detection | "
            "Requested: %s | "
            "Detected/Effective: %s | "
            "Job Circular Score: %d | "
            "CV/Resume Score: %d"
        ),
        DocumentType ==
        EPromptDocumentType::CVResume
        ? TEXT("CV/Resume")
        : TEXT("Job Circular"),

        EffectiveDocumentType ==
        EPromptDocumentType::CVResume
        ? TEXT("CV/Resume")
        : TEXT("Job Circular"),

        JobCircularScore,
        CVResumeScore
    );

    // ---------------------------------------------------------
    // SANITIZATION
    // ---------------------------------------------------------

    FString SanitizedDocumentText =
        ExtractedDocumentText;

    if (EffectiveDocumentType ==
        EPromptDocumentType::CVResume)
    {
        SanitizedDocumentText =
            SanitizeCVResumeText(
                ExtractedDocumentText
            );
    }

    SanitizedDocumentText =
        SanitizedDocumentText.TrimStartAndEnd();

    if (SanitizedDocumentText.IsEmpty())
    {
        const FString ErrorMessage =
            TEXT(
                "No usable document text remained "
                "after sanitization."
            );

        UE_LOG(
            LogTemp,
            Error,
            TEXT("%s"),
            *ErrorMessage
        );

        OnPromptGenerationFailed.Broadcast(
            ErrorMessage
        );

        return;
    }

    const FString FocusedPrompt =
        BuildFocusedQuestionGenerationPrompt(
            SanitizedDocumentText,
            EffectiveDocumentType
        );

    if (FocusedPrompt.IsEmpty())
    {
        const FString ErrorMessage =
            TEXT(
                "The Prompt Engine could not build "
                "a question-generation prompt."
            );

        UE_LOG(
            LogTemp,
            Error,
            TEXT("%s"),
            *ErrorMessage
        );

        OnPromptGenerationFailed.Broadcast(
            ErrorMessage
        );

        return;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "No-AI question-generation prompt is ready. "
            "Document type: %s. Prompt length: %d."
        ),
        EffectiveDocumentType ==
        EPromptDocumentType::CVResume
        ? TEXT("CV/Resume")
        : TEXT("Job Circular"),
        FocusedPrompt.Len()
    );

    OnQuestionGenerationPromptReady.Broadcast(
        FocusedPrompt
    );
}

bool UPromptEngineSubsystem::LoadDocumentTextForMultiFile(
    const FString& FilePath,
    FString& OutExtractedText,
    FString& OutErrorMessage
) const
{
    FString CleanFilePath = FilePath.TrimStartAndEnd();

    CleanFilePath.RemoveFromStart(TEXT("\""));
    CleanFilePath.RemoveFromEnd(TEXT("\""));

    if (CleanFilePath.IsEmpty())
    {
        OutErrorMessage = TEXT("No document file path was provided.");
        return false;
    }

    if (!FPaths::FileExists(CleanFilePath))
    {
        OutErrorMessage = FString::Printf(
            TEXT("The selected document does not exist: %s"),
            *CleanFilePath
        );
        return false;
    }

    const FString FileExtension =
        FPaths::GetExtension(CleanFilePath, false).ToLower();

    if (FileExtension != TEXT("txt") &&
        FileExtension != TEXT("json") &&
        FileExtension != TEXT("pdf"))
    {
        OutErrorMessage = FString::Printf(
            TEXT("Unsupported file type .%s. Supported formats are TXT, JSON, and PDF."),
            *FileExtension
        );
        return false;
    }

    if (FileExtension == TEXT("pdf"))
    {
        if (!ExtractTextFromPdf(
            CleanFilePath,
            OutExtractedText,
            OutErrorMessage))
        {
            return false;
        }
    }
    else
    {
        if (!FFileHelper::LoadFileToString(
            OutExtractedText,
            *CleanFilePath))
        {
            OutErrorMessage = FString::Printf(
                TEXT("Failed to read the selected document: %s"),
                *CleanFilePath
            );
            return false;
        }
    }

    OutExtractedText =
        OutExtractedText.TrimStartAndEnd();

    if (OutExtractedText.IsEmpty())
    {
        OutErrorMessage =
            TEXT("The selected document contains no readable text.");

        return false;
    }

    if (FileExtension == TEXT("json"))
    {
        TSharedPtr<FJsonObject> ParsedJsonObject;

        const TSharedRef<TJsonReader<>> JsonReader =
            TJsonReaderFactory<>::Create(
                OutExtractedText
            );

        if (!FJsonSerializer::Deserialize(
            JsonReader,
            ParsedJsonObject) ||
            !ParsedJsonObject.IsValid())
        {
            OutErrorMessage =
                TEXT("The selected JSON file is invalid or malformed.");

            return false;
        }
    }

    return true;
}

bool UPromptEngineSubsystem::ValidateExpectedDocumentType(
    const FString& DocumentText,
    EPromptDocumentType ExpectedType,
    FString& OutErrorMessage
) const
{
    int32 JobScore = 0;
    int32 CVScore = 0;

    const TArray<FString> StrongJobSignals =
    {
        TEXT("application deadline"),
        TEXT("employment status"),
        TEXT("job location"),
        TEXT("vacancy"),
        TEXT("apply procedure"),
        TEXT("job title:"),
        TEXT("required qualifications:"),
        TEXT("required skills:"),
        TEXT("responsibilities:"),
        TEXT("preferred experience:"),
        TEXT("\"job_title\""),
        TEXT("\"required_skills\""),
        TEXT("\"responsibilities\"")
    };

    const TArray<FString> StrongCVSignals =
    {
        TEXT("professional summary"),
        TEXT("career objective"),
        TEXT("work experience"),
        TEXT("employment history"),
        TEXT("professional experience"),
        TEXT("projects"),
        TEXT("certifications"),
        TEXT("references"),
        TEXT("portfolio"),
        TEXT("linkedin"),
        TEXT("github")
    };

    for (const FString& Signal : StrongJobSignals)
    {
        if (DocumentText.Contains(
            Signal,
            ESearchCase::IgnoreCase))
        {
            ++JobScore;
        }
    }

    for (const FString& Signal : StrongCVSignals)
    {
        if (DocumentText.Contains(
            Signal,
            ESearchCase::IgnoreCase))
        {
            ++CVScore;
        }
    }

    if (ExpectedType == EPromptDocumentType::CVResume &&
        JobScore >= 3 &&
        JobScore > CVScore)
    {
        OutErrorMessage =
            TEXT(
                "This file appears to be a Job Circular. "
                "Please upload it in the Job Circular field."
            );

        return false;
    }

    if (ExpectedType == EPromptDocumentType::JobCircular &&
        CVScore >= 3 &&
        CVScore > JobScore)
    {
        OutErrorMessage =
            TEXT(
                "This file appears to be a CV/Resume. "
                "Please upload it in the CV/Resume field."
            );

        return false;
    }

    return true;
}

void UPromptEngineSubsystem::GenerateQuestionPromptFromFiles(
    const FString& CVFilePath,
    const FString& JobCircularFilePath)
{
    FString CVText;
    FString JobCircularText;
    FString ErrorMessage;

    if (!LoadDocumentTextForMultiFile(
        CVFilePath,
        CVText,
        ErrorMessage))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("CV/Resume load failed: %s"),
            *ErrorMessage
        );

        OnPromptGenerationFailed.Broadcast(
            ErrorMessage
        );

        return;
    }



    if (!ValidateExpectedDocumentType(
        CVText,
        EPromptDocumentType::CVResume,
        ErrorMessage))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("CV/Resume validation failed: %s"),
            *ErrorMessage
        );

        OnPromptGenerationFailed.Broadcast(
            ErrorMessage
        );

        return;
    }

    if (!LoadDocumentTextForMultiFile(
        JobCircularFilePath,
        JobCircularText,
        ErrorMessage))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Job Circular load failed: %s"),
            *ErrorMessage
        );

        OnPromptGenerationFailed.Broadcast(
            ErrorMessage
        );

        return;
    }

    if (!ValidateExpectedDocumentType(
        JobCircularText,
        EPromptDocumentType::JobCircular,
        ErrorMessage))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Job Circular validation failed: %s"),
            *ErrorMessage
        );

        OnPromptGenerationFailed.Broadcast(
            ErrorMessage
        );

        return;
    }

    const FString SanitizedCVText =
        SanitizeCVResumeText(
            CVText
        ).TrimStartAndEnd();

    if (SanitizedCVText.IsEmpty())
    {
        const FString CVError =
            TEXT(
                "No usable CV/resume text remained "
                "after sanitization."
            );

        UE_LOG(
            LogTemp,
            Error,
            TEXT("%s"),
            *CVError
        );

        OnPromptGenerationFailed.Broadcast(
            CVError
        );

        return;
    }

    const FString CVFocusedPrompt =
        BuildFocusedQuestionGenerationPrompt(
            SanitizedCVText,
            EPromptDocumentType::CVResume
        );

    const FString JobFocusedPrompt =
        BuildFocusedQuestionGenerationPrompt(
            JobCircularText,
            EPromptDocumentType::JobCircular
        );

    const FString CombinedPrompt =
        FString::Printf(
            TEXT(
                "MULTI-DOCUMENT REAL INTERVIEW CONTEXT\n\n"
                "PRIMARY ROLE / JOB CIRCULAR CONTEXT:\n"
                "%s\n\n"
                "CANDIDATE CV / RESUME CONTEXT:\n"
                "%s\n\n"
                "MULTI-DOCUMENT MATCHING RULES:\n"
                "1. Treat the Job Circular as the primary authority for the target role.\n"
                "2. Use the CV/Resume as candidate-specific background evidence.\n"
                "3. Prefer questions that connect role requirements with relevant candidate background.\n"
                "4. Do not invent skills, experience, requirements, or qualifications.\n"
                "5. Keep all required question-count, stage, difficulty, and JSON-schema rules.\n"
            ),
            *JobFocusedPrompt,
            *CVFocusedPrompt
        );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "Multi-file prompt prepared | "
            "CV Prompt: %d | Job Prompt: %d | Combined: %d"
        ),
        CVFocusedPrompt.Len(),
        JobFocusedPrompt.Len(),
        CombinedPrompt.Len()
    );

    OnQuestionGenerationPromptReady.Broadcast(
        CombinedPrompt
    );
}

FString UPromptEngineSubsystem::BuildFocusedQuestionGenerationPrompt(
    const FString& SanitizedDocumentText,
    EPromptDocumentType DocumentType) const
{
    struct FDocumentLine
    {
        FString Text;
        FString NormalizedText;
        int32 SectionWeight = 1;
    };

    struct FFieldRule
    {
        FString FieldName;
        TArray<FString> Keywords;
    };

    struct FScoredTopic
    {
        FString Topic;
        FString Evidence;
        int32 Score = 0;
    };

    const TArray<FFieldRule> FieldRules =
    {
        {
            TEXT("Computer Science / Software / IT"),
            {
                TEXT("computer science"),
                TEXT("software"),
                TEXT("programming"),
                TEXT("developer"),
                TEXT("java"),
                TEXT("python"),
                TEXT("c++"),
                TEXT("c#"),
                TEXT("javascript"),
                TEXT("database"),
                TEXT("sql"),
                TEXT("api"),
                TEXT("unreal engine"),
                TEXT("machine learning"),
                TEXT("networking")
            }
        },
        {
            TEXT("Electrical and Electronic Engineering"),
            {
                TEXT("electrical engineering"),
                TEXT("electronic engineering"),
                TEXT("circuit"),
                TEXT("power system"),
                TEXT("transformer"),
                TEXT("plc"),
                TEXT("microcontroller"),
                TEXT("matlab"),
                TEXT("signal processing"),
                TEXT("control system")
            }
        },
        {
            TEXT("Civil Engineering"),
            {
                TEXT("civil engineering"),
                TEXT("construction"),
                TEXT("structural analysis"),
                TEXT("autocad"),
                TEXT("surveying"),
                TEXT("concrete"),
                TEXT("soil mechanics"),
                TEXT("estimation")
            }
        },
        {
            TEXT("Mechanical Engineering"),
            {
                TEXT("mechanical engineering"),
                TEXT("thermodynamics"),
                TEXT("fluid mechanics"),
                TEXT("manufacturing"),
                TEXT("solidworks"),
                TEXT("machine design"),
                TEXT("maintenance"),
                TEXT("hvac")
            }
        },
        {
            TEXT("Finance / Accounting"),
            {
                TEXT("accounting"),
                TEXT("finance"),
                TEXT("audit"),
                TEXT("taxation"),
                TEXT("financial reporting"),
                TEXT("ledger"),
                TEXT("budget"),
                TEXT("cash flow"),
                TEXT("balance sheet")
            }
        },
        {
            TEXT("Human Resources"),
            {
                TEXT("human resources"),
                TEXT("recruitment"),
                TEXT("onboarding"),
                TEXT("payroll"),
                TEXT("employee relations"),
                TEXT("performance management"),
                TEXT("talent acquisition")
            }
        },
        {
            TEXT("Marketing / Sales"),
            {
                TEXT("marketing"),
                TEXT("sales"),
                TEXT("digital marketing"),
                TEXT("social media"),
                TEXT("campaign"),
                TEXT("customer acquisition"),
                TEXT("market research"),
                TEXT("business development")
            }
        },
        {
            TEXT("Education"),
            {
                TEXT("education"),
                TEXT("teacher"),
                TEXT("teaching"),
                TEXT("lecturer"),
                TEXT("curriculum"),
                TEXT("classroom"),
                TEXT("student assessment")
            }
        },

        {
            TEXT("Veterinary Medicine"),
            {
                TEXT("veterinary medicine"),
                TEXT("veterinary"),
                TEXT("veterinarian"),
                TEXT("animal health"),
                TEXT("livestock health"),
                TEXT("companion animal")
            }
        },

        {
            TEXT("Healthcare"),
            {
                TEXT("healthcare"),
                TEXT("medical"),
                TEXT("nursing"),
                TEXT("patient"),
                TEXT("clinical"),
                TEXT("hospital"),
                TEXT("pharmacy"),
                TEXT("public health")
            }
        }
    };

    const TArray<FString> KnownTopics =
    {
        TEXT("C++"),
        TEXT("C#"),
        TEXT("Java"),
        TEXT("Python"),
        TEXT("JavaScript"),
        TEXT("TypeScript"),
        TEXT("PHP"),
        TEXT("SQL"),
        TEXT("MySQL"),
        TEXT("PostgreSQL"),
        TEXT("MongoDB"),
        TEXT("Spring Boot"),
        TEXT("Django"),
        TEXT("React"),
        TEXT("Node.js"),
        TEXT("REST API"),
        TEXT("Unreal Engine"),
        TEXT("Unreal Engine 5"),
        TEXT("Blueprint"),
        TEXT("Unity"),
        TEXT("Git"),
        TEXT("Machine Learning"),
        TEXT("Artificial Intelligence"),
        TEXT("Networking"),
        TEXT("Cybersecurity"),
        TEXT("MATLAB"),
        TEXT("PLC"),
        TEXT("Microcontroller"),
        TEXT("Circuit Analysis"),
        TEXT("Power System"),
        TEXT("Signal Processing"),
        TEXT("Control System"),
        TEXT("AutoCAD"),
        TEXT("Structural Analysis"),
        TEXT("Surveying"),
        TEXT("Construction"),
        TEXT("SolidWorks"),
        TEXT("Thermodynamics"),
        TEXT("Fluid Mechanics"),
        TEXT("HVAC"),
        TEXT("Financial Reporting"),
        TEXT("Accounting"),
        TEXT("Auditing"),
        TEXT("Taxation"),
        TEXT("Budgeting"),
        TEXT("Microsoft Excel"),
        TEXT("Recruitment"),
        TEXT("Onboarding"),
        TEXT("Payroll"),
        TEXT("Performance Management"),
        TEXT("Digital Marketing"),
        TEXT("Market Research"),
        TEXT("Sales"),
        TEXT("Teaching"),
        TEXT("Curriculum Development"),
        TEXT("Patient Care"),
        TEXT("Clinical Practice"),
        TEXT("Project Management"),
        TEXT("Communication"),
        TEXT("Problem Solving"),
        TEXT("Leadership"),
        TEXT("Teamwork")
    };

    TArray<FString> RawLines;

    SanitizedDocumentText.ParseIntoArrayLines(
        RawLines,
        false
    );

    TArray<FDocumentLine> DocumentLines;

    int32 CurrentSectionWeight = 1;
    int32 RejectedSensitiveOrAdministrativeLines = 0;

    for (const FString& RawLine : RawLines)
    {
        FString CleanLine =
            RawLine.TrimStartAndEnd();

        if (CleanLine.IsEmpty())
        {
            continue;
        }

        while (
            CleanLine.StartsWith(TEXT("•")) ||
            CleanLine.StartsWith(TEXT("*"))
            )
        {
            CleanLine = CleanLine.Mid(1);
            CleanLine.TrimStartAndEndInline();
        }

        if (CleanLine.IsEmpty())
        {
            continue;
        }

        FString NormalizedLine =
            CleanLine.ToLower();

        const bool bContainsSensitiveOrAdministrativeContent =
            NormalizedLine.Contains(TEXT("only male")) ||
            NormalizedLine.Contains(TEXT("only female")) ||
            NormalizedLine.Contains(TEXT("male candidates")) ||
            NormalizedLine.Contains(TEXT("female candidates")) ||
            NormalizedLine.Contains(TEXT("gender")) ||
            NormalizedLine.Contains(TEXT("age ")) ||
            NormalizedLine.StartsWith(TEXT("age")) ||
            NormalizedLine.Contains(TEXT("marital status")) ||
            NormalizedLine.Contains(TEXT("religion")) ||
            NormalizedLine.Contains(TEXT("nationality")) ||
            NormalizedLine.Contains(TEXT("date of birth")) ||
            NormalizedLine.Contains(TEXT("birth date")) ||
            NormalizedLine.Contains(TEXT("nid")) ||
            NormalizedLine.Contains(TEXT("national id")) ||
            NormalizedLine.Contains(TEXT("passport")) ||
            NormalizedLine.Contains(TEXT("application deadline")) ||
            NormalizedLine.Contains(TEXT("salary review")) ||
            NormalizedLine.Contains(TEXT("festival bonus")) ||
            NormalizedLine.Contains(TEXT("provident fund")) ||
            NormalizedLine.Contains(TEXT("insurance")) ||
            NormalizedLine.StartsWith(TEXT("compensation")) ||
            NormalizedLine.StartsWith(TEXT("workplace")) ||
            NormalizedLine.StartsWith(TEXT("employment status")) ||
            NormalizedLine.StartsWith(TEXT("job location")) ||
            NormalizedLine.StartsWith(TEXT("vacancy"));

        if (bContainsSensitiveOrAdministrativeContent)
        {
            ++RejectedSensitiveOrAdministrativeLines;
            continue;
        }

        FString HeadingText =
            NormalizedLine;

        HeadingText.ReplaceInline(
            TEXT(":"),
            TEXT("")
        );

        HeadingText =
            HeadingText.TrimStartAndEnd();

        if (CleanLine.Len() <= 80)
        {
            if (HeadingText.Contains(TEXT("required skill")) ||
                HeadingText.Contains(TEXT("technical skill")) ||
                HeadingText.Contains(TEXT("requirement")) ||
                HeadingText.Contains(TEXT("qualification")))
            {
                CurrentSectionWeight = 5;
                continue;
            }

            if (HeadingText.Contains(TEXT("responsibilit")) ||
                HeadingText.Contains(TEXT("job dut")) ||
                HeadingText.Contains(TEXT("role")))
            {
                CurrentSectionWeight = 5;
                continue;
            }

            if (HeadingText.Contains(TEXT("experience")) ||
                HeadingText.Contains(TEXT("project")))
            {
                CurrentSectionWeight = 4;
                continue;
            }

            if (HeadingText.Contains(TEXT("education")) ||
                HeadingText.Contains(TEXT("certification")) ||
                HeadingText.Contains(TEXT("summary")) ||
                HeadingText.Contains(TEXT("objective")))
            {
                CurrentSectionWeight = 3;
                continue;
            }
        }

        FDocumentLine DocumentLine;

        DocumentLine.Text =
            CleanLine.Left(500);

        DocumentLine.NormalizedText =
            NormalizedLine;

        DocumentLine.SectionWeight =
            CurrentSectionWeight;

        DocumentLines.Add(
            DocumentLine
        );
    }

    // ---------------------------------------------------------
    // CONTEXT-AWARE SHORT TECHNICAL TERM MATCHING
    // ---------------------------------------------------------

    auto IsShortTechnicalTerm =
        [](const FString& Term) -> bool
        {
            FString CompactTerm =
                Term.ToLower();

            CompactTerm.ReplaceInline(TEXT(" "), TEXT(""));

            bool bOnlySimpleCharacters = true;

            for (const TCHAR Character : CompactTerm)
            {
                if (!FChar::IsAlpha(Character) &&
                    !FChar::IsDigit(Character))
                {
                    bOnlySimpleCharacters = false;
                    break;
                }
            }

            return
                bOnlySimpleCharacters &&
                CompactTerm.Len() >= 2 &&
                CompactTerm.Len() <= 4;
        };

    auto HasTechnicalContext =
        [](const FDocumentLine& DocumentLine) -> bool
        {
            const FString& Line =
                DocumentLine.NormalizedText;

            return
                DocumentLine.SectionWeight >= 4 ||
                Line.Contains(TEXT("skill")) ||
                Line.Contains(TEXT("experience")) ||
                Line.Contains(TEXT("knowledge")) ||
                Line.Contains(TEXT("system")) ||
                Line.Contains(TEXT("technology")) ||
                Line.Contains(TEXT("tool")) ||
                Line.Contains(TEXT("software")) ||
                Line.Contains(TEXT("hardware")) ||
                Line.Contains(TEXT("network")) ||
                Line.Contains(TEXT("automation")) ||
                Line.Contains(TEXT("control")) ||
                Line.Contains(TEXT("program")) ||
                Line.Contains(TEXT("configuration")) ||
                Line.Contains(TEXT("configure")) ||
                Line.Contains(TEXT("installation")) ||
                Line.Contains(TEXT("troubleshoot")) ||
                Line.Contains(TEXT("maintenance")) ||
                Line.Contains(TEXT("diagnostic")) ||
                Line.Contains(TEXT("database")) ||
                Line.Contains(TEXT("server")) ||
                Line.Contains(TEXT("engineer"));
        };

    auto CountMatchingLines =
        [&DocumentLines](
            const FString& NormalizedTerm) -> int32
        {
            int32 MatchCount = 0;

            for (const FDocumentLine& DocumentLine :
                DocumentLines)
            {
                if (DocumentLine.NormalizedText.Contains(
                    NormalizedTerm))
                {
                    ++MatchCount;
                }
            }

            return MatchCount;
        };

    auto IsAcceptedTermMatch =
        [&IsShortTechnicalTerm,
        &HasTechnicalContext,
        &CountMatchingLines](
            const FDocumentLine& DocumentLine,
            const FString& Term) -> bool
        {
            const FString NormalizedTerm =
                Term.ToLower();

            if (!DocumentLine.NormalizedText.Contains(
                NormalizedTerm))
            {
                return false;
            }

            if (!IsShortTechnicalTerm(Term))
            {
                return true;
            }

            const int32 MatchingLineCount =
                CountMatchingLines(
                    NormalizedTerm
                );

            if (MatchingLineCount >= 2)
            {
                return true;
            }

            return HasTechnicalContext(
                DocumentLine
            );
        };

    FString DetectedField =
        TEXT("Unclassified Professional Field");

    FString FieldConfidence =
        TEXT("Low");

    int32 BestFieldScore = 0;
    int32 SecondBestFieldScore = 0;

    for (const FFieldRule& FieldRule : FieldRules)
    {
        int32 FieldScore = 0;

        for (const FString& Keyword :
            FieldRule.Keywords)
        {
            const FString NormalizedKeyword =
                Keyword.ToLower();

            const bool bSpecificMultiWordFieldPhrase =
                NormalizedKeyword.Contains(TEXT(" "));

            for (const FDocumentLine& DocumentLine :
                DocumentLines)
            {
                if (IsAcceptedTermMatch(
                    DocumentLine,
                    Keyword))
                {
                    int32 MatchScore =
                        DocumentLine.SectionWeight + 1;

                    // Multi-word phrases such as
                    // "electrical engineering",
                    // "computer science",
                    // "financial reporting", etc.
                    // are much stronger field evidence than generic
                    // single words such as "maintenance".
                    if (bSpecificMultiWordFieldPhrase)
                    {
                        MatchScore += 20;
                    }

                    FieldScore += MatchScore;
                }
            }
        }

        if (FieldScore > BestFieldScore)
        {
            SecondBestFieldScore =
                BestFieldScore;

            BestFieldScore =
                FieldScore;

            DetectedField =
                FieldRule.FieldName;
        }
        else if (FieldScore > SecondBestFieldScore)
        {
            SecondBestFieldScore =
                FieldScore;
        }
    }

    if (BestFieldScore >= 20 &&
        BestFieldScore >= SecondBestFieldScore + 5)
    {
        FieldConfidence =
            TEXT("High");
    }
    else if (BestFieldScore >= 8)
    {
        FieldConfidence =
            TEXT("Medium");
    }

    TArray<FScoredTopic> ScoredTopics;

    for (const FString& KnownTopic : KnownTopics)
    {
        FScoredTopic ScoredTopic;

        ScoredTopic.Topic =
            KnownTopic;

        for (const FDocumentLine& DocumentLine :
            DocumentLines)
        {
            if (IsAcceptedTermMatch(
                DocumentLine,
                KnownTopic))
            {
                ScoredTopic.Score +=
                    DocumentLine.SectionWeight + 1;

                if (ScoredTopic.Evidence.IsEmpty())
                {
                    ScoredTopic.Evidence =
                        DocumentLine.Text;
                }
            }
        }

        if (ScoredTopic.Score >= 6)
        {
            ScoredTopics.Add(
                ScoredTopic
            );
        }
    }

    ScoredTopics.Sort(
        [](const FScoredTopic& Left,
            const FScoredTopic& Right)
        {
            return Left.Score > Right.Score;
        }
    );

    TArray<FString> DetectedTopics;

    const int32 MaximumTopicCount =
        FMath::Min(
            ScoredTopics.Num(),
            10
        );

    for (int32 Index = 0;
        Index < MaximumTopicCount;
        Index++)
    {
        DetectedTopics.AddUnique(
            ScoredTopics[Index].Topic
        );
    }

    // ---------------------------------------------------------
    // HIGH-VALUE EVIDENCE QUALITY FILTERING
    // ---------------------------------------------------------

    auto HasLikelyMergedPdfColumns =
        [](const FString& Text) -> bool
        {
            int32 LowerToUpperTransitions = 0;

            for (int32 Index = 1;
                Index < Text.Len();
                ++Index)
            {
                const TCHAR Previous =
                    Text[Index - 1];

                const TCHAR Current =
                    Text[Index];

                if (FChar::IsLower(Previous) &&
                    FChar::IsUpper(Current))
                {
                    ++LowerToUpperTransitions;

                    if (LowerToUpperTransitions >= 2)
                    {
                        return true;
                    }
                }
            }

            return false;
        };

    auto IsWeakEvidenceLine =
        [&HasLikelyMergedPdfColumns](
            const FDocumentLine& DocumentLine) -> bool
        {
            const FString& Line =
                DocumentLine.NormalizedText;

            if (Line.IsEmpty())
            {
                return true;
            }

            if (DocumentLine.Text.Len() < 8)
            {
                return true;
            }

            if (Line.Equals(TEXT("others")) ||
                Line.Equals(TEXT("others.")) ||
                Line.StartsWith(TEXT("others if")) ||
                Line.StartsWith(TEXT("if necessary")) ||
                Line.StartsWith(TEXT("and ")) ||
                Line.StartsWith(TEXT("or ")))
            {
                return true;
            }

            if (HasLikelyMergedPdfColumns(
                DocumentLine.Text))
            {
                return true;
            }

            return false;
        };

    const TArray<FString> HighValueEvidenceSignals =
    {
        TEXT("troubleshoot"),
        TEXT("diagnos"),
        TEXT("install"),
        TEXT("repair"),
        TEXT("maintain"),
        TEXT("maintenance"),
        TEXT("service"),
        TEXT("support"),
        TEXT("configure"),
        TEXT("configuration"),
        TEXT("monitor"),
        TEXT("operate"),
        TEXT("design"),
        TEXT("develop"),
        TEXT("implement"),
        TEXT("analy"),
        TEXT("test"),
        TEXT("inspect"),
        TEXT("manage"),
        TEXT("prepare"),
        TEXT("document"),
        TEXT("report"),
        TEXT("resolve"),
        TEXT("handle"),
        TEXT("process"),
        TEXT("perform"),
        TEXT("coordinate"),
        TEXT("supervise"),
        TEXT("research"),
        TEXT("teach"),
        TEXT("evaluate"),
        TEXT("calibrat"),
        TEXT("customer"),
        TEXT("claim"),
        TEXT("replacement"),
        TEXT("project"),
        TEXT("responsib"),
        TEXT("requirement"),
        TEXT("skill"),
        TEXT("knowledge"),
        TEXT("experience")
    };

    struct FScoredEvidence
    {
        FString Text;
        int32 Score = 0;
    };

    TArray<FScoredEvidence> ScoredEvidenceLines;

    int32 RejectedWeakEvidenceLines = 0;

    for (const FDocumentLine& DocumentLine :
        DocumentLines)
    {
        if (IsWeakEvidenceLine(
            DocumentLine))
        {
            ++RejectedWeakEvidenceLines;
            continue;
        }

        FScoredEvidence Evidence;

        Evidence.Text =
            DocumentLine.Text;

        Evidence.Score =
            DocumentLine.SectionWeight;

        for (const FString& Topic :
            DetectedTopics)
        {
            if (IsAcceptedTermMatch(
                DocumentLine,
                Topic))
            {
                Evidence.Score += 3;
            }
        }

        for (const FString& Signal :
            HighValueEvidenceSignals)
        {
            if (DocumentLine.NormalizedText.Contains(
                Signal))
            {
                Evidence.Score += 2;
            }
        }

        if (DocumentLine.NormalizedText.Contains(
            TEXT("job title")) ||
            DocumentLine.NormalizedText.Contains(
                TEXT("position")) ||
            DocumentLine.NormalizedText.Contains(
                TEXT("responsib")) ||
            DocumentLine.NormalizedText.Contains(
                TEXT("experience")) ||
            DocumentLine.NormalizedText.Contains(
                TEXT("project")))
        {
            Evidence.Score += 2;
        }

        if (DocumentLine.NormalizedText.Contains(
            TEXT(
                "applicants should have experience "
                "in the following business area"
            )))
        {
            Evidence.Score -= 5;
        }

        if (DocumentLine.Text.EndsWith(TEXT("&")))
        {
            Evidence.Score -= 1;
        }

        if (Evidence.Score >= 6)
        {
            ScoredEvidenceLines.Add(
                Evidence
            );
        }
    }

    ScoredEvidenceLines.Sort(
        [](const FScoredEvidence& Left,
            const FScoredEvidence& Right)
        {
            return Left.Score > Right.Score;
        }
    );

    TArray<FString> SelectedEvidence;
    TSet<FString> UsedEvidence;

    for (const FScoredEvidence& Evidence :
        ScoredEvidenceLines)
    {
        const FString EvidenceKey =
            Evidence.Text.ToLower();

        if (UsedEvidence.Contains(
            EvidenceKey))
        {
            continue;
        }

        UsedEvidence.Add(
            EvidenceKey
        );

        SelectedEvidence.Add(
            Evidence.Text
        );

        if (SelectedEvidence.Num() >= 10)
        {
            break;
        }
    }

    if (SelectedEvidence.IsEmpty())
    {
        for (const FDocumentLine& DocumentLine :
            DocumentLines)
        {
            if (IsWeakEvidenceLine(
                DocumentLine))
            {
                continue;
            }

            SelectedEvidence.Add(
                DocumentLine.Text
            );

            if (SelectedEvidence.Num() >= 8)
            {
                break;
            }
        }
    }

    const FString DocumentTypeText =
        DocumentType ==
        EPromptDocumentType::CVResume
        ? TEXT("CV / Resume")
        : TEXT("Job Circular");

    const FString TopicsText =
        DetectedTopics.IsEmpty()
        ? TEXT(
            "No predefined topic was detected. "
            "Infer relevant topics only from the evidence."
        )
        : FString::Join(
            DetectedTopics,
            TEXT(", ")
        );

    FString EvidenceText;

    if (!SelectedEvidence.IsEmpty())
    {
        EvidenceText =
            TEXT("- ") +
            FString::Join(
                SelectedEvidence,
                TEXT("\n- ")
            );
    }
    else
    {
        EvidenceText =
            TEXT(
                "No high-confidence evidence lines were selected."
            );
    }

    const FString FocusedPrompt =
        FString::Printf(
            TEXT(
                "INTERVIEW QUESTION GENERATION CONTEXT\n\n"
                "DOCUMENT TYPE:\n%s\n\n"
                "RULE-BASED DETECTED FIELD:\n%s\n\n"
                "FIELD CONFIDENCE:\n%s\n\n"
                "DOCUMENTED TOPICS AND EXPERTISE:\n%s\n\n"
                "SELECTED SOURCE EVIDENCE:\n%s\n\n"
                "GENERATION RULES:\n"
                "1. Generate interview questions using only the "
                "documented topics and source evidence above.\n"
                "2. Infer the exact role and profession conservatively.\n"
                "3. Do not invent skills, tools, qualifications, "
                "experience, responsibilities, or projects.\n"
                "4. Treat Technical questions as role-specific or "
                "domain-specific questions, not necessarily programming.\n"
                "5. Match question difficulty to the documented "
                "experience and responsibilities.\n"
                "6. Do not mention the candidate's name, contact "
                "information, address, identification data, religion, "
                "medical information, gender, marital status, or politics.\n"
                "7. Do not create discriminatory or inappropriate "
                "questions.\n"
                "8. Avoid duplicate questions.\n"
                "9. Keep every question concise, clear, and directly "
                "grounded in the supplied evidence.\n"
                "10. Follow the Question Engine's required JSON schema, "
                "stage distribution, and question-count rules.\n"
            ),
            *DocumentTypeText,
            *DetectedField,
            *FieldConfidence,
            *TopicsText,
            *EvidenceText
        );

    UE_LOG(
        LogTemp,
        Log,
        TEXT(
            "Focused prompt created. Field: %s. "
            "Confidence: %s. Topics: %d. Evidence lines: %d. "
            "Rejected sensitive/administrative lines: %d. "
            "Rejected weak evidence lines: %d."
        ),
        *DetectedField,
        *FieldConfidence,
        DetectedTopics.Num(),
        SelectedEvidence.Num(),
        RejectedSensitiveOrAdministrativeLines,
        RejectedWeakEvidenceLines
    );

    return FocusedPrompt.TrimStartAndEnd();
}

FString UPromptEngineSubsystem::SanitizeCVResumeText(
    const FString& CVResumeText) const
{
    TArray<FString> OriginalLines;

    CVResumeText.ParseIntoArrayLines(
        OriginalLines,
        false
    );

    const TArray<FString> SensitivePrefixes =
    {
        TEXT("name:"),
        TEXT("full name:"),
        TEXT("candidate name:"),
        TEXT("email:"),
        TEXT("e-mail:"),
        TEXT("phone:"),
        TEXT("mobile:"),
        TEXT("telephone:"),
        TEXT("contact number:"),
        TEXT("address:"),
        TEXT("home address:"),
        TEXT("date of birth:"),
        TEXT("dob:"),
        TEXT("national id:"),
        TEXT("nid:"),
        TEXT("passport:"),
        TEXT("linkedin:"),
        TEXT("github:"),
        TEXT("portfolio:")
    };

    TArray<FString> SanitizedLines;
    int32 RemovedLineCount = 0;

    for (const FString& OriginalLine : OriginalLines)
    {
        const FString TrimmedLine =
            OriginalLine.TrimStartAndEnd();

        if (TrimmedLine.IsEmpty())
        {
            SanitizedLines.Add(FString());
            continue;
        }

        const FString LowercaseLine =
            TrimmedLine.ToLower();

        bool bContainsSensitiveIdentityData = false;

        for (const FString& SensitivePrefix :
            SensitivePrefixes)
        {
            if (LowercaseLine.StartsWith(
                SensitivePrefix))
            {
                bContainsSensitiveIdentityData = true;
                break;
            }
        }

        if (bContainsSensitiveIdentityData)
        {
            RemovedLineCount++;
            continue;
        }

        SanitizedLines.Add(
            TrimmedLine
        );
    }

    FString SanitizedText =
        FString::Join(
            SanitizedLines,
            TEXT("\n")
        );

    SanitizedText =
        SanitizedText.TrimStartAndEnd();

    UE_LOG(
        LogTemp,
        Log,
        TEXT(
            "CV/resume sanitization completed. "
            "Removed sensitive lines: %d. "
            "Remaining text length: %d characters."
        ),
        RemovedLineCount,
        SanitizedText.Len()
    );

    return SanitizedText;
}

FString UPromptEngineSubsystem::GetPdfToTextExecutablePath() const
{
    const FString PackagedExecutablePath =
        FPaths::ConvertRelativePathToFull(
            FPaths::Combine(
                FPlatformProcess::BaseDir(),
                TEXT("ThirdParty"),
                TEXT("Poppler"),
                TEXT("bin"),
                TEXT("pdftotext.exe")
            )
        );

    if (IFileManager::Get().FileExists(*PackagedExecutablePath))
    {
        return PackagedExecutablePath;
    }

    return FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectDir(),
            TEXT("ThirdParty"),
            TEXT("Poppler"),
            TEXT("bin"),
            TEXT("pdftotext.exe")
        )
    );
}

bool UPromptEngineSubsystem::ExtractTextFromPdf(
    const FString& PdfFilePath,
    FString& OutExtractedText,
    FString& OutErrorMessage
) const
{
    OutExtractedText.Empty();
    OutErrorMessage.Empty();

    const FString PdfToTextExecutablePath =
        GetPdfToTextExecutablePath();

    if (!FPaths::FileExists(PdfFilePath))
    {
        OutErrorMessage = FString::Printf(
            TEXT("PDF file does not exist: %s"),
            *PdfFilePath
        );

        return false;
    }

    if (!FPaths::FileExists(PdfToTextExecutablePath))
    {
        OutErrorMessage = FString::Printf(
            TEXT("pdftotext executable was not found: %s"),
            *PdfToTextExecutablePath
        );

        return false;
    }

    const FString OutputDirectory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("PromptEngine"),
        TEXT("PdfText")
    );

    if (!IFileManager::Get().MakeDirectory(
        *OutputDirectory,
        true))
    {
        OutErrorMessage = FString::Printf(
            TEXT("Could not create PDF extraction directory: %s"),
            *OutputDirectory
        );

        return false;
    }

    const FString OutputTextPath = FPaths::Combine(
        OutputDirectory,
        FString::Printf(
            TEXT("PdfExtract_%s.txt"),
            *FGuid::NewGuid().ToString(
                EGuidFormats::Digits
            )
        )
    );

    const FString ProcessArguments = FString::Printf(
        TEXT("-layout -enc UTF-8 \"%s\" \"%s\""),
        *PdfFilePath,
        *OutputTextPath
    );

    int32 ReturnCode = INDEX_NONE;
    FString StandardOutput;
    FString StandardError;

    const bool bProcessStarted =
        FPlatformProcess::ExecProcess(
            *PdfToTextExecutablePath,
            *ProcessArguments,
            &ReturnCode,
            &StandardOutput,
            &StandardError
        );

    if (!bProcessStarted)
    {
        OutErrorMessage =
            TEXT("Failed to start the pdftotext process.");

        return false;
    }

    if (ReturnCode != 0)
    {
        OutErrorMessage = FString::Printf(
            TEXT(
                "pdftotext failed with return code %d. "
                "Error: %s"
            ),
            ReturnCode,
            *StandardError
        );

        IFileManager::Get().Delete(
            *OutputTextPath
        );

        return false;
    }

    if (!FFileHelper::LoadFileToString(
        OutExtractedText,
        *OutputTextPath))
    {
        OutErrorMessage = FString::Printf(
            TEXT(
                "pdftotext finished, but the extracted "
                "text file could not be loaded: %s"
            ),
            *OutputTextPath
        );

        IFileManager::Get().Delete(
            *OutputTextPath
        );

        return false;
    }

    IFileManager::Get().Delete(
        *OutputTextPath
    );

    OutExtractedText =
        OutExtractedText.TrimStartAndEnd();

    if (OutExtractedText.IsEmpty())
    {
        OutErrorMessage =
            TEXT(
                "The PDF contains no readable embedded text. "
                "It may be a scanned or image-based PDF."
            );

        return false;
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT(
            "PDF text extraction succeeded. "
            "Extracted text length: %d characters."
        ),
        OutExtractedText.Len()
    );

    return true;
}