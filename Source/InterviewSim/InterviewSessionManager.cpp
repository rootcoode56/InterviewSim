#include "InterviewSessionManager.h"
#include "OpenAIQuestionEngine.h"
#include "QuestionPoolManager.h"
#include "EvaluationEngine.h"
#include "RepetitionDetector.h"
#include "ConversationalAIModule.h"
#include "Engine/World.h"
#include "Misc/DateTime.h"
#include "Json.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ConvaiChatbotComponent.h"
#include "InterviewEvalServerSubsystem.h"


namespace
{
	int32 CountPatternOccurrences(
		const FString& Text,
		const FString& Pattern
	)
	{
		if (Text.IsEmpty() || Pattern.IsEmpty())
		{
			return 0;
		}

		int32 Count = 0;
		int32 SearchStart = 0;

		while (SearchStart < Text.Len())
		{
			const int32 FoundIndex = Text.Find(
				Pattern,
				ESearchCase::IgnoreCase,
				ESearchDir::FromStart,
				SearchStart
			);

			if (FoundIndex == INDEX_NONE)
			{
				break;
			}

			Count++;
			SearchStart = FoundIndex + Pattern.Len();
		}

		return Count;
	}

	int32 CountGrammarMistakesLightweight(
		const FString& CandidateAnswer
	)
	{
		FString CleanAnswer = CandidateAnswer;
		CleanAnswer.TrimStartAndEndInline();

		if (CleanAnswer.IsEmpty())
		{
			return 0;
		}

		int32 MistakeCount = 0;

		const FString LowerAnswer = CleanAnswer.ToLower();
		const FString PaddedAnswer = TEXT(" ") + LowerAnswer + TEXT(" ");

		// Common contraction mistakes.
		// We do not count missing final punctuation because Convai/STT
		// answers often do not include punctuation.
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" im "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" dont "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" doesnt "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" cant "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" wont "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" didnt "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" isnt "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" arent "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" wasnt "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" werent "));

		// Very common subject-verb grammar mistakes.
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" i is "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" i are "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" he are "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" she are "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" it are "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" we is "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" they is "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" you is "));

		// Common article mistakes.
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" a actor "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" a object "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" a engine "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" a example "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" an class "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" an pointer "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" an function "));
		MistakeCount += CountPatternOccurrences(PaddedAnswer, TEXT(" an variable "));

		return FMath::Clamp(MistakeCount, 0, 10);
	}

	UWorld* ResolveInterviewWorld(UObject* ContextObject)
	{
		if (!ContextObject)
		{
			return nullptr;
		}

		if (UWorld* DirectWorld = ContextObject->GetWorld())
		{
			return DirectWorld;
		}

		return ContextObject->GetTypedOuter<UWorld>();
	}
}

void UInterviewSessionManager::Initialize()
{
	CurrentState = EInterviewState::None;
	CurrentInterviewMode = EInterviewMode::None;
	bInterviewPaused = false;

	ConversationalAI = NewObject<UConversationalAIModule>(this);

	if (ConversationalAI)
	{
		ConversationalAI->InitializeAI();
	}

	QuestionEngine = NewObject<UOpenAIQuestionEngine>(this);
	QuestionPool = NewObject<UQuestionPoolManager>(this);

	EvaluationEngine = NewObject<UEvaluationEngine>(this);
	RepetitionDetector = NewObject<URepetitionDetector>(this);

	if (EvaluationEngine)
	{
		EvaluationEngine->OnEvaluationCompleted.AddDynamic(
			this,
			&UInterviewSessionManager::HandleEvaluationCompleted
		);
	}

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			InterviewEvalServerSubsystem =
				GameInstance->GetSubsystem<
				UInterviewEvalServerSubsystem
				>();

			if (InterviewEvalServerSubsystem)
			{
				InterviewEvalServerSubsystem
					->OnSidecarEvaluationCompleted
					.AddDynamic(
						this,
						&UInterviewSessionManager::HandleSidecarEvaluationCompleted
					);
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Interview Session Manager Initialized"));
}

void UInterviewSessionManager::SetInterviewMode(
	EInterviewMode NewMode
)
{
	// The mode must not change during an active interview.
	if (
		bInterviewTimerActive ||
		(
			CurrentState != EInterviewState::None &&
			CurrentState != EInterviewState::Finished
			)
		)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Cannot change interview mode while an "
				"interview is active | Current State: %s"
			),
			*GetStateAsString()
		);

		return;
	}

	CurrentInterviewMode = NewMode;

	FString ModeName;

	switch (CurrentInterviewMode)
	{
	case EInterviewMode::RealInterview:
		ModeName = TEXT("Real Interview");
		break;

	case EInterviewMode::PracticeInterview:
		ModeName = TEXT("Practice Interview");
		break;

	case EInterviewMode::None:
	default:
		ModeName = TEXT("None");
		break;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Interview mode selected: %s"),
		*ModeName
	);
}

EInterviewMode UInterviewSessionManager::GetInterviewMode() const
{
	return CurrentInterviewMode;
}

void UInterviewSessionManager::PauseInterview()
{
	if (
		bInterviewFinalized ||
		CurrentState == EInterviewState::None ||
		CurrentState == EInterviewState::Closing ||
		CurrentState == EInterviewState::Finished
		)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Pause request ignored | Current State: %s"
			),
			*GetStateAsString()
		);

		return;
	}

	if (bInterviewPaused)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Interview is already paused.")
		);

		return;
	}

	// StopInterviewTimer clears the Unreal timer but keeps
	// RemainingInterviewTimeSeconds unchanged.
	StopInterviewTimer();

	bInterviewPaused = true;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Interview paused | State: %s | "
			"Remaining Time: %.0f seconds"
		),
		*GetStateAsString(),
		RemainingInterviewTimeSeconds
	);
}

void UInterviewSessionManager::ResumeInterview()
{
	if (
		bInterviewFinalized ||
		CurrentState == EInterviewState::None ||
		CurrentState == EInterviewState::Closing ||
		CurrentState == EInterviewState::Finished
		)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Resume request ignored | Current State: %s"
			),
			*GetStateAsString()
		);

		return;
	}

	if (!bInterviewPaused)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Interview is not currently paused.")
		);

		return;
	}

	if (RemainingInterviewTimeSeconds <= 0.0f)
	{
		bInterviewPaused = false;

		CurrentState = EInterviewState::Closing;
		QuestionsAskedInCurrentState = 0;

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Interview cannot resume because no time remains. "
				"Starting Closing state."
			)
		);

		HandleClosingState();
		return;
	}

	UWorld* World = ResolveInterviewWorld(this);

	if (!World)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Cannot resume interview: "
				"no valid UWorld was found."
			)
		);

		return;
	}

	// Resume the existing countdown without resetting its time.
	World->GetTimerManager().SetTimer(
		InterviewTimerHandle,
		this,
		&UInterviewSessionManager::HandleInterviewTimerTick,
		1.0f,
		true
	);

	bInterviewTimerActive = true;
	bInterviewPaused = false;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Interview resumed | State: %s | "
			"Remaining Time: %.0f seconds"
		),
		*GetStateAsString(),
		RemainingInterviewTimeSeconds
	);
}

void UInterviewSessionManager::ResetInterview()
{
	// Stop the active timer and restore the configured full duration.
	ResetInterviewTimer();

	// Return the FSM to its pre-interview state.
	CurrentState = EInterviewState::None;
	CurrentInterviewMode = EInterviewMode::None;

	// Reset interview progression.
	QuestionsAskedInCurrentState = 0;
	TotalQuestionsAsked = 0;
	CurrentDifficulty = TEXT("Easy");

	// Clear current question and answer data.
	CurrentQuestionText.Empty();
	LastCandidateAnswer.Empty();

	// Reset evaluation-related runtime values.
	LastQuickScore = 5.0f;
	LastAnswerInputType = EAnswerInputType::SpeechSTT;

	AnswerHistory.Reset();
	EvaluationHistory.Reset();

	//FinalInterviewReport = FEvaluationResult();

	// Clear the previously supplied job prompt.
	LastJobPrompt.Empty();

	// Restore session flags.
	bInterviewPaused = false;
	bInterviewFinalized = false;
	bAuraaTurnPending = false;

	bClosingRequestedAfterCurrentSpeech = false;
	bClosingSpeechActive = false;

	bPracticeGenerationTransitionActive = false;
	bPracticeQuestionPoolReady = false;
	bPracticeHoldingMessageFinished = false;

	PracticeHoldingFinishedTimeSeconds = 0.0;
	bPracticeUsePatienceAcknowledgement = false;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Interview reset completed | "
			"State: None | Mode: None | "
			"Remaining Time: %.0f seconds"
		),
		RemainingInterviewTimeSeconds
	);
}

bool UInterviewSessionManager::IsInterviewPaused() const
{
	return bInterviewPaused;
}

bool UInterviewSessionManager::IsInterviewFinalized() const
{
	return bInterviewFinalized;
}

bool UInterviewSessionManager::IsInterviewActive() const
{
	return
		!bInterviewFinalized &&
		CurrentState != EInterviewState::None &&
		CurrentState != EInterviewState::Finished;
}

bool UInterviewSessionManager::IsAuraaTurnPending() const
{
	return bAuraaTurnPending;
}

bool UInterviewSessionManager::NotifyAuraaSpeechFinished()
{
	// If the interview has already finished, this speech event
	// cannot belong to an active Closing turn.
	if (bInterviewFinalized)
	{
		return false;
	}

	// =====================================================
	// Actual Closing / outro speech finished
	// =====================================================
	if (bClosingSpeechActive)
	{
		bClosingSpeechActive = false;
		bAuraaTurnPending = false;

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Actual Closing speech finished. "
				"Interview may now be finalized."
			)
		);

		return true;
	}

	// =====================================================
	// Practice holding-message lifecycle
	// =====================================================
	if (bPracticeGenerationTransitionActive)
	{
		bPracticeHoldingMessageFinished = true;

		PracticeHoldingFinishedTimeSeconds =
			GetWorld()
			? GetWorld()->GetTimeSeconds()
			: 0.0;

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Practice holding message finished. "
				"Waiting for personalized QuestionPool if necessary."
			)
		);

		// The interview timer may have expired while the
		// Practice holding message was still being spoken.
		if (
			bClosingRequestedAfterCurrentSpeech &&
			CurrentState == EInterviewState::Closing
			)
		{
			bClosingRequestedAfterCurrentSpeech = false;

			bPracticeGenerationTransitionActive = false;
			bPracticeQuestionPoolReady = false;
			bPracticeHoldingMessageFinished = false;

			bAuraaTurnPending = false;

			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"Pending interviewer turn finished after timer expiry. "
					"Starting actual Closing speech now."
				)
			);

			HandleClosingState();
			return false;
		}

		TryContinuePracticeAfterGenerationWait();
		return false;
	}

	// =====================================================
	// Normal interviewer speech lifecycle
	// =====================================================
	bAuraaTurnPending = false;

	// If the timer expired while Auraa was speaking a normal
	// acknowledgement/question, allow that turn to finish first.
	// Only now should the real Closing/outro begin.
	if (
		bClosingRequestedAfterCurrentSpeech &&
		CurrentState == EInterviewState::Closing
		)
	{
		bClosingRequestedAfterCurrentSpeech = false;

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Pending interviewer turn finished after timer expiry. "
				"Starting actual Closing speech now."
			)
		);

		HandleClosingState();
		return false;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Auraa interviewer turn completed. "
			"Candidate interaction is now allowed."
		)
	);

	return false;
}

