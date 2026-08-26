// Fill out your copyright notice in the Description page of Project Settings.

#include "QuestionPoolManager.h"

void UQuestionPoolManager::AddQuestion(
	const FInterviewQuestion& Question
)
{
	const FString CleanQuestionText =
		Question.QuestionText.TrimStartAndEnd();

	const FString CleanStage =
		Question.Stage.TrimStartAndEnd();

	const FString CleanDifficulty =
		Question.Difficulty.TrimStartAndEnd();

	if (CleanQuestionText.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Empty question skipped.")
		);
		return;
	}

	FInterviewQuestion SafeQuestion;

	SafeQuestion.QuestionText = CleanQuestionText;
	SafeQuestion.Stage = CleanStage;
	SafeQuestion.Difficulty = CleanDifficulty;
	SafeQuestion.Score = Question.Score;
	SafeQuestion.SourceModel = Question.SourceModel;
	SafeQuestion.bUsed = false;

	QuestionPool.Add(SafeQuestion);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Question added safely without duplicate check | Stage: %s | "
			"Difficulty: %s | Question: %s"
		),
		*SafeQuestion.Stage,
		*SafeQuestion.Difficulty,
		*SafeQuestion.QuestionText
	);
}
void UQuestionPoolManager::AddQuestions(
	const TArray<FInterviewQuestion>& Questions
)
{
	for (const FInterviewQuestion& Question : Questions)
	{
		AddQuestion(Question);
	}
}

TArray<FInterviewQuestion>
UQuestionPoolManager::GetAllQuestions() const
{
	return QuestionPool;
}

FInterviewQuestion UQuestionPoolManager::GetBestQuestion()
{
	if (QuestionPool.Num() == 0)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("QuestionPool is empty.")
		);
		return FInterviewQuestion();
	}

	QuestionPool.Sort(
		[](
			const FInterviewQuestion& A,
			const FInterviewQuestion& B
			)
		{
			return A.Score > B.Score;
		}
	);

	for (FInterviewQuestion& CandidateQuestion : QuestionPool)
	{
		if (CandidateQuestion.bUsed)
		{
			continue;
		}

		if (CandidateQuestion.QuestionText.IsEmpty())
		{
			continue;
		}

		CandidateQuestion.bUsed = true;
		AskedQuestions.Add(CandidateQuestion.QuestionText);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Best unused question selected: %s"),
			*CandidateQuestion.QuestionText
		);

		return CandidateQuestion;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("No unused question found in QuestionPool.")
	);

	return FInterviewQuestion();
}

FInterviewQuestion
UQuestionPoolManager::GetBestQuestionForStageAndDifficulty(
	const FString& TargetStage,
	const FString& TargetDifficulty
)
{
	if (QuestionPool.Num() == 0)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("QuestionPool is empty.")
		);
		return FInterviewQuestion();
	}

	QuestionPool.Sort(
		[](
			const FInterviewQuestion& A,
			const FInterviewQuestion& B
			)
		{
			return A.Score > B.Score;
		}
	);

	// =====================================================
	// First pass: exact Stage + Difficulty
	// =====================================================

	for (FInterviewQuestion& CandidateQuestion : QuestionPool)
	{
		if (CandidateQuestion.bUsed)
		{
			continue;
		}

		if (CandidateQuestion.QuestionText.IsEmpty())
		{
			continue;
		}

		if (
			!CandidateQuestion.Stage.Equals(
				TargetStage,
				ESearchCase::IgnoreCase
			)
			)
		{
			continue;
		}

		if (
			!CandidateQuestion.Difficulty.Equals(
				TargetDifficulty,
				ESearchCase::IgnoreCase
			)
			)
		{
			continue;
		}

		CandidateQuestion.bUsed = true;
		AskedQuestions.Add(CandidateQuestion.QuestionText);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Best question selected for "
				"Stage [%s] Difficulty [%s]: %s"
			),
			*TargetStage,
			*TargetDifficulty,
			*CandidateQuestion.QuestionText
		);

		return CandidateQuestion;
	}

	// =====================================================
	// Second pass: same Stage, any Difficulty
	// =====================================================

	for (FInterviewQuestion& CandidateQuestion : QuestionPool)
	{
		if (CandidateQuestion.bUsed)
		{
			continue;
		}

		if (CandidateQuestion.QuestionText.IsEmpty())
		{
			continue;
		}

		if (
			!CandidateQuestion.Stage.Equals(
				TargetStage,
				ESearchCase::IgnoreCase
			)
			)
		{
			continue;
		}

		CandidateQuestion.bUsed = true;
		AskedQuestions.Add(CandidateQuestion.QuestionText);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"No exact difficulty match. "
				"Selected same-stage question [%s / %s]: %s"
			),
			*CandidateQuestion.Stage,
			*CandidateQuestion.Difficulty,
			*CandidateQuestion.QuestionText
		);

		return CandidateQuestion;
	}

	// Do not select a question from another stage.
	// Returning an empty question lets InterviewSessionManager
	// request regeneration while preserving FSM authority.
	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"No unused same-stage question found for "
			"Stage [%s] Difficulty [%s]."
		),
		*TargetStage,
		*TargetDifficulty
	);

	return FInterviewQuestion();
}

