// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InterviewTypes.generated.h"

UENUM(BlueprintType)
enum class EInterviewState : uint8
{
	None        UMETA(DisplayName = "None"),
	Greeting    UMETA(DisplayName = "Greeting"),
	Warmup      UMETA(DisplayName = "Warmup"),
	Technical   UMETA(DisplayName = "Technical"),
	Behavioral  UMETA(DisplayName = "Behavioral"),
	Closing     UMETA(DisplayName = "Closing"),
	Finished    UMETA(DisplayName = "Finished")
};

// Defines which interview workflow is currently active.
UENUM(BlueprintType)
enum class EInterviewMode : uint8
{
	None               UMETA(DisplayName = "None"),
	RealInterview      UMETA(DisplayName = "Real Interview"),
	PracticeInterview  UMETA(DisplayName = "Practice Interview")
};

UENUM(BlueprintType)
enum class EAnswerInputType : uint8
{
	SpeechSTT    UMETA(DisplayName = "Speech STT"),
	WrittenText  UMETA(DisplayName = "Written Text"),
	WrittenCode  UMETA(DisplayName = "Written Code")
};

USTRUCT(BlueprintType)
struct FLightweightEvaluationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float QuickScore = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float RelevanceScore = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float ClarityScore = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 GrammarMistakeCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsRepeated = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bShouldContinueInterview = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Feedback;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString WarningMessage;
};

USTRUCT(BlueprintType)
struct FEvaluationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Score = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Correctness = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Clarity = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Relevance = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Confidence = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 GrammarMistakeCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Feedback;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FString> Strengths;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FString> Weaknesses;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bRepeated = false;
};

USTRUCT(BlueprintType)
struct FInterviewAnswerRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Question;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Answer;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EAnswerInputType AnswerInputType = EAnswerInputType::SpeechSTT;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString InterviewState;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Timestamp;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FEvaluationResult Evaluation;
};