void UInterviewSessionManager::TryContinuePracticeAfterGenerationWait()
{
	if (!bPracticeGenerationTransitionActive)
	{
		return;
	}

	// Practice can continue only when BOTH:
	// 1. the personalized question pool is ready, and
	// 2. Auraa has finished the temporary holding message.
	if (
		!bPracticeQuestionPoolReady ||
		!bPracticeHoldingMessageFinished
		)
	{
		return;
	}

	// The interview may have been exited/finalized while
	// question generation was running asynchronously.
	if (
		bInterviewFinalized ||
		CurrentState == EInterviewState::Closing ||
		CurrentState == EInterviewState::Finished
		)
	{
		bPracticeGenerationTransitionActive = false;
		bPracticeQuestionPoolReady = false;
		bPracticeHoldingMessageFinished = false;

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Practice generation transition cancelled because "
				"the interview is closing or finished."
			)
		);

		return;
	}

	const double CurrentTimeSeconds =
		GetWorld()
		? GetWorld()->GetTimeSeconds()
		: 0.0;

	const double SilentWaitSeconds =
		(
			PracticeHoldingFinishedTimeSeconds > 0.0 &&
			CurrentTimeSeconds >= PracticeHoldingFinishedTimeSeconds
			)
		? CurrentTimeSeconds - PracticeHoldingFinishedTimeSeconds
		: 0.0;

	bPracticeUsePatienceAcknowledgement =
		SilentWaitSeconds >= 8.0;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Practice generation silent wait measured | "
			"Wait: %.2f seconds | "
			"Use Patience Acknowledgement: %s"
		),
		SilentWaitSeconds,
		bPracticeUsePatienceAcknowledgement
		? TEXT("true")
		: TEXT("false")
	);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Practice generation transition complete | "
			"QuestionPool Ready: true | "
			"Holding Message Finished: true | "
			"Continuing to first Warmup question."
		)
	);

	// Clear the special transition before asking the real
	// Warmup question so its speech-finished event is treated
	// as a normal interviewer turn.
	bPracticeGenerationTransitionActive = false;
	bPracticeQuestionPoolReady = false;
	bPracticeHoldingMessageFinished = false;

	AskNextQuestionFromPool();
}

bool UInterviewSessionManager::CanStartConfiguredInterview() const
{
	if (CurrentInterviewMode == EInterviewMode::None)
	{
		return false;
	}

	if (
		bInterviewTimerActive ||
		(
			CurrentState != EInterviewState::None &&
			CurrentState != EInterviewState::Finished
			)
		)
	{
		return false;
	}

	if (!QuestionEngine || !QuestionPool)
	{
		return false;
	}

	// Practice Interview can start without Nawshin's focused prompt.
	// The API/practice flow will collect candidate info during the interview.
	if (CurrentInterviewMode == EInterviewMode::PracticeInterview)
	{
		return true;
	}

	// Real Interview must have Nawshin's focused prompt.
	if (CurrentInterviewMode == EInterviewMode::RealInterview)
	{
		return !LastJobPrompt.TrimStartAndEnd().IsEmpty();
	}

	return false;
}

void UInterviewSessionManager::SetJobPrompt(
	const FString& InJobPrompt
)
{
	FString CleanPrompt = InJobPrompt;
	CleanPrompt.TrimStartAndEndInline();

	if (CleanPrompt.IsEmpty())
	{
		LastJobPrompt.Empty();

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"No focused job prompt supplied. "
				"LastJobPrompt has been cleared. "
				"Real Interview cannot start until Nawshin's "
				"focused prompt is provided. "
				"Practice Interview will use its built-in practice prompt."
			)
		);

		return;
	}

	LastJobPrompt = CleanPrompt;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Focused job prompt stored successfully | "
			"Prompt Length: %d characters"
		),
		LastJobPrompt.Len()
	);
}


FString UInterviewSessionManager::GetEffectiveJobPrompt() const
{
	const FString CleanLastJobPrompt =
		LastJobPrompt.TrimStartAndEnd();

	// Practice Interview no longer uses a hardcoded/fake prompt.
	// It starts without a document prompt. The practice API flow
	// should collect candidate education, background, expertise,
	// and related information during the interview.
	if (
		CurrentInterviewMode ==
		EInterviewMode::PracticeInterview
		)
	{
		return FString();
	}

	// Real Interview requires the focused prompt generated by
	// Nawshin's Prompt Engine from PDF, TXT, or JSON documents.
	if (
		CurrentInterviewMode ==
		EInterviewMode::RealInterview
		)
	{
		if (!CleanLastJobPrompt.IsEmpty())
		{
			return CleanLastJobPrompt;
		}

		return FString();
	}

	return FString();
}

void UInterviewSessionManager::SetInterviewDurationMinutes(
	float InDurationMinutes
)
{
	if (!FMath::IsFinite(InDurationMinutes))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Invalid interview duration supplied.")
		);
		return;
	}

	if (bInterviewTimerActive)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Cannot change interview duration while "
				"the interview timer is active."
			)
		);
		return;
	}

	const float RequestedDuration = InDurationMinutes;

	if (FMath::IsNearlyEqual(InDurationMinutes, 5.0f))
	{
		InterviewDurationMinutes = 5.0f;
	}
	else if (FMath::IsNearlyEqual(InDurationMinutes, 10.0f))
	{
		InterviewDurationMinutes = 10.0f;
	}
	else if (FMath::IsNearlyEqual(InDurationMinutes, 15.0f))
	{
		InterviewDurationMinutes = 15.0f;
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Unsupported interview duration: %.2f. "
				"Only 5, 10, or 15 minutes are allowed."
			),
			InDurationMinutes
		);

		return;
	}

	ResetInterviewTimer();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Interview duration configured | "
			"Requested: %.2f minutes | "
			"Applied: %.2f minutes"
		),
		RequestedDuration,
		InterviewDurationMinutes
	);
}

float UInterviewSessionManager::GetInterviewDurationMinutes() const
{
	return InterviewDurationMinutes;
}

float UInterviewSessionManager::GetRemainingInterviewTimeSeconds() const
{
	return FMath::Max(
		0.0f,
		RemainingInterviewTimeSeconds
	);
}

float UInterviewSessionManager::GetRemainingInterviewTimeMinutes() const
{
	return GetRemainingInterviewTimeSeconds() / 60.0f;
}

bool UInterviewSessionManager::IsInterviewTimerActive() const
{
	return bInterviewTimerActive;
}

void UInterviewSessionManager::StartInterviewTimer()
{
	ResetInterviewTimer();

	UWorld* World = ResolveInterviewWorld(this);

	if (!World)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Cannot start interview timer: "
				"no valid UWorld was found."
			)
		);
		return;
	}

	World->GetTimerManager().SetTimer(
		InterviewTimerHandle,
		this,
		&UInterviewSessionManager::HandleInterviewTimerTick,
		1.0f,
		true
	);

	bInterviewTimerActive = true;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Interview timer started | "
			"Duration: %.2f minutes | "
			"Remaining: %.0f seconds"
		),
		InterviewDurationMinutes,
		RemainingInterviewTimeSeconds
	);
}

void UInterviewSessionManager::StopInterviewTimer()
{
	const bool bWasTimerActive = bInterviewTimerActive;

	if (UWorld* World = ResolveInterviewWorld(this))
	{
		World->GetTimerManager().ClearTimer(
			InterviewTimerHandle
		);
	}

	InterviewTimerHandle.Invalidate();
	bInterviewTimerActive = false;

	if (bWasTimerActive)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Interview timer stopped | "
				"Remaining: %.0f seconds"
			),
			RemainingInterviewTimeSeconds
		);
	}
}

void UInterviewSessionManager::ResetInterviewTimer()
{
	StopInterviewTimer();

	RemainingInterviewTimeSeconds =
		InterviewDurationMinutes * 60.0f;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Interview timer reset | "
			"Duration: %.2f minutes | "
			"Remaining: %.0f seconds"
		),
		InterviewDurationMinutes,
		RemainingInterviewTimeSeconds
	);
}

void UInterviewSessionManager::HandleInterviewTimerTick()
{
	if (
		bInterviewFinalized ||
		CurrentState == EInterviewState::Finished
		)
	{
		StopInterviewTimer();
		return;
	}

	if (!bInterviewTimerActive)
	{
		return;
	}

	RemainingInterviewTimeSeconds = FMath::Max(
		0.0f,
		RemainingInterviewTimeSeconds - 1.0f
	);

	if (RemainingInterviewTimeSeconds > 0.0f)
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Interview time expired | Current State: %s | "
			"Starting automatic closing."
		),
		*GetStateAsString()
	);

	StopInterviewTimer();

	if (
		CurrentState != EInterviewState::Closing &&
		CurrentState != EInterviewState::Finished
		)
	{
		CurrentState = EInterviewState::Closing;
		QuestionsAskedInCurrentState = 0;
	}

	if (CurrentState == EInterviewState::Closing)
	{
		// If the actual Closing/outro is already being spoken,
		// do not start another Closing turn.
		if (bClosingSpeechActive)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"Interview timer expired while the actual Closing "
					"speech is already active. Waiting for it to finish."
				)
			);

			return;
		}

		// If Auraa is currently speaking a normal question,
		// acknowledgement, derailment warning, or Practice holding
		// message, let that turn finish naturally first.
		if (bAuraaTurnPending)
		{
			bClosingRequestedAfterCurrentSpeech = true;

			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"Interview timer expired while Auraa is speaking. "
					"Closing is queued until the current speech finishes."
				)
			);

			return;
		}

		// Auraa is idle, so the real Closing/outro can begin now.
		HandleClosingState();
	}
}

void UInterviewSessionManager::StartInterview()
{
	UE_LOG(LogTemp, Warning, TEXT("START INTERVIEW FUNCTION CALLED"));

	// =====================================================
	// Reset all runtime data from any previous interview
	// =====================================================

	QuestionsAskedInCurrentState = 0;
	TotalQuestionsAsked = 0;
	CurrentDifficulty = TEXT("Easy");

	CurrentQuestionText.Empty();
	LastCandidateAnswer.Empty();

	// Reset the acknowledgement score so a new interview cannot
	// inherit the previous interview's final answer score.
	LastQuickScore = 5.0f;

	LastAnswerInputType = EAnswerInputType::SpeechSTT;

	AnswerHistory.Reset();
	EvaluationHistory.Reset();

	FinalInterviewReport = FEvaluationResult();

	bInterviewFinalized = false;
	bInterviewPaused = false;
	bAuraaTurnPending = false;

	bClosingRequestedAfterCurrentSpeech = false;
	bClosingSpeechActive = false;

	bPracticeGenerationTransitionActive = false;
	bPracticeQuestionPoolReady = false;
	bPracticeHoldingMessageFinished = false;

	PracticeHoldingFinishedTimeSeconds = 0.0;
	bPracticeUsePatienceAcknowledgement = false;

	// Start a fresh timer for this interview.
	StartInterviewTimer();

	// =====================================================
	// Begin the new interview
	// =====================================================

	CurrentState = EInterviewState::Greeting;
	PrintCurrentState();

	// The introduction question belongs to the Greeting state.
	// It is separate from the pooled Warmup/Technical/Behavioral
	// question counters.
	CurrentQuestionText =
		TEXT("Please introduce yourself and tell me about your professional background.");

	TotalQuestionsAsked++;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("New interview initialized | State: %s | Difficulty: %s | Total Questions: %d"),
		*GetStateAsString(),
		*CurrentDifficulty,
		TotalQuestionsAsked
	);

	if (ConversationalAI)
	{
		bAuraaTurnPending = true;

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Auraa interviewer turn started. "
				"Waiting for Greeting speech to finish."
			)
		);

		ConversationalAI->SendQuestionToAI(
			TEXT("Start the interview now. Greet the candidate and ask them to introduce themselves.")
		);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Convai greeting and introduction request sent.")
		);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("ConversationalAI is not initialized. Cannot start interview greeting.")
		);
	}
}

