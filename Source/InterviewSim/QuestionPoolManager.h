// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "InterviewQuestion.h"
#include "QuestionPoolManager.generated.h"

UCLASS()
class INTERVIEWSIM_API UQuestionPoolManager : public UObject
{
	GENERATED_BODY()

public:
	void AddQuestion(const FInterviewQuestion& Question);
	void AddQuestions(const TArray<FInterviewQuestion>& Questions);
	
	TArray<FInterviewQuestion> GetAllQuestions() const;

	FInterviewQuestion GetBestQuestion();

	FInterviewQuestion GetBestQuestionForStageAndDifficulty(
		const FString& TargetStage,
		const FString& TargetDifficulty
	);

private:
	UPROPERTY()
	TArray<FInterviewQuestion> QuestionPool;

	UPROPERTY()
	TArray<FString> AskedQuestions;

	FString NormalizeQuestion(const FString& QuestionText) const;

	bool IsDuplicate(const FString& NewQuestion) const;

	bool IsIntroductionQuestion(const FString& QuestionText) const;
};