FString UQuestionPoolManager::NormalizeQuestion(
	const FString& QuestionText
) const
{
	FString LowerText = QuestionText.ToLower();
	LowerText.TrimStartAndEndInline();

	FString NormalizedText;
	NormalizedText.Reserve(LowerText.Len());

	bool bPreviousCharacterWasSpace = false;

	for (const TCHAR Character : LowerText)
	{
		if (FChar::IsAlnum(Character))
		{
			NormalizedText.AppendChar(Character);
			bPreviousCharacterWasSpace = false;
		}
		else if (!bPreviousCharacterWasSpace)
		{
			NormalizedText.AppendChar(TEXT(' '));
			bPreviousCharacterWasSpace = true;
		}
	}

	NormalizedText.TrimStartAndEndInline();

	return NormalizedText;
}

bool UQuestionPoolManager::IsDuplicate(
	const FString& NewQuestion
) const
{
	const FString NormalizedNewQuestion =
		NormalizeQuestion(NewQuestion);

	if (NormalizedNewQuestion.IsEmpty())
	{
		return false;
	}

	for (const FInterviewQuestion& ExistingQuestion : QuestionPool)
	{
		const FString NormalizedExistingQuestion =
			NormalizeQuestion(
				ExistingQuestion.QuestionText
			);

		if (NormalizedExistingQuestion.IsEmpty())
		{
			continue;
		}

		if (
			NormalizedExistingQuestion.Equals(
				NormalizedNewQuestion,
				ESearchCase::CaseSensitive
			)
			)
		{
			return true;
		}

		// Catch questions that differ only by a short opening
		// or closing phrase.
		if (
			NormalizedNewQuestion.Len() >= 25 &&
			NormalizedExistingQuestion.Len() >= 25 &&
			(
				NormalizedNewQuestion.Contains(
					NormalizedExistingQuestion
				) ||
				NormalizedExistingQuestion.Contains(
					NormalizedNewQuestion
				)
				)
			)
		{
			return true;
		}
	}

	for (const FString& AskedQuestion : AskedQuestions)
	{
		const FString NormalizedAskedQuestion =
			NormalizeQuestion(AskedQuestion);

		if (
			NormalizedAskedQuestion.Equals(
				NormalizedNewQuestion,
				ESearchCase::CaseSensitive
			)
			)
		{
			return true;
		}
	}

	return false;
}

bool UQuestionPoolManager::IsIntroductionQuestion(
	const FString& QuestionText
) const
{
	const FString NormalizedQuestion =
		NormalizeQuestion(QuestionText);

	if (NormalizedQuestion.IsEmpty())
	{
		return false;
	}

	static const TArray<FString> IntroductionPhrases =
	{
		TEXT("tell me about yourself"),
		TEXT("tell us about yourself"),
		TEXT("tell me a bit about yourself"),
		TEXT("tell us a bit about yourself"),
		TEXT("tell me a little about yourself"),
		TEXT("tell us a little about yourself"),
		TEXT("tell me a little bit about yourself"),
		TEXT("tell us a little bit about yourself"),
		TEXT("introduce yourself"),
		TEXT("brief introduction"),
		TEXT("provide an introduction"),
		TEXT("personal introduction"),
		TEXT("professional background"),
		TEXT("overview of your background"),
		TEXT("overview of your career"),
		TEXT("background and experience"),
		TEXT("who you are")
	};

	for (const FString& IntroductionPhrase : IntroductionPhrases)
	{
		if (NormalizedQuestion.Contains(IntroductionPhrase))
		{
			return true;
		}
	}

	return false;
}

