// Fill out your copyright notice in the Description page of Project Settings.

#include "InterviewEvalServerSubsystem.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#include "Engine/World.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "TimerManager.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    bool bInterviewEvalServerLaunchClaimed = false;
}

void UInterviewEvalServerSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (StartEvaluationServer())
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                HealthCheckTimerHandle,
                this,
                &UInterviewEvalServerSubsystem::CheckEvaluationServerHealth,
                15.0f,
                false
            );
        }
    }
}

void UInterviewEvalServerSubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(
            HealthCheckTimerHandle
        );
    }

    bServerHealthy = false;

    StopEvaluationServer();

    Super::Deinitialize();
}

bool UInterviewEvalServerSubsystem::StartEvaluationServer()
{
#if PLATFORM_WINDOWS

    // The same subsystem already started the server.
    if (EvaluationServerProcessHandle.IsValid() &&
        FPlatformProcess::IsProcRunning(
            EvaluationServerProcessHandle))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Interview evaluation server is already running.")
        );

        return true;
    }

    // Another subsystem instance already started or claimed it.
    if (bInterviewEvalServerLaunchClaimed)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "Interview evaluation server startup was already claimed "
                "by another subsystem instance."
            )
        );

        return false;
    }

    bInterviewEvalServerLaunchClaimed = true;

    const FString ExecutablePath =
        GetEvaluationServerExecutablePath();

    if (!IFileManager::Get().FileExists(
        *ExecutablePath))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "Optional InterviewEvalServer.exe was not found: %s"
            ),
            *ExecutablePath
        );

        bInterviewEvalServerLaunchClaimed = false;

        return false;
    }

    const FString WorkingDirectory =
        FPaths::GetPath(ExecutablePath);

    uint32 ProcessId = 0;

    EvaluationServerProcessHandle =
        FPlatformProcess::CreateProc(
            *ExecutablePath,
            TEXT(""),
            true,
            true,
            true,
            &ProcessId,
            0,
            *WorkingDirectory,
            nullptr
        );

    if (!EvaluationServerProcessHandle.IsValid())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "Optional InterviewEvalServer.exe could not be started."
            )
        );

        bInterviewEvalServerLaunchClaimed = false;

        return false;
    }

    bStartedEvaluationServer = true;

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "Optional interview evaluation server started. "
            "Process ID: %u | Path: %s"
        ),
        ProcessId,
        *ExecutablePath
    );

    return true;

#else

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "Interview evaluation sidecar is currently supported only on Windows."
        )
    );

    return false;

#endif
}

void UInterviewEvalServerSubsystem::StopEvaluationServer()
{
#if PLATFORM_WINDOWS

    if (!bStartedEvaluationServer ||
        !EvaluationServerProcessHandle.IsValid())
    {
        return;
    }

    if (FPlatformProcess::IsProcRunning(
        EvaluationServerProcessHandle))
    {
        FPlatformProcess::TerminateProc(
            EvaluationServerProcessHandle,
            true
        );
    }

    FPlatformProcess::CloseProc(
        EvaluationServerProcessHandle
    );

    EvaluationServerProcessHandle.Reset();
    bStartedEvaluationServer = false;

    bInterviewEvalServerLaunchClaimed = false;

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Optional interview evaluation server stopped.")
    );

#endif
}

FString
UInterviewEvalServerSubsystem::GetEvaluationServerExecutablePath() const
{
    const FString PackagedExecutablePath =
        FPaths::ConvertRelativePathToFull(
            FPaths::Combine(
                FPlatformProcess::BaseDir(),
                TEXT("InterviewEvalServer"),
                TEXT("InterviewEvalServer.exe")
            )
        );

    if (IFileManager::Get().FileExists(
        *PackagedExecutablePath))
    {
        return PackagedExecutablePath;
    }

    return FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectDir(),
            TEXT("ThirdParty"),
            TEXT("InterviewEvalServer"),
            TEXT("InterviewEvalServer.exe")
        )
    );
}