bool UInterviewSessionManager::StartConfiguredInterview()
{
	if (CurrentInterviewMode == EInterviewMode::None)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Cannot start configured interview because "
				"no interview mode has been selected."
			)
		);

		return false;
	}

	if (
		bInterviewTimerActive ||
		(
			CurrentState != EInterviewState::None &&
			CurrentState != EInterviewState::Finished
			)
		)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Cannot start configured interview because "
				"an interview is already active | State: %s"
			),
			*GetStateAsString()
		);

		return false;
	}

	if (!QuestionEngine || !QuestionPool)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Cannot start configured interview because "
				"QuestionEngine or QuestionPool is not initialized."
			)
		);

		return false;
	}

	// =====================================================
	// Practice Interview
	// =====================================================
	// Practice Interview does not use Nawshin's focused prompt.
	// It should start without a document prompt. The practice API
	// flow should collect candidate education, background,
	// expertise, and related information.

	if (CurrentInterviewMode == EInterviewMode::PracticeInterview)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Practice Interview setup accepted. "
				"Waiting for the loading transition before "
				"starting the interview timer and Greeting."
			)
		);

		return true;
	}

	// =====================================================
	// Real Interview
	// =====================================================
	// Real Interview must use Nawshin's focused prompt.
	if (CurrentInterviewMode == EInterviewMode::RealInterview)
	{
		const FString EffectiveJobPrompt =
			GetEffectiveJobPrompt();

		if (EffectiveJobPrompt.IsEmpty())
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT(
					"Cannot start Real Interview because "
					"Nawshin's focused job prompt is missing."
				)
			);

			return false;
		}


		ReceiveJobPromptAndGenerateQuestions(
			EffectiveJobPrompt
		);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Configured interview started | "
				"State: %s | Duration: %.2f minutes"
			),
			*GetStateAsString(),
			InterviewDurationMinutes
		);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Real Interview started using Nawshin's "
				"focused job prompt."
			)
		);

		return true;
	}

	return false;
}

void UInterviewSessionManager::AdvanceState()
{
	switch (CurrentState)
	{
	case EInterviewState::Greeting:
		CurrentState = EInterviewState::Warmup;
		break;

	case EInterviewState::Warmup:
		CurrentState = EInterviewState::Technical;
		break;

	case EInterviewState::Technical:
		CurrentState = EInterviewState::Behavioral;
		break;

	case EInterviewState::Behavioral:
		CurrentState = EInterviewState::Closing;
		break;

	case EInterviewState::Closing:
		FinalizeInterview();
		break;

	default:
		break;
	}

	PrintCurrentState();
}

FString UInterviewSessionManager::GetStateAsString() const
{
	switch (CurrentState)
	{
	case EInterviewState::None:
		return TEXT("None");
	case EInterviewState::Greeting:
		return TEXT("Greeting");
	case EInterviewState::Warmup:
		return TEXT("Warm-up");
	case EInterviewState::Technical:
		return TEXT("Technical");
	case EInterviewState::Behavioral:
		return TEXT("Behavioral");
	case EInterviewState::Closing:
		return TEXT("Closing");
	case EInterviewState::Finished:
		return TEXT("Finished");
	default:
		return TEXT("Unknown");
	}
}

void UInterviewSessionManager::PrintCurrentState()
{
	UE_LOG(LogTemp, Warning, TEXT("Current Interview State: %s"),
		*GetStateAsString());
}


bool UInterviewSessionManager::IsRepeatedAnswerLocal(const FString& CandidateAnswer) const
{
	FString NewAnswer = CandidateAnswer.ToLower();
	NewAnswer.TrimStartAndEndInline();

	if (NewAnswer.IsEmpty())
	{
		return false;
	}

	for (const FInterviewAnswerRecord& Record : AnswerHistory)
	{
		FString PreviousAnswer = Record.Answer.ToLower();
		PreviousAnswer.TrimStartAndEndInline();

		if (PreviousAnswer.IsEmpty())
		{
			continue;
		}

		// Exact same answer
		if (PreviousAnswer.Equals(NewAnswer))
		{
			return true;
		}

		// Very simple near-duplicate check
		if (NewAnswer.Len() > 20 && PreviousAnswer.Len() > 20)
		{
			if (PreviousAnswer.Contains(NewAnswer) || NewAnswer.Contains(PreviousAnswer))
			{
				return true;
			}
		}
	}

	return false;
}

FLightweightEvaluationResult UInterviewSessionManager::EvaluateAnswerLightweight(
	const FString& CandidateAnswer,
	const FString& QuestionText
)
{
	FLightweightEvaluationResult Result;

	Result.bShouldContinueInterview = true;
	Result.bIsRepeated = IsRepeatedAnswerLocal(CandidateAnswer);
	Result.GrammarMistakeCount = CountGrammarMistakesLightweight(CandidateAnswer);

	FString CleanAnswer = CandidateAnswer;
	CleanAnswer.TrimStartAndEndInline();

	if (CleanAnswer.IsEmpty())
	{
		Result.QuickScore = 0.0f;
		Result.ClarityScore = 0.0f;
		Result.RelevanceScore = 0.0f;
		Result.Feedback = TEXT("No answer was detected.");
		Result.WarningMessage = TEXT("Empty answer.");
		return Result;
	}

	const int32 AnswerLength = CleanAnswer.Len();

	if (AnswerLength < 20)
	{
		Result.QuickScore = 3.0f;
		Result.ClarityScore = 3.0f;
		Result.RelevanceScore = 3.0f;
		Result.Feedback = TEXT("The answer is too short. More explanation is needed.");
	}
	else if (AnswerLength < 80)
	{
		Result.QuickScore = 5.0f;
		Result.ClarityScore = 5.0f;
		Result.RelevanceScore = 5.0f;
		Result.Feedback = TEXT("The answer is acceptable but needs more detail.");
	}
	else
	{
		Result.QuickScore = 7.0f;
		Result.ClarityScore = 7.0f;
		Result.RelevanceScore = 7.0f;
		Result.Feedback = TEXT("The answer has enough detail to continue the interview.");
	}

	// Grammar mistakes should reduce clarity because unclear grammar
	// makes the candidate's answer harder to understand.
	if (Result.GrammarMistakeCount > 0)
	{
		const float GrammarPenalty =
			static_cast<float>(Result.GrammarMistakeCount) * 0.5f;

		Result.ClarityScore = FMath::Max(
			0.0f,
			Result.ClarityScore - GrammarPenalty
		);

		Result.Feedback += TEXT(" Grammar improvement is needed.");
	}

	if (Result.bIsRepeated)
	{
		Result.QuickScore = FMath::Max(0.0f, Result.QuickScore - 2.0f);
		Result.ClarityScore = FMath::Max(0.0f, Result.ClarityScore - 1.0f);
		Result.RelevanceScore = FMath::Max(0.0f, Result.RelevanceScore - 1.0f);

		Result.WarningMessage = TEXT("Repeated answer detected. Repetition penalty applied.");
		Result.Feedback += TEXT(" Repetition was detected.");
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Lightweight Evaluation | Score: %.2f | Clarity: %.2f | Grammar Mistakes: %d | Repeated: %s | Feedback: %s"),
		Result.QuickScore,
		Result.ClarityScore,
		Result.GrammarMistakeCount,
		Result.bIsRepeated ? TEXT("true") : TEXT("false"),
		*Result.Feedback
	);

	return Result;
}

void UInterviewSessionManager::SaveLightweightAnswerRecord(
	const FString& Question,
	const FString& CandidateAnswer,
	EAnswerInputType InputType,
	const FLightweightEvaluationResult& LightweightResult
)
{
	FInterviewAnswerRecord Record;

	Record.Question = Question;
	Record.Answer = CandidateAnswer;
	Record.AnswerInputType = InputType;
	Record.InterviewState = GetStateAsString();
	Record.Timestamp = FDateTime::Now().ToString();

	Record.Evaluation.Score = LightweightResult.QuickScore;
	Record.Evaluation.Correctness = LightweightResult.QuickScore;
	Record.Evaluation.Clarity = LightweightResult.ClarityScore;
	Record.Evaluation.Relevance = LightweightResult.RelevanceScore;
	Record.Evaluation.Confidence = LightweightResult.QuickScore;
	Record.Evaluation.GrammarMistakeCount = LightweightResult.GrammarMistakeCount;
	Record.Evaluation.Feedback = LightweightResult.Feedback;
	Record.Evaluation.bRepeated = LightweightResult.bIsRepeated;;

	Record.Evaluation.Strengths.Add(TEXT("Candidate provided an answer."));

	if (LightweightResult.GrammarMistakeCount > 0)
	{
		Record.Evaluation.Weaknesses.Add(
			FString::Printf(
				TEXT("Grammar mistakes detected: %d."),
				LightweightResult.GrammarMistakeCount
			)
		);
	}

	if (!LightweightResult.WarningMessage.IsEmpty())
	{
		Record.Evaluation.Weaknesses.Add(LightweightResult.WarningMessage);
	}
	AnswerHistory.Add(Record);
	EvaluationHistory.Add(Record.Evaluation);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Lightweight answer record saved. Total AnswerHistory records: %d"),
		AnswerHistory.Num()
	);
}

