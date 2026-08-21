// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InterviewTypes.h"
#include "Http.h"
#include "EvaluationEngine.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnEvaluationCompleted,
	FEvaluationResult,
	Result
);

UCLASS()
class INTERVIEWSIM_API UEvaluationEngine : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnEvaluationCompleted OnEvaluationCompleted;

	void EvaluateAnswer(
		const FString& Question,
		const FString& Answer
	);

private:

	void OnEvaluationResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bSuccess
	);
};