void UInterviewEvalServerSubsystem::CheckEvaluationServerHealth()
{
    bServerHealthy = false;

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();

    Request->SetURL(
        TEXT("http://127.0.0.1:8000/health")
    );

    Request->SetVerb(TEXT("GET"));
    Request->SetHeader(
        TEXT("Accept"),
        TEXT("application/json")
    );

    const TWeakObjectPtr<UInterviewEvalServerSubsystem> WeakThis(this);

    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis](
            FHttpRequestPtr HttpRequest,
            FHttpResponsePtr HttpResponse,
            bool bWasSuccessful)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            UInterviewEvalServerSubsystem* Subsystem =
                WeakThis.Get();

            const bool bReceivedHealthyResponse =
                bWasSuccessful &&
                HttpResponse.IsValid() &&
                HttpResponse->GetResponseCode() == 200;

            Subsystem->bServerHealthy =
                bReceivedHealthyResponse;

            if (bReceivedHealthyResponse)
            {
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT(
                        "Interview evaluation server health check passed. Response: %s"
                    ),
                    *HttpResponse->GetContentAsString()
                );
            }
            else
            {
                const int32 ResponseCode =
                    HttpResponse.IsValid()
                    ? HttpResponse->GetResponseCode()
                    : 0;

                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT(
                        "Interview evaluation server health check failed. Response code: %d"
                    ),
                    ResponseCode
                );
            }
        }
    );

    if (!Request->ProcessRequest())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "Could not start the interview evaluation server health request."
            )
        );
    }
}