void UInterviewSessionManager::AskNextQuestionFromPool()
{
	// =====================================================
	// Time-aware safety checks
	// =====================================================

	if (
		bInterviewFinalized ||
		CurrentState == EInterviewState::Closing ||
		CurrentState == EInterviewState::Finished
		)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Next question request ignored because "
				"the interview is closing or finished."
			)
		);

		return;
	}

	// Preserve the completed question/answer before CurrentQuestionText
	// is replaced with the next interview question.
	const FString PreviousQuestionForAuraa = CurrentQuestionText;
	const FString CandidateAnswerForAuraa = LastCandidateAnswer;

	// Do not begin another question when there is not enough
	// time for Auraa to ask it, the candidate to answer it,
	// and the interview to close cleanly.
	constexpr float MinimumTimeForNextQuestionSeconds = 0.0f;

	if (
		bInterviewTimerActive &&
		GetRemainingInterviewTimeSeconds() <=
		MinimumTimeForNextQuestionSeconds
		)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Not enough time for another question | "
				"Remaining: %.0f seconds | "
				"Starting Closing state."
			),
			GetRemainingInterviewTimeSeconds()
		);

		CurrentState = EInterviewState::Closing;
		QuestionsAskedInCurrentState = 0;

		HandleClosingState();
		return;
	}

	if (!QuestionPool)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("QuestionPool is not initialized. Cannot ask next question.")
		);
		return;
	}

	// =====================================================
	// Convert the FSM state into Nazia's question-stage text
	// =====================================================

	FString TargetStage;

	switch (CurrentState)
	{
	case EInterviewState::Warmup:
		TargetStage = TEXT("Warmup");
		break;

	case EInterviewState::Technical:
		TargetStage = TEXT("Technical");
		break;

	case EInterviewState::Behavioral:
		TargetStage = TEXT("Behavioral");
		break;

	default:
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("AskNextQuestionFromPool called from unsupported state: %s"),
			*GetStateAsString()
		);
		return;
	}

	// Store the current FSM request so regeneration uses the
	// same stage and difficulty.
	const FString RequestedDifficulty = CurrentDifficulty;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("FSM requesting question | Stage: %s | Difficulty: %s"),
		*TargetStage,
		*RequestedDifficulty
	);

	// =====================================================
	// Ask QuestionPool for an FSM-appropriate question
	// =====================================================

	FInterviewQuestion NextQuestion =
		QuestionPool->GetBestQuestionForStageAndDifficulty(
			TargetStage,
			RequestedDifficulty
		);

	// =====================================================
	// Regenerate questions if no usable question is available
	// =====================================================

	if (NextQuestion.QuestionText.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"No unused question available for Stage [%s] "
				"Difficulty [%s]. Attempting regeneration."
			),
			*TargetStage,
			*RequestedDifficulty
		);

		if (!QuestionEngine)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("QuestionEngine is not initialized. Cannot regenerate questions.")
			);
			return;
		}

		const FString EffectiveJobPrompt =
			GetEffectiveJobPrompt();

		if (EffectiveJobPrompt.IsEmpty())
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT(
					"No effective job prompt is available. "
					"Cannot regenerate questions."
				)
			);
			return;
		}

		QuestionEngine->GenerateQuestionsFromPrompt(
			EffectiveJobPrompt,
			FOnQuestionsGenerated::CreateLambda(
				[
					this,
					TargetStage,
					RequestedDifficulty,
					PreviousQuestionForAuraa,
					CandidateAnswerForAuraa
				](
					const TArray<FInterviewQuestion>& GeneratedQuestions
					)
				{
					// The interview may have closed while question generation
					// was running asynchronously.
					if (
						bInterviewFinalized ||
						CurrentState == EInterviewState::Closing ||
						CurrentState == EInterviewState::Finished
						)
					{
						UE_LOG(
							LogTemp,
							Warning,
							TEXT(
								"Regenerated questions ignored because "
								"the interview is closing or finished."
							)
						);
						return;
					}

					constexpr float MinimumTimeForNextQuestionSeconds = 0.0f;

					if (
						bInterviewTimerActive &&
						GetRemainingInterviewTimeSeconds() <=
						MinimumTimeForNextQuestionSeconds
						)
					{
						UE_LOG(
							LogTemp,
							Warning,
							TEXT(
								"Regeneration completed too close to interview end | "
								"Remaining: %.0f seconds | Starting Closing state."
							),
							GetRemainingInterviewTimeSeconds()
						);

						CurrentState = EInterviewState::Closing;
						QuestionsAskedInCurrentState = 0;

						HandleClosingState();
						return;
					}

					if (!QuestionPool)
					{
						UE_LOG(
							LogTemp,
							Error,
							TEXT(
								"QuestionPool is not initialized "
								"inside regeneration callback."
							)
						);
						return;
					}

					UE_LOG(
						LogTemp,
						Warning,
						TEXT("Regenerated questions received: %d"),
						GeneratedQuestions.Num()
					);

					QuestionPool->AddQuestions(GeneratedQuestions);

					FInterviewQuestion RegeneratedQuestion =
						QuestionPool->GetBestQuestionForStageAndDifficulty(
							TargetStage,
							RequestedDifficulty
						);

					if (RegeneratedQuestion.QuestionText.IsEmpty())
					{
						UE_LOG(
							LogTemp,
							Error,
							TEXT(
								"Regeneration completed, but no valid question "
								"was found for Stage [%s] Difficulty [%s]."
							),
							*TargetStage,
							*RequestedDifficulty
						);
						return;
					}

					CurrentQuestionText =
						RegeneratedQuestion.QuestionText;

					QuestionsAskedInCurrentState++;
					TotalQuestionsAsked++;

					UE_LOG(
						LogTemp,
						Warning,
						TEXT(
							"Regenerated question selected | "
							"Stage: %s | Difficulty: %s | "
							"State Questions: %d | Total Questions: %d | "
							"Question: %s"
						),
						*RegeneratedQuestion.Stage,
						*RegeneratedQuestion.Difficulty,
						QuestionsAskedInCurrentState,
						TotalQuestionsAsked,
						*CurrentQuestionText
					);

					if (ConversationalAI)
					{
						bAuraaTurnPending = true;

						UE_LOG(
							LogTemp,
							Warning,
							TEXT(
								"Auraa interviewer turn started. "
								"Waiting for next-question speech to finish."
							)
						);

						ConversationalAI->SendAcknowledgementAndQuestion(
							CurrentQuestionText,
							LastQuickScore,
							PreviousQuestionForAuraa,
							CandidateAnswerForAuraa,
							bPracticeUsePatienceAcknowledgement
						);

						bPracticeUsePatienceAcknowledgement = false;
					}
					else
					{
						UE_LOG(
							LogTemp,
							Error,
							TEXT(
								"ConversationalAI is not initialized. "
								"Cannot send acknowledgement and next question."
							)
						);
					}
				}
			)
		);

		return;
	}

	// =====================================================
	// Save and deliver the selected question
	// =====================================================

	CurrentQuestionText = NextQuestion.QuestionText;

	QuestionsAskedInCurrentState++;
	TotalQuestionsAsked++;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"FSM question selected | "
			"Stage: %s | Difficulty: %s | "
			"State Questions: %d | Total Questions: %d | "
			"Question: %s"
		),
		*NextQuestion.Stage,
		*NextQuestion.Difficulty,
		QuestionsAskedInCurrentState,
		TotalQuestionsAsked,
		*CurrentQuestionText
	);

	if (ConversationalAI)
	{
		bAuraaTurnPending = true;

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Auraa interviewer turn started. "
				"Waiting for next-question speech to finish."
			)
		);

		ConversationalAI->SendAcknowledgementAndQuestion(
			CurrentQuestionText,
			LastQuickScore,
			PreviousQuestionForAuraa,
			CandidateAnswerForAuraa,
			bPracticeUsePatienceAcknowledgement
		);

		bPracticeUsePatienceAcknowledgement = false;
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"ConversationalAI is not initialized. "
				"Cannot send acknowledgement and next question."
			)
		);
	}
}

bool UInterviewSessionManager::ValidateGeneratedQuestionDistribution(
	const TArray<FInterviewQuestion>& GeneratedQuestions,
	FString& OutErrorMessage
) const
{
	if (GeneratedQuestions.Num() != 10)
	{
		OutErrorMessage = FString::Printf(
			TEXT(
				"Invalid question count. Expected 10 generated questions, "
				"but received %d."
			),
			GeneratedQuestions.Num()
		);

		return false;
	}

	int32 WarmupEasyCount = 0;

	int32 TechnicalEasyCount = 0;
	int32 TechnicalMediumCount = 0;
	int32 TechnicalHardCount = 0;

	int32 BehavioralMediumCount = 0;

	int32 InvalidStageCount = 0;
	int32 InvalidDifficultyCount = 0;

	for (const FInterviewQuestion& Question : GeneratedQuestions)
	{
		const FString Stage =
			Question.Stage.TrimStartAndEnd();

		const FString Difficulty =
			Question.Difficulty.TrimStartAndEnd();

		if (
			Stage.Equals(TEXT("Greeting"), ESearchCase::IgnoreCase) ||
			Stage.Equals(TEXT("Closing"), ESearchCase::IgnoreCase) ||
			Stage.Equals(TEXT("Finished"), ESearchCase::IgnoreCase)
			)
		{
			InvalidStageCount++;
			continue;
		}

		if (Stage.Equals(TEXT("Warmup"), ESearchCase::IgnoreCase))
		{
			if (Difficulty.Equals(TEXT("Easy"), ESearchCase::IgnoreCase))
			{
				WarmupEasyCount++;
			}
			else
			{
				InvalidDifficultyCount++;
			}

			continue;
		}

		if (Stage.Equals(TEXT("Technical"), ESearchCase::IgnoreCase))
		{
			if (Difficulty.Equals(TEXT("Easy"), ESearchCase::IgnoreCase))
			{
				TechnicalEasyCount++;
			}
			else if (Difficulty.Equals(TEXT("Medium"), ESearchCase::IgnoreCase))
			{
				TechnicalMediumCount++;
			}
			else if (Difficulty.Equals(TEXT("Hard"), ESearchCase::IgnoreCase))
			{
				TechnicalHardCount++;
			}
			else
			{
				InvalidDifficultyCount++;
			}

			continue;
		}

		if (Stage.Equals(TEXT("Behavioral"), ESearchCase::IgnoreCase))
		{
			if (Difficulty.Equals(TEXT("Medium"), ESearchCase::IgnoreCase))
			{
				BehavioralMediumCount++;
			}
			else
			{
				InvalidDifficultyCount++;
			}

			continue;
		}

		InvalidStageCount++;
	}

	const bool bValidDistribution =
		WarmupEasyCount == 2 &&
		TechnicalEasyCount == 2 &&
		TechnicalMediumCount == 2 &&
		TechnicalHardCount == 1 &&
		BehavioralMediumCount == 3 &&
		InvalidStageCount == 0 &&
		InvalidDifficultyCount == 0;

	if (!bValidDistribution)
	{
		OutErrorMessage = FString::Printf(
			TEXT(
				"Invalid generated question distribution. "
				"Expected: Warmup Easy=2, Technical Easy=2, "
				"Technical Medium=2, Technical Hard=1, "
				"Behavioral Medium=3, Greeting/Closing/Finished=0. "
				"Received: Warmup Easy=%d, Technical Easy=%d, "
				"Technical Medium=%d, Technical Hard=%d, "
				"Behavioral Medium=%d, InvalidStage=%d, "
				"InvalidDifficulty=%d."
			),
			WarmupEasyCount,
			TechnicalEasyCount,
			TechnicalMediumCount,
			TechnicalHardCount,
			BehavioralMediumCount,
			InvalidStageCount,
			InvalidDifficultyCount
		);

		return false;
	}

	OutErrorMessage.Empty();
	return true;
}

