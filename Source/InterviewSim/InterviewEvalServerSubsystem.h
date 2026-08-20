// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InterviewTypes.h"
#include "InterviewEvalServerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
    FOnSidecarEvaluationCompleted,
    FEvaluationResult,
    Result,
    bool,
    bRequestSucceeded,
    FString,
    Source,
    FString,
    ErrorMessage,
    int32,
    AnswerRecordIndex
);

UCLASS()
class INTERVIEWSIM_API UInterviewEvalServerSubsystem
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    UPROPERTY(BlueprintAssignable, Category = "Interview Evaluation Server")
    FOnSidecarEvaluationCompleted OnSidecarEvaluationCompleted;

    virtual void Initialize(
        FSubsystemCollectionBase& Collection
    ) override;

    virtual void Deinitialize() override;

    UFUNCTION(BlueprintPure, Category = "Interview Evaluation Server")
    bool IsEvaluationServerHealthy() const
    {
        return bServerHealthy;
    }

    UFUNCTION(BlueprintCallable, Category = "Interview Evaluation Server")
    void RequestDetailedEvaluation(
        const FString& Question,
        const FString& Answer,
        int32 AnswerRecordIndex
    );

private:
    bool StartEvaluationServer();

    void StopEvaluationServer();

    FString GetEvaluationServerExecutablePath() const;

    FProcHandle EvaluationServerProcessHandle;

    bool bStartedEvaluationServer = false;

    void CheckEvaluationServerHealth();

    FTimerHandle HealthCheckTimerHandle;

    bool bServerHealthy = false;
};