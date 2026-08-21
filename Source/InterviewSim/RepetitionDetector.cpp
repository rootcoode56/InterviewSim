// Fill out your copyright notice in the Description page of Project Settings.


#include "RepetitionDetector.h"

bool URepetitionDetector::IsRepeatedAnswer(const FString& NewAnswer)
{
	for (const FString& OldAnswer : PreviousAnswers)
	{
		float Similarity = CalculateSimilarity(NewAnswer, OldAnswer);

		if (Similarity >= 0.8f)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Repeated answer detected"));

			return true;
		}
	}

	PreviousAnswers.Add(NewAnswer);

	return false;
}

float URepetitionDetector::CalculateSimilarity(
	const FString& A,
	const FString& B)
{
	if (A.IsEmpty() || B.IsEmpty())
	{
		return 0.0f;
	}

	int32 MatchCount = 0;

	int32 MinLength = FMath::Min(A.Len(), B.Len());
	int32 MaxLength = FMath::Max(A.Len(), B.Len());

	for (int32 i = 0; i < MinLength; i++)
	{
		if (A[i] == B[i])
		{
			MatchCount++;
		}
	}

	return static_cast<float>(MatchCount) /
		static_cast<float>(MaxLength);
}