void UInterviewSessionManager::RegenerateQuestionsWithStrictDistribution(
	const FString& OriginalGenerationPrompt,
	const FString& FlowLabel,
	bool bStartInterviewAfterSuccess
)
{
	FString CleanOriginalPrompt = OriginalGenerationPrompt;
	CleanOriginalPrompt.TrimStartAndEndInline();

	FString CleanFlowLabel = FlowLabel;
	CleanFlowLabel.TrimStartAndEndInline();

	if (CleanFlowLabel.IsEmpty())
	{
		CleanFlowLabel = TEXT("Question generation");
	}

	if (CleanOriginalPrompt.IsEmpty())
	{
		const FString ErrorMessage = FString::Printf(
			TEXT(
				"%s strict regeneration could not start because "
				"the original generation prompt is empty."
			),
			*CleanFlowLabel
		);

		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s"),
			*ErrorMessage
		);

		OnQuestionGenerationFailedNative.Broadcast(
			ErrorMessage
		);

		return;
	}

	if (!IsValid(QuestionEngine) || !IsValid(QuestionPool))
	{
		const FString ErrorMessage = FString::Printf(
			TEXT(
				"%s strict regeneration could not start because "
				"QuestionEngine or QuestionPool is invalid."
			),
			*CleanFlowLabel
		);

		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s"),
			*ErrorMessage
		);

		OnQuestionGenerationFailedNative.Broadcast(
			ErrorMessage
		);

		return;
	}

	const FString StrictRegenerationPrompt =
		FString::Printf(
			TEXT(
				"%s\n\n"

				"STRICT REGENERATION REQUIREMENT:\n"
				"The previous response was rejected because its question "
				"distribution was invalid.\n"
				"Regenerate the complete question set from the beginning.\n\n"

				"You must return exactly 10 questions in this exact distribution:\n"
				"1. Warmup - Easy\n"
				"2. Warmup - Easy\n"
				"3. Technical - Easy\n"
				"4. Technical - Easy\n"
				"5. Technical - Medium\n"
				"6. Technical - Medium\n"
				"7. Technical - Hard\n"
				"8. Behavioral - Medium\n"
				"9. Behavioral - Medium\n"
				"10. Behavioral - Medium\n\n"

				"Mandatory validation rules:\n"
				"- Total questions must be exactly 10.\n"
				"- Warmup Easy must be exactly 2.\n"
				"- Technical Easy must be exactly 2.\n"
				"- Technical Medium must be exactly 2.\n"
				"- Technical Hard must be exactly 1.\n"
				"- Behavioral Medium must be exactly 3.\n"
				"- Do not generate Greeting questions.\n"
				"- Do not generate Closing questions.\n"
				"- Do not generate Finished-stage questions.\n"
				"- Do not use any other stage or difficulty combination.\n"
				"- Every question must be unique.\n"
				"- Use the exact Stage values: Warmup, Technical, Behavioral.\n"
				"- Use the exact Difficulty values: Easy, Medium, Hard.\n"
				"- Return only valid JSON.\n"
				"- Do not include Markdown, code fences, explanations, "
				"headings, or text outside the JSON.\n\n"

				"Required JSON format:\n"
				"{\"questions\":["
				"{\"QuestionText\":\"...\","
				"\"Stage\":\"Warmup\","
				"\"Difficulty\":\"Easy\","
				"\"Score\":8}"
				"]}"
			),
			*CleanOriginalPrompt
		);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"%s first response had an invalid distribution. "
			"Starting one strict regeneration attempt | "
			"Prompt Length: %d"
		),
		*CleanFlowLabel,
		StrictRegenerationPrompt.Len()
	);

	TWeakObjectPtr<UInterviewSessionManager> WeakThis(this);

	QuestionEngine->GenerateQuestionsFromPrompt(
		StrictRegenerationPrompt,
		FOnQuestionsGenerated::CreateLambda(
			[
				WeakThis,
				CleanFlowLabel,
				bStartInterviewAfterSuccess
			](
				const TArray<FInterviewQuestion>& RegeneratedQuestions
				)
			{
				if (!WeakThis.IsValid())
				{
					UE_LOG(
						LogTemp,
						Warning,
						TEXT(
							"Strict regeneration callback ignored because "
							"InterviewSessionManager is no longer valid."
						)
					);

					return;
				}

				UInterviewSessionManager* StrongThis =
					WeakThis.Get();

				if (!IsValid(StrongThis))
				{
					UE_LOG(
						LogTemp,
						Warning,
						TEXT(
							"Strict regeneration callback ignored because "
							"InterviewSessionManager is invalid."
						)
					);

					return;
				}

				if (
					StrongThis->bInterviewFinalized ||
					StrongThis->CurrentState == EInterviewState::Closing ||
					StrongThis->CurrentState == EInterviewState::Finished
					)
				{
					UE_LOG(
						LogTemp,
						Warning,
						TEXT(
							"%s strict regeneration result ignored because "
							"the interview is closing or finished."
						),
						*CleanFlowLabel
					);

					return;
				}

				if (!IsValid(StrongThis->QuestionPool))
				{
					const FString ErrorMessage =
						FString::Printf(
							TEXT(
								"%s strict regeneration failed because "
								"QuestionPool is no longer valid."
							),
							*CleanFlowLabel
						);

					UE_LOG(
						LogTemp,
						Error,
						TEXT("%s"),
						*ErrorMessage
					);

					StrongThis
						->OnQuestionGenerationFailedNative
						.Broadcast(ErrorMessage);

					return;
				}

				UE_LOG(
					LogTemp,
					Warning,
					TEXT(
						"%s strict regeneration response received: %d questions"
					),
					*CleanFlowLabel,
					RegeneratedQuestions.Num()
				);

				FString DistributionErrorMessage;

				if (
					!StrongThis
					->ValidateGeneratedQuestionDistribution(
						RegeneratedQuestions,
						DistributionErrorMessage
					)
					)
				{
					const FString FinalErrorMessage =
						FString::Printf(
							TEXT(
								"%s question generation failed after "
								"one strict regeneration attempt. %s"
							),
							*CleanFlowLabel,
							*DistributionErrorMessage
						);

					UE_LOG(
						LogTemp,
						Error,
						TEXT("%s"),
						*FinalErrorMessage
					);

					// Practice-only failure safety:
					// If even strict regeneration fails, cancel the temporary
					// holding transition so candidate input cannot remain locked forever.
					if (!bStartInterviewAfterSuccess)
					{
						const bool bHoldingAlreadyFinished =
							StrongThis->bPracticeHoldingMessageFinished;

						StrongThis->bPracticeGenerationTransitionActive = false;
						StrongThis->bPracticeQuestionPoolReady = false;
						StrongThis->bPracticeHoldingMessageFinished = false;

						// If Auraa had already finished the holding message,
						// there will be no later speech-finished event to unlock input.
						if (bHoldingAlreadyFinished)
						{
							StrongThis->bAuraaTurnPending = false;
						}

						UE_LOG(
							LogTemp,
							Warning,
							TEXT(
								"Practice generation transition cancelled because "
								"strict question regeneration failed."
							)
						);
					}

					StrongThis
						->OnQuestionGenerationFailedNative
						.Broadcast(FinalErrorMessage);

					return;
				}

				StrongThis->QuestionPool->AddQuestions(
					RegeneratedQuestions
				);

				UE_LOG(
					LogTemp,
					Warning,
					TEXT(
						"%s strict regeneration succeeded. "
						"Questions safely added to QuestionPool. "
						"Count: %d"
					),
					*CleanFlowLabel,
					RegeneratedQuestions.Num()
				);

				StrongThis->OnQuestionsReadyNative.Broadcast(
					RegeneratedQuestions.Num()
				);

				if (bStartInterviewAfterSuccess)
				{
					// Real Interview:
					// Close loading UI, start timer, and begin Greeting.
					StrongThis->StartInterview();
				}
				else
				{
					// Practice Interview:
					// Greeting is already complete and FSM is in Warmup.
					// The regenerated personalized pool is ready, but the
					// first Warmup question must wait until Auraa has also
					// finished the temporary holding message.
					StrongThis->bPracticeQuestionPoolReady = true;

					UE_LOG(
						LogTemp,
						Warning,
						TEXT(
							"Practice strict-regenerated QuestionPool is ready. "
							"Waiting for Auraa holding message if necessary."
						)
					);

					StrongThis->TryContinuePracticeAfterGenerationWait();
				}
			}
		)
	);
}

void UInterviewSessionManager::ReceiveJobPromptAndGenerateQuestions(
	const FString& JobPrompt
)
{
	FString CleanPrompt = JobPrompt;
	CleanPrompt.TrimStartAndEndInline();

	if (!CleanPrompt.Equals(LastJobPrompt))
	{
		SetJobPrompt(JobPrompt);
	}

	const FString EffectiveJobPrompt =
		GetEffectiveJobPrompt();

	if (EffectiveJobPrompt.IsEmpty())
	{
		const FString ErrorMessage =
			TEXT(
				"Cannot generate Real Interview questions because "
				"Nawshin's focused prompt is empty."
			);

		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s"),
			*ErrorMessage
		);

		OnQuestionGenerationFailedNative.Broadcast(
			ErrorMessage
		);

		return;
	}

	if (!QuestionEngine || !QuestionPool)
	{
		const FString ErrorMessage =
			TEXT(
				"QuestionEngine or QuestionPool is not initialized."
			);

		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s"),
			*ErrorMessage
		);

		OnQuestionGenerationFailedNative.Broadcast(
			ErrorMessage
		);

		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Starting background question generation. "
			"Effective job prompt: %s"
		),
		*EffectiveJobPrompt
	);

	TWeakObjectPtr<UInterviewSessionManager> WeakThis(this);

	QuestionEngine->GenerateQuestionsFromPrompt(
		EffectiveJobPrompt,
		FOnQuestionsGenerated::CreateLambda(
			[WeakThis, EffectiveJobPrompt](
				const TArray<FInterviewQuestion>& GeneratedQuestions
				)
			{
				if (!WeakThis.IsValid())
				{
					UE_LOG(
						LogTemp,
						Warning,
						TEXT(
							"Question generation callback ignored because "
							"InterviewSessionManager is no longer valid."
						)
					);
					return;
				}

				UInterviewSessionManager* StrongThis =
					WeakThis.Get();

				if (!IsValid(StrongThis))
				{
					UE_LOG(
						LogTemp,
						Warning,
						TEXT(
							"Question generation callback ignored because "
							"InterviewSessionManager is invalid."
						)
					);
					return;
				}

				UE_LOG(
					LogTemp,
					Warning,
					TEXT("Generated questions received: %d"),
					GeneratedQuestions.Num()
				);

				if (!IsValid(StrongThis->QuestionPool))
				{
					const FString ErrorMessage =
						TEXT(
							"QuestionPool is not valid inside "
							"Real Interview generation callback."
						);

					UE_LOG(
						LogTemp,
						Error,
						TEXT("%s"),
						*ErrorMessage
					);

					StrongThis->OnQuestionGenerationFailedNative.Broadcast(
						ErrorMessage
					);

					return;
				}


				FString DistributionErrorMessage;

				if (
					!StrongThis->ValidateGeneratedQuestionDistribution(
						GeneratedQuestions,
						DistributionErrorMessage
					)
					)
				{
					UE_LOG(
						LogTemp,
						Warning,
						TEXT(
							"Real Interview first generation response rejected. "
							"%s"
						),
						*DistributionErrorMessage
					);

					StrongThis->RegenerateQuestionsWithStrictDistribution(
						EffectiveJobPrompt,
						TEXT("Real Interview"),
						true
					);

					return;
				}

				StrongThis->QuestionPool->AddQuestions(
					GeneratedQuestions
				);

				UE_LOG(
					LogTemp,
					Warning,
					TEXT(
						"Generated questions safely added to "
						"QuestionPool. Count: %d"
					),
					GeneratedQuestions.Num()
				);

				StrongThis->OnQuestionsReadyNative.Broadcast(
					GeneratedQuestions.Num()
				);

				// The readiness broadcast synchronously closes WBP_Load
				// and opens WBP_Interview before the timer and greeting begin.
				StrongThis->StartInterview();

			}
		)
	);
}

