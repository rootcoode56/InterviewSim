#pragma once

#include "CoreMinimal.h"
#include "InterviewQuestion.generated.h"

USTRUCT(BlueprintType)
struct FInterviewQuestion
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString QuestionText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Stage;
	// Example: Warmup, Technical, Behavioral, Closing

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Difficulty;
	// Example: Easy, Medium, Hard

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Score = 0.0f;
	// Used for ranking/selecting the best question

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString SourceModel;
	// Example: Gemini, DeepSeek, OpenRouter, Fallback

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bUsed = false;
	// True after this question has already been asked
};