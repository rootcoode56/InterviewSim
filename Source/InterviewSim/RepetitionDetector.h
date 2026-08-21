// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RepetitionDetector.generated.h"

UCLASS()
class INTERVIEWSIM_API URepetitionDetector : public UObject
{
	GENERATED_BODY()

public:

	bool IsRepeatedAnswer(const FString& NewAnswer);

private:

	UPROPERTY()
	TArray<FString> PreviousAnswers;

	float CalculateSimilarity(
		const FString& A,
		const FString& B
	);
};