void UInterviewSessionManager::GeneratePracticeQuestionsFromCandidateProfile(
	const FString& CandidateProfile
)
{
	FString CleanCandidateProfile = CandidateProfile;
	CleanCandidateProfile.TrimStartAndEndInline();

	if (CleanCandidateProfile.IsEmpty())
	{
		const FString ErrorMessage =
			TEXT(
				"Cannot generate Practice Interview questions because "
				"candidate profile is empty."
			);

		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s"),
			*ErrorMessage
		);

		OnQuestionGenerationFailedNative.Broadcast(
			ErrorMessage
		);

		return;
	}

	if (!QuestionEngine || !QuestionPool)
	{
		const FString ErrorMessage =
			TEXT(
				"Cannot generate Practice Interview questions because "
				"QuestionEngine or QuestionPool is not initialized."
			);

		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s"),
			*ErrorMessage
		);

		OnQuestionGenerationFailedNative.Broadcast(
			ErrorMessage
		);

		return;
	}

	const FString PracticeGenerationPrompt =
		FString::Printf(
			TEXT(
				"You are an experienced adaptive interview coach conducting a Practice Interview.\n\n"

				"The candidate gave this introduction/background:\n"
				"\"%s\"\n\n"

				"Use this real candidate information to generate exactly 10 unique practice interview questions.\n\n"

				"Important rules:\n"
				"- Do not ask the candidate to introduce themselves again.\n"
				"- Warmup questions should collect or clarify education, background, expertise, interests, projects, and goals.\n"
				"- Technical questions should be adapted to the candidate's stated background, skills, and experience.\n"
				"- Behavioral questions should evaluate communication, teamwork, feedback, challenges, confidence, and learning habits.\n\n"

				"Question distribution:\n"
				"- 2 Warmup questions (Easy)\n"
				"- 5 Technical questions:\n"
				"  - 2 Easy\n"
				"  - 2 Medium\n"
				"  - 1 Hard\n"
				"- 3 Behavioral questions (Medium)\n\n"

				"For each question provide:\n"
				"- QuestionText\n"
				"- Stage (Warmup, Technical, Behavioral)\n"
				"- Difficulty (Easy, Medium, Hard)\n"
				"- Score (0-10)\n\n"

				"Return only valid JSON in this format:\n"
				"{\"questions\":[{\"QuestionText\":\"...\",\"Stage\":\"Warmup\",\"Difficulty\":\"Easy\",\"Score\":8}]}"
			),
			*CleanCandidateProfile
		);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Practice Interview generating questions from "
			"candidate profile | Prompt Length: %d characters"
		),
		PracticeGenerationPrompt.Len()
	);

	TWeakObjectPtr<UInterviewSessionManager> WeakThis(this);

	QuestionEngine->GenerateQuestionsFromPrompt(
		PracticeGenerationPrompt,
		FOnQuestionsGenerated::CreateLambda(
			[WeakThis, PracticeGenerationPrompt](
				const TArray<FInterviewQuestion>& GeneratedQuestions
				)
			{
				if (!WeakThis.IsValid())
				{
					UE_LOG(
						LogTemp,
						Warning,
						TEXT(
							"Practice question generation callback ignored "
							"because InterviewSessionManager is no longer valid."
						)
					);
					return;
				}

				UInterviewSessionManager* StrongThis =
					WeakThis.Get();

				if (!IsValid(StrongThis))
				{
					UE_LOG(
						LogTemp,
						Warning,
						TEXT(
							"Practice question generation callback ignored "
							"because InterviewSessionManager is invalid."
						)
					);
					return;
				}

				if (
					StrongThis->bInterviewFinalized ||
					StrongThis->CurrentState == EInterviewState::Closing ||
					StrongThis->CurrentState == EInterviewState::Finished
					)
				{
					UE_LOG(
						LogTemp,
						Warning,
						TEXT(
							"Practice generated questions ignored because "
							"the interview is closing or finished."
						)
					);
					return;
				}

				if (!IsValid(StrongThis->QuestionPool))
				{
					const FString ErrorMessage =
						TEXT(
							"QuestionPool is not valid inside "
							"Practice Interview generation callback."
						);

					UE_LOG(
						LogTemp,
						Error,
						TEXT("%s"),
						*ErrorMessage
					);

					StrongThis->OnQuestionGenerationFailedNative.Broadcast(
						ErrorMessage
					);

					return;
				}

				FString DistributionErrorMessage;

				if (
					!StrongThis->ValidateGeneratedQuestionDistribution(
						GeneratedQuestions,
						DistributionErrorMessage
					)
				)
				{
					UE_LOG(
						LogTemp,
						Warning,
						TEXT(
							"Practice Interview first generation response rejected. "
							"%s"
						),
						*DistributionErrorMessage
					);

					StrongThis->RegenerateQuestionsWithStrictDistribution(
						PracticeGenerationPrompt,
						TEXT("Practice Interview"),
						false
					);

					return;
				}

				UE_LOG(
					LogTemp,
					Warning,
					TEXT(
						"Practice Interview generated questions received: %d"
					),
					GeneratedQuestions.Num()
				);

				StrongThis->QuestionPool->AddQuestions(
					GeneratedQuestions
				);

				UE_LOG(
					LogTemp,
					Warning,
					TEXT(
						"Practice Interview questions safely added "
						"to QuestionPool. Count: %d"
					),
					GeneratedQuestions.Num()
				);

				StrongThis->OnQuestionsReadyNative.Broadcast(
					GeneratedQuestions.Num()
				);

				// The personalized Practice pool is ready.
				// Do not ask the Warmup question yet unless Auraa has also
				// finished the temporary holding message.
				StrongThis->bPracticeQuestionPoolReady = true;

				UE_LOG(
					LogTemp,
					Warning,
					TEXT(
						"Practice QuestionPool is ready. "
						"Waiting for Auraa holding message if necessary."
					)
				);

				StrongThis->TryContinuePracticeAfterGenerationWait();
			}
		)
	);
}

void UInterviewSessionManager::SubmitCandidateAnswer(
	const FString& Answer,
	EAnswerInputType InputType
)
{
	if (bInterviewFinalized || CurrentState == EInterviewState::Finished)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Interview already finalized/finished. Candidate answer ignored.")
		);
		return;
	}

	if (bInterviewPaused)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Candidate answer ignored because "
				"the interview is currently paused."
			)
		);

		return;
	}

	FString CleanAnswer = Answer;
	CleanAnswer.TrimStartAndEndInline();

	if (CleanAnswer.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Empty candidate answer ignored. Waiting for real answer.")
		);
		return;
	}

	LastCandidateAnswer = CleanAnswer;
	LastAnswerInputType = InputType;

	if (CurrentQuestionText.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("No current question found. Cannot process answer.")
		);
		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Submitting answer for lightweight evaluation: %s"),
		*CleanAnswer
	);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Current question: %s"),
		*CurrentQuestionText
	);

	FLightweightEvaluationResult LightweightResult =
		EvaluateAnswerLightweight(
			CleanAnswer,
			CurrentQuestionText
		);

	// Store the latest score before advancing the FSM.
	// The next question will use this value to choose
	// an appropriate acknowledgement.
	LastQuickScore = FMath::Clamp(
		LightweightResult.QuickScore,
		0.0f,
		10.0f
	);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Latest QuickScore stored for next acknowledgement: %.2f"
		),
		LastQuickScore
	);

	SaveLightweightAnswerRecord(
		CurrentQuestionText,
		CleanAnswer,
		InputType,
		LightweightResult
	);

	const int32 SavedAnswerRecordIndex =
		AnswerHistory.Num() - 1;

	if (
		InterviewEvalServerSubsystem &&
		InterviewEvalServerSubsystem
		->IsEvaluationServerHealthy() &&
		AnswerHistory.IsValidIndex(
			SavedAnswerRecordIndex
		)
		)
	{
		InterviewEvalServerSubsystem
			->RequestDetailedEvaluation(
				CurrentQuestionText,
				CleanAnswer,
				SavedAnswerRecordIndex
			);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Detailed sidecar evaluation requested "
				"for answer record %d."
			),
			SavedAnswerRecordIndex
		);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Detailed sidecar evaluation skipped. "
				"Lightweight evaluation remains active."
			)
		);
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Answer processed using lightweight evaluation. "
			"Continuing interview."
		)
	);

	// =====================================================
// Practice Interview first-answer question generation
// =====================================================
// In Practice Interview, the Greeting answer becomes the
// candidate profile. The API then generates practice
// questions from the candidate's real background.
	if (
		CurrentInterviewMode == EInterviewMode::PracticeInterview &&
		CurrentState == EInterviewState::Greeting
		)
	{
		AdvanceInterviewFSM();

		if (
			bInterviewFinalized ||
			CurrentState == EInterviewState::Closing ||
			CurrentState == EInterviewState::Finished
			)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"Practice question generation skipped because "
					"the interview is closing or finished."
				)
			);
			return;
		}

		// Begin the special Practice transition.
		// Question generation and Auraa's holding message now run
		// independently, and Warmup will start only when both finish.
		bPracticeGenerationTransitionActive = true;
		bPracticeQuestionPoolReady = false;
		bPracticeHoldingMessageFinished = false;

		if (ConversationalAI)
		{
			bAuraaTurnPending = true;

			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"Practice generation transition started. "
					"Auraa is delivering the holding message."
				)
			);

			ConversationalAI->SendQuestionToAI(
				TEXT(
					"Say exactly this one short transition message to the candidate: "
					"\"Thank you. I'm preparing a few questions based on your background. "
					"This will take just a moment.\" "
					"Do not evaluate the candidate's answer. "
					"Do not ask any interview question. "
					"Do not add anything else."
				)
			);
		}
		else
		{
			// If Convai is unavailable, do not permanently block
			// Practice progression.
			bPracticeHoldingMessageFinished = true;

			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"ConversationalAI unavailable for Practice holding message. "
					"Continuing once QuestionPool becomes ready."
				)
			);
		}

		GeneratePracticeQuestionsFromCandidateProfile(
			CleanAnswer
		);

		return;
	}

	if (CurrentState != EInterviewState::Finished)
	{
		// Advance the FSM first so the next action uses
		// the correct interview state.
		AdvanceInterviewFSM();

		// Closing may finalize the interview immediately.
		// Do not request another question afterward.
		if (
			bInterviewFinalized ||
			CurrentState == EInterviewState::Closing ||
			CurrentState == EInterviewState::Finished
			)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"No next question requested because "
					"the interview is closing or finished."
				)
			);
			return;
		}

		AskNextQuestionFromPool();
	}
}

void UInterviewSessionManager::HandleEvaluationCompleted(
	FEvaluationResult Result
)
{
	// This callback belongs to the older asynchronous/full-evaluation path.
	//
	// The Alpha interview flow now uses SubmitCandidateAnswer() as the
	// single authoritative path for:
	// - lightweight evaluation,
	// - saving the answer record,
	// - advancing the FSM,
	// - requesting the next question.
	//
	// Therefore, this legacy callback must not save another record or
	// advance the interview, because that could cause duplicate history
	// entries and double FSM progression.

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Legacy evaluation callback received and ignored | "
			"Score: %.2f | Current State: %s | "
			"SubmitCandidateAnswer remains the authoritative path."
		),
		Result.Score,
		*GetStateAsString()
	);
}

void UInterviewSessionManager::HandleSidecarEvaluationCompleted(
	FEvaluationResult Result,
	bool bRequestSucceeded,
	FString Source,
	FString ErrorMessage,
	int32 AnswerRecordIndex
)
{
	if (!bRequestSucceeded)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Sidecar evaluation failed for answer record %d. "
				"Keeping lightweight evaluation. Error: %s"
			),
			AnswerRecordIndex,
			*ErrorMessage
		);

		return;
	}

	if (!AnswerHistory.IsValidIndex(AnswerRecordIndex))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Sidecar evaluation returned an invalid "
				"AnswerHistory index: %d"
			),
			AnswerRecordIndex
		);

		return;
	}

	AnswerHistory[AnswerRecordIndex].Evaluation = Result;

	if (EvaluationHistory.IsValidIndex(AnswerRecordIndex))
	{
		EvaluationHistory[AnswerRecordIndex] = Result;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Answer record %d updated with detailed evaluation. "
			"Source: %s | Score: %.2f"
		),
		AnswerRecordIndex,
		*Source,
		Result.Score
	);
}