void UInterviewEvalServerSubsystem::RequestDetailedEvaluation(
    const FString& Question,
    const FString& Answer,
    int32 AnswerRecordIndex)
{
    FEvaluationResult EmptyResult;

    const FString CleanQuestion =
        Question.TrimStartAndEnd();

    const FString CleanAnswer =
        Answer.TrimStartAndEnd();

    if (CleanQuestion.IsEmpty() ||
        CleanAnswer.IsEmpty())
    {
        OnSidecarEvaluationCompleted.Broadcast(
            EmptyResult,
            false,
            TEXT("unavailable"),
            TEXT("Question or answer was empty."),
            AnswerRecordIndex
        );

        return;
    }

    if (!bServerHealthy)
    {
        OnSidecarEvaluationCompleted.Broadcast(
            EmptyResult,
            false,
            TEXT("unavailable"),
            TEXT("Interview evaluation server is not healthy."),
            AnswerRecordIndex
        );

        return;
    }

    const TSharedPtr<FJsonObject> RequestJson =
        MakeShared<FJsonObject>();

    RequestJson->SetStringField(
        TEXT("question"),
        CleanQuestion
    );

    RequestJson->SetStringField(
        TEXT("answer"),
        CleanAnswer
    );

    FString RequestBody;

    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(
            &RequestBody
        );

    if (!FJsonSerializer::Serialize(
        RequestJson.ToSharedRef(),
        Writer))
    {
        OnSidecarEvaluationCompleted.Broadcast(
            EmptyResult,
            false,
            TEXT("unavailable"),
            TEXT("Could not create the evaluation request JSON."),
            AnswerRecordIndex
        );

        return;
    }

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();

    Request->SetURL(
        TEXT("http://127.0.0.1:8000/evaluate")
    );

    Request->SetVerb(TEXT("POST"));

    Request->SetHeader(
        TEXT("Content-Type"),
        TEXT("application/json")
    );

    Request->SetHeader(
        TEXT("Accept"),
        TEXT("application/json")
    );

    Request->SetContentAsString(
        RequestBody
    );

    const TWeakObjectPtr<UInterviewEvalServerSubsystem> WeakThis(this);

    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis, AnswerRecordIndex](
            FHttpRequestPtr HttpRequest,
            FHttpResponsePtr HttpResponse,
            bool bWasSuccessful)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            UInterviewEvalServerSubsystem* Subsystem =
                WeakThis.Get();

            FEvaluationResult Result;

            bool bRequestSucceeded = false;

            FString Source =
                TEXT("unavailable");

            FString ErrorMessage;

            if (bWasSuccessful &&
                HttpResponse.IsValid() &&
                HttpResponse->GetResponseCode() == 200)
            {
                TSharedPtr<FJsonObject> ResponseJson;

                const TSharedRef<TJsonReader<>> Reader =
                    TJsonReaderFactory<>::Create(
                        HttpResponse->GetContentAsString()
                    );

                if (FJsonSerializer::Deserialize(
                    Reader,
                    ResponseJson) &&
                    ResponseJson.IsValid())
                {
                    double NumberValue = 0.0;

                    if (ResponseJson->TryGetNumberField(
                        TEXT("score"),
                        NumberValue))
                    {
                        Result.Score =
                            static_cast<float>(NumberValue);
                    }

                    if (ResponseJson->TryGetNumberField(
                        TEXT("correctness"),
                        NumberValue))
                    {
                        Result.Correctness =
                            static_cast<float>(NumberValue);
                    }

                    if (ResponseJson->TryGetNumberField(
                        TEXT("clarity"),
                        NumberValue))
                    {
                        Result.Clarity =
                            static_cast<float>(NumberValue);
                    }

                    if (ResponseJson->TryGetNumberField(
                        TEXT("relevance"),
                        NumberValue))
                    {
                        Result.Relevance =
                            static_cast<float>(NumberValue);
                    }

                    if (ResponseJson->TryGetNumberField(
                        TEXT("confidence"),
                        NumberValue))
                    {
                        Result.Confidence =
                            static_cast<float>(NumberValue);
                    }

                    if (ResponseJson->TryGetNumberField(
                        TEXT("grammar_mistakes"),
                        NumberValue))
                    {
                        Result.GrammarMistakeCount =
                            static_cast<int32>(NumberValue);
                    }

                    ResponseJson->TryGetBoolField(
                        TEXT("repeated"),
                        Result.bRepeated
                    );

                    ResponseJson->TryGetStringField(
                        TEXT("feedback"),
                        Result.Feedback
                    );

                    ResponseJson->TryGetStringField(
                        TEXT("source"),
                        Source
                    );

                    ResponseJson->TryGetStringField(
                        TEXT("error"),
                        ErrorMessage
                    );

                    const TArray<TSharedPtr<FJsonValue>>*
                        StrengthValues = nullptr;

                    if (ResponseJson->TryGetArrayField(
                        TEXT("strengths"),
                        StrengthValues))
                    {
                        for (const TSharedPtr<FJsonValue>& Value :
                            *StrengthValues)
                        {
                            FString Strength;

                            if (Value.IsValid() &&
                                Value->TryGetString(Strength))
                            {
                                Result.Strengths.Add(Strength);
                            }
                        }
                    }

                    const TArray<TSharedPtr<FJsonValue>>*
                        WeaknessValues = nullptr;

                    if (ResponseJson->TryGetArrayField(
                        TEXT("weaknesses"),
                        WeaknessValues))
                    {
                        for (const TSharedPtr<FJsonValue>& Value :
                            *WeaknessValues)
                        {
                            FString Weakness;

                            if (Value.IsValid() &&
                                Value->TryGetString(Weakness))
                            {
                                Result.Weaknesses.Add(Weakness);
                            }
                        }
                    }

                    bRequestSucceeded = true;

                    UE_LOG(
                        LogTemp,
                        Warning,
                        TEXT(
                            "Detailed sidecar evaluation received. "
                            "Source: %s | Score: %.2f"
                        ),
                        *Source,
                        Result.Score
                    );
                }
                else
                {
                    ErrorMessage =
                        TEXT(
                            "The evaluation server returned invalid JSON."
                        );
                }
            }
            else
            {
                const int32 ResponseCode =
                    HttpResponse.IsValid()
                    ? HttpResponse->GetResponseCode()
                    : 0;

                ErrorMessage =
                    FString::Printf(
                        TEXT(
                            "Evaluation request failed. "
                            "Response code: %d"
                        ),
                        ResponseCode
                    );
            }

            Subsystem->OnSidecarEvaluationCompleted.Broadcast(
                Result,
                bRequestSucceeded,
                Source,
                ErrorMessage,
                AnswerRecordIndex
            );
        }
    );

    if (!Request->ProcessRequest())
    {
        OnSidecarEvaluationCompleted.Broadcast(
            EmptyResult,
            false,
            TEXT("unavailable"),
            TEXT("Could not start the evaluation HTTP request."),
            AnswerRecordIndex
        );
    }
}