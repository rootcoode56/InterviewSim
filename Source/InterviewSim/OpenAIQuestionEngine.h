// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "InterviewQuestion.h"
#include "OpenAIQuestionEngine.generated.h"

DECLARE_DELEGATE_OneParam(FOnQuestionsGenerated, const TArray<FInterviewQuestion>&);

UCLASS()
class INTERVIEWSIM_API UOpenAIQuestionEngine : public UObject
{
	GENERATED_BODY()

public:
	void GenerateQuestionsFromPrompt(const FString& JobPrompt, FOnQuestionsGenerated OnComplete);

private:
	FString GeminiApiKey;
	FString OpenRouterApiKey;

	bool bQuestionsAlreadyReturned = false;
	int32 GeminiRetryCount = 0;
	int32 MaxGeminiRetries = 2;


	bool LoadGeminiApiKeyFromConfig();
	bool LoadOpenRouterApiKeyFromConfig();
	void RetryGeminiRequest(const FString& JobPrompt, FOnQuestionsGenerated OnComplete);
	void GenerateQuestionsFromOpenRouter(const FString& JobPrompt, FOnQuestionsGenerated OnComplete);

	FString NormalizeStageValue(const FString& RawStage) const;
	FString NormalizeDifficultyValue(const FString& RawDifficulty) const;
	float NormalizeScoreValue(float RawScore) const;

	TArray<FInterviewQuestion> CreateLocalFallbackQuestions(const FString& JobPrompt) const;

	TArray<FInterviewQuestion> ParseQuestionsFromResponse(const FString& ResponseString);
	TArray<FInterviewQuestion> ParseQuestionsFromOpenRouterResponse(const FString& ResponseString);
};