void UInterviewSessionManager::SaveAnswerHistoryToJson()
{
	TArray<TSharedPtr<FJsonValue>> RecordsArray;

	for (const FInterviewAnswerRecord& Record : AnswerHistory)
	{
		TSharedPtr<FJsonObject> RecordObject = MakeShareable(new FJsonObject);

		RecordObject->SetStringField(TEXT("question"), Record.Question);
		RecordObject->SetStringField(TEXT("answer"), Record.Answer);

		RecordObject->SetStringField(TEXT("interview_state"), Record.InterviewState);
		RecordObject->SetStringField(TEXT("timestamp"), Record.Timestamp);

		FString AnswerTypeString;

		switch (Record.AnswerInputType)
		{
		case EAnswerInputType::SpeechSTT:
			AnswerTypeString = TEXT("speech_stt");
			break;

		case EAnswerInputType::WrittenText:
			AnswerTypeString = TEXT("written_text");
			break;

		case EAnswerInputType::WrittenCode:
			AnswerTypeString = TEXT("written_code");
			break;

		default:
			AnswerTypeString = TEXT("unknown");
			break;
		}

		RecordObject->SetStringField(TEXT("answer_type"), AnswerTypeString);

		TSharedPtr<FJsonObject> EvaluationObject = MakeShareable(new FJsonObject);
		EvaluationObject->SetNumberField(TEXT("score"), Record.Evaluation.Score);
		EvaluationObject->SetNumberField(TEXT("correctness"), Record.Evaluation.Correctness);
		EvaluationObject->SetNumberField(TEXT("clarity"), Record.Evaluation.Clarity);
		EvaluationObject->SetNumberField(TEXT("relevance"), Record.Evaluation.Relevance);
		EvaluationObject->SetNumberField(TEXT("confidence"), Record.Evaluation.Confidence);
		EvaluationObject->SetStringField(TEXT("feedback"), Record.Evaluation.Feedback);
		EvaluationObject->SetBoolField(TEXT("repeated"), Record.Evaluation.bRepeated);

		RecordObject->SetObjectField(TEXT("evaluation"), EvaluationObject);

		RecordsArray.Add(MakeShareable(new FJsonValueObject(RecordObject)));
	}

	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
	RootObject->SetArrayField(TEXT("answers"), RecordsArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);

	if (FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
	{
		FString FilePath = FPaths::ProjectSavedDir() / TEXT("InterviewAnswerHistory.json");

		if (FFileHelper::SaveStringToFile(OutputString, *FilePath))
		{
			UE_LOG(LogTemp, Warning, TEXT("Answer history saved to: %s"), *FilePath);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to save answer history JSON"));
		}
	}
}

FEvaluationResult UInterviewSessionManager::GenerateFinalInterviewReport()
{
	FEvaluationResult FinalReport;

	if (EvaluationHistory.Num() == 0)
	{
		FinalReport.Feedback = TEXT("No evaluated answers found.");
		return FinalReport;
	}

	for (const FEvaluationResult& Result : EvaluationHistory)
	{
		FinalReport.Score += Result.Score;
		FinalReport.Correctness += Result.Correctness;
		FinalReport.Clarity += Result.Clarity;
		FinalReport.Relevance += Result.Relevance;
		FinalReport.Confidence += Result.Confidence;

		FinalReport.Strengths.Append(Result.Strengths);
		FinalReport.Weaknesses.Append(Result.Weaknesses);
	}

	const float Count = EvaluationHistory.Num();

	FinalReport.Score /= Count;
	FinalReport.Correctness /= Count;
	FinalReport.Clarity /= Count;
	FinalReport.Relevance /= Count;
	FinalReport.Confidence /= Count;

	FinalReport.Feedback = TEXT("Final interview evaluation completed.");

	return FinalReport;
}

FEvaluationResult UInterviewSessionManager::GetFinalInterviewReport() const
{
	return FinalInterviewReport;
}

int32 UInterviewSessionManager::GetAnsweredQuestionCount() const
{
	return AnswerHistory.Num();
}

int32 UInterviewSessionManager::GetAskedQuestionCount() const
{
	return TotalQuestionsAsked;
}

int32 UInterviewSessionManager::GetExpectedAnswerCount() const
{
	// 1 Greeting answer + all pooled interview questions.
	return
		1 +
		WarmupQuestionLimit +
		TechnicalQuestionLimit +
		BehavioralQuestionLimit;
}

float UInterviewSessionManager::GetInterviewProgressPercent() const
{
	const int32 ExpectedAnswerCount =
		GetExpectedAnswerCount();

	if (ExpectedAnswerCount <= 0)
	{
		return 0.0f;
	}

	const float Progress =
		static_cast<float>(GetAnsweredQuestionCount()) /
		static_cast<float>(ExpectedAnswerCount);

	return FMath::Clamp(
		Progress,
		0.0f,
		1.0f
	);
}


FString UInterviewSessionManager::GetCurrentQuestionText() const
{
	return CurrentQuestionText;
}

FString UInterviewSessionManager::GetCurrentDifficulty() const
{
	return CurrentDifficulty;
}

void UInterviewSessionManager::FinalizeInterview()
{
	if (bInterviewFinalized)
	{
		UE_LOG(LogTemp, Warning, TEXT("Interview already finalized."));
		return;
	}

	

	// Prevent the Closing fallback from firing after finalization.
	UWorld* World = ResolveInterviewWorld(this);

	if (World)
	{
		World->GetTimerManager().ClearTimer(
			ClosingFallbackTimerHandle
		);
	}

	ClosingFallbackTimerHandle.Invalidate();

	bInterviewPaused = false;
	bInterviewFinalized = true;

	// Ensure the interview timer cannot continue after finalization.
	StopInterviewTimer();

	CurrentState = EInterviewState::Finished;

	UE_LOG(LogTemp, Warning, TEXT("Interview finalized. Generating reports and exports..."));

	FinalInterviewReport = GenerateFinalInterviewReport();

	UE_LOG(LogTemp, Warning, TEXT("===== FINAL INTERVIEW REPORT ====="));
	UE_LOG(LogTemp, Warning, TEXT("Final Score: %.2f"), FinalInterviewReport.Score);
	UE_LOG(LogTemp, Warning, TEXT("Final Correctness: %.2f"), FinalInterviewReport.Correctness);
	UE_LOG(LogTemp, Warning, TEXT("Final Clarity: %.2f"), FinalInterviewReport.Clarity);
	UE_LOG(LogTemp, Warning, TEXT("Final Relevance: %.2f"), FinalInterviewReport.Relevance);
	UE_LOG(LogTemp, Warning, TEXT("Final Confidence: %.2f"), FinalInterviewReport.Confidence);
	UE_LOG(LogTemp, Warning, TEXT("Final Feedback: %s"), *FinalInterviewReport.Feedback);

	ExportAnswerHistoryJSON(TEXT("InterviewAnswerHistory.json"));
	ExportTranscriptToFile(TEXT("InterviewTranscript.txt"));
	
	OnInterviewFinalizedNative.Broadcast();

	UE_LOG(LogTemp, Warning, TEXT("FinalizeInterview completed."));
}

FString UInterviewSessionManager::BuildDialogueTranscript() const
{
	FString Transcript;
	FString InterviewModeString;

	switch (CurrentInterviewMode)
	{
	case EInterviewMode::RealInterview:
		InterviewModeString = TEXT("Real Interview");
		break;

	case EInterviewMode::PracticeInterview:
		InterviewModeString = TEXT("Practice Interview");
		break;

	case EInterviewMode::None:
	default:
		InterviewModeString = TEXT("None");
		break;
	}

	Transcript += TEXT("Interview Dialogue Transcript\n");
	Transcript += TEXT("============================\n");
	Transcript += FString::Printf(
		TEXT("Interview Mode: %s\n\n"),
		*InterviewModeString
	);

	for (int32 Index = 0; Index < AnswerHistory.Num(); Index++)
	{
		const FInterviewAnswerRecord& Record = AnswerHistory[Index];

		Transcript += FString::Printf(TEXT("Question %d\n"), Index + 1);
		Transcript += TEXT("Interviewer:\n");
		Transcript += Record.Question + TEXT("\n\n");

		Transcript += TEXT("Candidate:\n");
		Transcript += Record.Answer + TEXT("\n\n");

		Transcript += TEXT("Evaluation:\n");
		Transcript += FString::Printf(TEXT("Score: %.2f/10\n"), Record.Evaluation.Score);
		Transcript += FString::Printf(TEXT("Correctness: %.2f/10\n"), Record.Evaluation.Correctness);
		Transcript += FString::Printf(TEXT("Clarity: %.2f/10\n"), Record.Evaluation.Clarity);
		Transcript += FString::Printf(TEXT("Relevance: %.2f/10\n"), Record.Evaluation.Relevance);
		Transcript += FString::Printf(TEXT("Confidence: %.2f/10\n"), Record.Evaluation.Confidence);
		Transcript += FString::Printf(TEXT("Grammar Mistakes: %d\n"), Record.Evaluation.GrammarMistakeCount);
		Transcript += TEXT("Feedback: ") + Record.Evaluation.Feedback + TEXT("\n");

		if (Record.Evaluation.bRepeated)
		{
			Transcript += TEXT("Warning: Repeated answer detected.\n");
		}

		Transcript += TEXT("\n----------------------------------------\n\n");
	}

	return Transcript;
}

FString UInterviewSessionManager::BuildFinalReportText() const
{
	FString Report;

	Report += TEXT("Final Interview Report\n");
	Report += TEXT("======================\n\n");

	Report += FString::Printf(TEXT("Total Questions Answered: %d\n"), AnswerHistory.Num());

	float TotalScore = 0.0f;
	float TotalCorrectness = 0.0f;
	float TotalClarity = 0.0f;
	float TotalRelevance = 0.0f;
	float TotalConfidence = 0.0f;
	int32 TotalGrammarMistakes = 0;
	int32 RepeatedCount = 0;

	for (const FInterviewAnswerRecord& Record : AnswerHistory)
	{
		TotalScore += Record.Evaluation.Score;
		TotalCorrectness += Record.Evaluation.Correctness;
		TotalClarity += Record.Evaluation.Clarity;
		TotalRelevance += Record.Evaluation.Relevance;
		TotalConfidence += Record.Evaluation.Confidence;
		TotalGrammarMistakes += Record.Evaluation.GrammarMistakeCount;

		if (Record.Evaluation.bRepeated)
		{
			RepeatedCount++;
		}
	}

	const float Count = AnswerHistory.Num() > 0 ? static_cast<float>(AnswerHistory.Num()) : 1.0f;

	const float AverageScore = TotalScore / Count;
	const float AverageCorrectness = TotalCorrectness / Count;
	const float AverageClarity = TotalClarity / Count;
	const float AverageRelevance = TotalRelevance / Count;
	const float AverageConfidence = TotalConfidence / Count;

	Report += FString::Printf(TEXT("Average Score: %.2f/10\n"), AverageScore);
	Report += FString::Printf(TEXT("Average Correctness: %.2f/10\n"), AverageCorrectness);
	Report += FString::Printf(TEXT("Average Clarity: %.2f/10\n"), AverageClarity);
	Report += FString::Printf(TEXT("Average Relevance: %.2f/10\n"), AverageRelevance);
	Report += FString::Printf(TEXT("Average Confidence: %.2f/10\n"), AverageConfidence);
	Report += FString::Printf(TEXT("Total Grammar Mistakes: %d\n"), TotalGrammarMistakes);
	Report += FString::Printf(TEXT("Repeated Answers: %d\n\n"), RepeatedCount);

	if (AverageScore >= 8.0f)
	{
		Report += TEXT("Overall Performance: Strong\n\n");
	}
	else if (AverageScore >= 5.0f)
	{
		Report += TEXT("Overall Performance: Moderate\n\n");
	}
	else
	{
		Report += TEXT("Overall Performance: Needs Improvement\n\n");
	}

	Report += TEXT("Detailed Question-wise Evaluation\n");
	Report += TEXT("---------------------------------\n\n");

	for (int32 Index = 0; Index < AnswerHistory.Num(); Index++)
	{
		const FInterviewAnswerRecord& Record = AnswerHistory[Index];

		Report += FString::Printf(TEXT("Question %d:\n"), Index + 1);
		Report += Record.Question + TEXT("\n\n");

		Report += TEXT("Candidate Answer:\n");
		Report += Record.Answer + TEXT("\n\n");

		Report += FString::Printf(TEXT("Score: %.2f/10\n"), Record.Evaluation.Score);
		Report += FString::Printf(TEXT("Grammar Mistakes: %d\n"), Record.Evaluation.GrammarMistakeCount);
		Report += TEXT("Feedback: ") + Record.Evaluation.Feedback + TEXT("\n\n");
	}

	return Report;
}

bool UInterviewSessionManager::ExportTranscriptToFile(const FString& FileName)
{
	const FString FolderPath = FPaths::ProjectSavedDir() / TEXT("InterviewReports");
	IFileManager::Get().MakeDirectory(*FolderPath, true);

	const FString FullPath = FolderPath / FileName;

	FString OutputText;
	OutputText += BuildFinalReportText();
	OutputText += TEXT("\n\n");
	OutputText += BuildDialogueTranscript();

	const bool bSaved = FFileHelper::SaveStringToFile(OutputText, *FullPath);

	if (bSaved)
	{
		UE_LOG(LogTemp, Warning, TEXT("Transcript/report exported to: %s"), *FullPath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to export transcript/report."));
	}

	return bSaved;
}

bool UInterviewSessionManager::ExportAnswerHistoryJSON(const FString& FileName)
{
	const FString FolderPath = FPaths::ProjectSavedDir() / TEXT("InterviewReports");
	IFileManager::Get().MakeDirectory(*FolderPath, true);

	TArray<TSharedPtr<FJsonValue>> RecordsArray;

	for (const FInterviewAnswerRecord& Record : AnswerHistory)
	{
		TSharedPtr<FJsonObject> RecordObject = MakeShareable(new FJsonObject);

		RecordObject->SetStringField(TEXT("question"), Record.Question);
		RecordObject->SetStringField(TEXT("answer"), Record.Answer);
		RecordObject->SetStringField(TEXT("interview_state"), Record.InterviewState);
		RecordObject->SetStringField(TEXT("timestamp"), Record.Timestamp);

		FString AnswerTypeString;

		switch (Record.AnswerInputType)
		{
		case EAnswerInputType::SpeechSTT:
			AnswerTypeString = TEXT("speech_stt");
			break;

		case EAnswerInputType::WrittenText:
			AnswerTypeString = TEXT("written_text");
			break;

		case EAnswerInputType::WrittenCode:
			AnswerTypeString = TEXT("written_code");
			break;

		default:
			AnswerTypeString = TEXT("unknown");
			break;
		}

		RecordObject->SetStringField(TEXT("answer_type"), AnswerTypeString);

		TSharedPtr<FJsonObject> EvaluationObject = MakeShareable(new FJsonObject);

		EvaluationObject->SetNumberField(TEXT("score"), Record.Evaluation.Score);
		EvaluationObject->SetNumberField(TEXT("correctness"), Record.Evaluation.Correctness);
		EvaluationObject->SetNumberField(TEXT("clarity"), Record.Evaluation.Clarity);
		EvaluationObject->SetNumberField(TEXT("relevance"), Record.Evaluation.Relevance);
		EvaluationObject->SetNumberField(TEXT("confidence"), Record.Evaluation.Confidence);
		EvaluationObject->SetNumberField(TEXT("grammar_mistakes"), Record.Evaluation.GrammarMistakeCount);
		EvaluationObject->SetStringField(TEXT("feedback"), Record.Evaluation.Feedback);
		EvaluationObject->SetBoolField(TEXT("repeated"), Record.Evaluation.bRepeated);

		TArray<TSharedPtr<FJsonValue>> StrengthsArray;
		for (const FString& Strength : Record.Evaluation.Strengths)
		{
			StrengthsArray.Add(MakeShareable(new FJsonValueString(Strength)));
		}
		EvaluationObject->SetArrayField(TEXT("strengths"), StrengthsArray);

		TArray<TSharedPtr<FJsonValue>> WeaknessesArray;
		for (const FString& Weakness : Record.Evaluation.Weaknesses)
		{
			WeaknessesArray.Add(MakeShareable(new FJsonValueString(Weakness)));
		}
		EvaluationObject->SetArrayField(TEXT("weaknesses"), WeaknessesArray);

		RecordObject->SetObjectField(TEXT("evaluation"), EvaluationObject);

		RecordsArray.Add(MakeShareable(new FJsonValueObject(RecordObject)));
	}

	FString InterviewModeString;

	switch (CurrentInterviewMode)
	{
	case EInterviewMode::RealInterview:
		InterviewModeString = TEXT("Real Interview");
		break;

	case EInterviewMode::PracticeInterview:
		InterviewModeString = TEXT("Practice Interview");
		break;

	case EInterviewMode::None:
	default:
		InterviewModeString = TEXT("None");
		break;
	}

	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);

	RootObject->SetStringField(
		TEXT("interview_mode"),
		InterviewModeString
	);

	RootObject->SetStringField(
		TEXT("interview_date"),
		FDateTime::Now().ToString()
	);

	RootObject->SetNumberField(TEXT("total_answers"), AnswerHistory.Num());
	RootObject->SetArrayField(TEXT("answers"), RecordsArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);

	if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to serialize AnswerHistory JSON."));
		return false;
	}

	const FString FullPath = FolderPath / FileName;
	const bool bSaved = FFileHelper::SaveStringToFile(OutputString, *FullPath);

	if (bSaved)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnswerHistory JSON exported to: %s"), *FullPath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to save AnswerHistory JSON."));
	}

	return bSaved;
}

void UInterviewSessionManager::SetConvaiChatbot(UConvaiChatbotComponent* InChatbotComponent)
{
	if (ConversationalAI)
	{
		ConversationalAI->SetConvaiChatbot(InChatbotComponent);

		UE_LOG(LogTemp, Warning, TEXT("Convai Chatbot connected to Interview Session."));
	}
}

void UInterviewSessionManager::AdvanceInterviewFSM()
{
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("FSM Tick | State: %s | Questions in State: %d | Total Questions: %d"),
		*GetStateAsString(),
		QuestionsAskedInCurrentState,
		TotalQuestionsAsked
	);

	switch (CurrentState)
	{
	case EInterviewState::Greeting:
		HandleGreetingState();
		break;

	case EInterviewState::Warmup:
		HandleWarmupState();
		break;

	case EInterviewState::Technical:
		HandleTechnicalState();
		break;

	case EInterviewState::Behavioral:
		HandleBehavioralState();
		break;

	case EInterviewState::Closing:
		HandleClosingState();
		break;

	case EInterviewState::Finished:
		HandleFinishedState();
		break;

	default:
		break;
	}
}

void UInterviewSessionManager::HandleGreetingState()
{
	UE_LOG(LogTemp, Warning, TEXT("FSM: Greeting State"));

	CurrentState = EInterviewState::Warmup;

	QuestionsAskedInCurrentState = 0;

	CurrentDifficulty = TEXT("Easy");

	UE_LOG(LogTemp, Warning, TEXT("FSM initialized. Entering Warmup state."));
}

void UInterviewSessionManager::HandleWarmupState()
{
	CurrentDifficulty = TEXT("Easy");

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("FSM: Warmup | Difficulty: %s | %d / %d"),
		*CurrentDifficulty,
		QuestionsAskedInCurrentState,
		WarmupQuestionLimit
	);

	if (QuestionsAskedInCurrentState >= WarmupQuestionLimit)
	{
		UE_LOG(LogTemp, Warning, TEXT("Warmup completed. Advancing to Technical."));

		QuestionsAskedInCurrentState = 0;

		AdvanceState();
	}
}

void UInterviewSessionManager::HandleTechnicalState()
{
	// Alpha version:
	// Easy -> Medium -> Hard

	if (QuestionsAskedInCurrentState < 2)
	{
		CurrentDifficulty = TEXT("Easy");
	}
	else if (QuestionsAskedInCurrentState < 4)
	{
		CurrentDifficulty = TEXT("Medium");
	}
	else
	{
		CurrentDifficulty = TEXT("Hard");
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("FSM: Technical | Difficulty: %s | %d / %d"),
		*CurrentDifficulty,
		QuestionsAskedInCurrentState,
		TechnicalQuestionLimit
	);

	if (QuestionsAskedInCurrentState >= TechnicalQuestionLimit)
	{
		UE_LOG(LogTemp, Warning, TEXT("Technical stage completed. Advancing to Behavioral."));

		QuestionsAskedInCurrentState = 0;

		AdvanceState();
	}
}

void UInterviewSessionManager::HandleBehavioralState()
{
	// Alpha Behavioral questions are generated as Medium difficulty.
	CurrentDifficulty = TEXT("Medium");

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("FSM: Behavioral | Difficulty: %s | %d / %d"),
		*CurrentDifficulty,
		QuestionsAskedInCurrentState,
		BehavioralQuestionLimit
	);

	if (QuestionsAskedInCurrentState >= BehavioralQuestionLimit)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Behavioral stage completed. "
				"Advancing to Closing."
			)
		);

		QuestionsAskedInCurrentState = 0;

		// Behavioral -> Closing
		AdvanceState();

		// There is no additional candidate answer after the final
		// Behavioral answer, so Closing must begin immediately.
		if (CurrentState == EInterviewState::Closing)
		{
			HandleClosingState();
		}
	}
}

void UInterviewSessionManager::HandleClosingState()
{
	if (bInterviewFinalized)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Closing ignored because the interview "
				"has already been finalized."
			)
		);
		return;
	}

	if (CurrentState != EInterviewState::Closing)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"HandleClosingState called from invalid state: %s"
			),
			*GetStateAsString()
		);
		return;
	}

	// Do not start the Closing/outro more than once.
	if (bClosingSpeechActive)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"HandleClosingState ignored because "
				"the actual Closing speech is already active."
			)
		);

		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("FSM: Closing State")
	);

	// Clear the previous question so no stale question remains active.
	CurrentQuestionText.Empty();

	if (ConversationalAI)
	{
		// We are now starting the ACTUAL Closing/outro turn.
		// Any earlier pending interviewer turn has already finished.
		bClosingRequestedAfterCurrentSpeech = false;
		bClosingSpeechActive = true;
		bAuraaTurnPending = true;

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Auraa interviewer turn started. "
				"Waiting for Closing speech to finish."
			)
		);

		ConversationalAI->SendQuestionToAI(
			TEXT(
				"The interview is now complete. "
				"Thank the candidate politely for their time and participation. "
				"Tell them that the interview has concluded. "
				"Do not reveal any score, evaluation, feedback, or result. "
				"Do not ask another question. "
				"Do not restart the interview."
			)
		);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Closing message request sent to Auraa. "
				"Waiting for Auraa to finish talking before finalization."
			)
		);

		UWorld* World = ResolveInterviewWorld(this);

		if (World)
		{
			World->GetTimerManager().SetTimer(
				ClosingFallbackTimerHandle,
				this,
				&UInterviewSessionManager::HandleClosingFallbackTimeout,
				15.0f,
				false
			);

			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"Closing fallback timer started for 15 seconds."
				)
			);
		}
		else
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT(
					"Closing fallback timer could not start. "
					"Finalizing immediately."
				)
			);

			FinalizeInterview();
		}

		return;
	}

	UE_LOG(
		LogTemp,
		Error,
		TEXT(
			"ConversationalAI is not initialized. "
			"Finalizing the interview immediately."
		)
	);

	FinalizeInterview();
}

void UInterviewSessionManager::HandleClosingFallbackTimeout()
{
	if (bInterviewFinalized)
	{
		return;
	}

	if (CurrentState != EInterviewState::Closing)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Closing fallback ignored | Current State: %s"
			),
			*GetStateAsString()
		);

		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Closing speech fallback timeout reached. "
			"Finalizing the interview."
		)
	);

	FinalizeInterview();
}

void UInterviewSessionManager::HandleFinishedState()
{
	UE_LOG(LogTemp, Warning, TEXT("FSM: Finished State"));
}