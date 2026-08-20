#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "UObject/NoExportTypes.h"
#include "InterviewTypes.h"
#include "ConversationalAIModule.h"
#include "InterviewQuestion.h"
#include "InterviewSessionManager.generated.h"



class UOpenAIQuestionEngine;
class UQuestionPoolManager;
class UEvaluationEngine;
class URepetitionDetector;
class UConvaiChatbotComponent;
class UInterviewEvalServerSubsystem;

DECLARE_MULTICAST_DELEGATE(
	FOnInterviewFinalizedNative
);

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnQuestionsReadyNative,
	int32
);

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnQuestionGenerationFailedNative,
	const FString&
);

UCLASS()
class INTERVIEWSIM_API UInterviewSessionManager : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void Initialize();

	UFUNCTION(BlueprintCallable)
	void StartInterview();

	// Starts the interview using the previously selected mode,
	// configured duration, and stored/default job prompt.
	UFUNCTION(BlueprintCallable, Category = "Interview Setup")
	bool StartConfiguredInterview();

	// Selects which interview workflow will be used.
	// Nawshin's mode-selection widget will call this later.
	UFUNCTION(BlueprintCallable, Category = "Interview Setup")
	void SetInterviewMode(EInterviewMode NewMode);

	// Returns the currently selected interview workflow.
	UFUNCTION(BlueprintPure, Category = "Interview Setup")
	EInterviewMode GetInterviewMode() const;

	// Pauses the current interview and its timer.
	UFUNCTION(BlueprintCallable, Category = "Interview Control")
	void PauseInterview();

	// Resumes a previously paused interview.
	UFUNCTION(BlueprintCallable, Category = "Interview Control")
	void ResumeInterview();

	// Clears the completed/current session and returns to a clean pre-interview state.
	UFUNCTION(BlueprintCallable, Category = "Interview Control")
	void ResetInterview();

	// Returns true while the interview is paused.
	UFUNCTION(BlueprintPure, Category = "Interview Control")
	bool IsInterviewPaused() const;

	// Returns true after the current interview has been finalized.
	UFUNCTION(BlueprintPure, Category = "Interview Control")
	bool IsInterviewFinalized() const;

	// Returns true while a session exists and has not finished.
	UFUNCTION(BlueprintPure, Category = "Interview Control")
	bool IsInterviewActive() const;

	UFUNCTION(BlueprintPure, Category = "Interview Control")
	bool IsAuraaTurnPending() const;

	// Returns true only when the speech that just finished
	// was the actual Closing/outro speech.
	bool NotifyAuraaSpeechFinished();

	// Returns true when the configured interview is ready to start.
	UFUNCTION(BlueprintPure, Category = "Interview Setup")
	bool CanStartConfiguredInterview() const;

	UFUNCTION(BlueprintCallable)
	void AdvanceState();

	UFUNCTION(BlueprintCallable)
	void ReceiveJobPromptAndGenerateQuestions(const FString& JobPrompt);

	// Stores a job prompt supplied manually or later by Nawshin's UI.
	UFUNCTION(BlueprintCallable, Category = "Interview Setup")
	void SetJobPrompt(const FString& InJobPrompt);

	// Sets the interview duration in minutes.
	// Only 5, 10, or 15 minutes are supported.
	UFUNCTION(BlueprintCallable, Category = "Interview Timer")
	void SetInterviewDurationMinutes(float InDurationMinutes);

	// Returns the configured total interview duration.
	UFUNCTION(BlueprintPure, Category = "Interview Timer")
	float GetInterviewDurationMinutes() const;

	// Returns the current remaining interview time in seconds.
	UFUNCTION(BlueprintPure, Category = "Interview Timer")
	float GetRemainingInterviewTimeSeconds() const;

	// Returns the current remaining interview time in minutes.
	UFUNCTION(BlueprintPure, Category = "Interview Timer")
	float GetRemainingInterviewTimeMinutes() const;

	// Returns true while the interview timer is running.
	UFUNCTION(BlueprintPure, Category = "Interview Timer")
	bool IsInterviewTimerActive() const;

	UFUNCTION(BlueprintCallable)
	void SetConvaiChatbot(UConvaiChatbotComponent* InChatbotComponent);

	UFUNCTION(BlueprintCallable)
	void SubmitCandidateAnswer(const FString& Answer, EAnswerInputType InputType = EAnswerInputType::SpeechSTT);

	UFUNCTION(BlueprintCallable)
	void SaveAnswerHistoryToJson();

	UFUNCTION(BlueprintCallable)
	FString GetStateAsString() const;

	UFUNCTION(BlueprintCallable)
	FEvaluationResult GenerateFinalInterviewReport();

	// Lightweight live evaluation for FSM/adaptive interview flow.
	UFUNCTION(BlueprintCallable, Category = "Evaluation")
	FLightweightEvaluationResult EvaluateAnswerLightweight(
		const FString& CandidateAnswer,
		const FString& QuestionText
	);

	// Called by FSM when interview ends.
	UFUNCTION(BlueprintCallable, Category = "Interview Report")
	void FinalizeInterview();

	// Broadcast after the final report and export files are ready.
	FOnInterviewFinalizedNative OnInterviewFinalizedNative;

	FOnQuestionsReadyNative OnQuestionsReadyNative;

	FOnQuestionGenerationFailedNative OnQuestionGenerationFailedNative;

	// Returns the stored result from the most recently finalized interview.
	UFUNCTION(BlueprintPure, Category = "Interview Report")
	FEvaluationResult GetFinalInterviewReport() const;

	UFUNCTION(BlueprintPure, Category = "Interview Progress")
	int32 GetAnsweredQuestionCount() const;

	UFUNCTION(BlueprintPure, Category = "Interview Progress")
	int32 GetAskedQuestionCount() const;

	UFUNCTION(BlueprintPure, Category = "Interview Progress")
	int32 GetExpectedAnswerCount() const;

	UFUNCTION(BlueprintPure, Category = "Interview Progress")
	float GetInterviewProgressPercent() const;

	UFUNCTION(BlueprintPure, Category = "Interview Progress")
	FString GetCurrentQuestionText() const;

	UFUNCTION(BlueprintPure, Category = "Interview Progress")
	FString GetCurrentDifficulty() const;

	// Builds readable interviewer/candidate dialogue.
	UFUNCTION(BlueprintCallable, Category = "Interview Report")
	FString BuildDialogueTranscript() const;

	// Builds final report text from AnswerHistory.
	UFUNCTION(BlueprintCallable, Category = "Interview Report")
	FString BuildFinalReportText() const;

	// Exports dialogue transcript as .txt.
	UFUNCTION(BlueprintCallable, Category = "Interview Report")
	bool ExportTranscriptToFile(const FString& FileName);

	// Exports AnswerHistory as JSON.
	UFUNCTION(BlueprintCallable, Category = "Interview Report")
	bool ExportAnswerHistoryJSON(const FString& FileName);

private:
	EInterviewState CurrentState;

	// Stores whether the current session is a Real or Practice interview.
	// It remains None until a mode is selected.
	UPROPERTY()
	EInterviewMode CurrentInterviewMode =
		EInterviewMode::None;

	// =====================
	// FSM Runtime Variables
	// =====================

	// Number of questions asked in the current interview state.
	int32 QuestionsAskedInCurrentState = 0;

	// Total questions asked during the interview.
	int32 TotalQuestionsAsked = 0;

	// Current difficulty requested from QuestionPool.
	FString CurrentDifficulty = TEXT("Easy");

	// =====================
	// Alpha Interview Limits
	// =====================

	const int32 WarmupQuestionLimit = 2;
	const int32 TechnicalQuestionLimit = 5;
	const int32 BehavioralQuestionLimit = 3;

	UPROPERTY()
	UOpenAIQuestionEngine* QuestionEngine;

	UPROPERTY()
	UQuestionPoolManager* QuestionPool;

	UPROPERTY()
	UEvaluationEngine* EvaluationEngine;

	UPROPERTY()
	UInterviewEvalServerSubsystem* InterviewEvalServerSubsystem = nullptr;

	UPROPERTY()
	URepetitionDetector* RepetitionDetector;

	FString CurrentQuestionText;
	FString LastCandidateAnswer;

	// Stores the latest lightweight evaluation score so the next
	// Convai acknowledgement can reflect the previous answer.
	float LastQuickScore = 5.0f;

	// Prompt supplied by Nawshin's Prompt Engine for Real Interview mode.
	FString LastJobPrompt;

	// =====================
	// Interview Timer
	// =====================

	// Supported duration range.
	const float MinimumInterviewDurationMinutes = 5.0f;
	const float MaximumInterviewDurationMinutes = 15.0f;

	// Safe Alpha default.
	float InterviewDurationMinutes = 15.0f;

	// Updated while the interview is running.
	float RemainingInterviewTimeSeconds = 0.0f;

	// Prevents timer logic from running after stop/finalization.
	bool bInterviewTimerActive = false;

	// Prevents answer processing and progression while paused.
	bool bInterviewPaused = false;

	// True from the moment Auraa's interviewer turn is requested
	// until Convai reports that Auraa finished speaking.
	bool bAuraaTurnPending = false;

	// True when the interview timer expires while Auraa is already
	// speaking. The current turn is allowed to finish before the
	// actual Closing/outro speech begins.
	bool bClosingRequestedAfterCurrentSpeech = false;

	// True only while Auraa's actual Closing/outro speech is active.
	bool bClosingSpeechActive = false;

	// =====================
	// Practice Question Generation Transition
	// =====================

	// True only while Practice Interview is waiting for the
	// personalized question pool after the Greeting answer.
	bool bPracticeGenerationTransitionActive = false;

	// True after Gemini/OpenRouter/LocalFallback has produced
	// and stored a valid Practice question pool.
	bool bPracticeQuestionPoolReady = false;

	// True after Auraa has finished the short holding message
	// spoken while Practice questions are being generated.
	bool bPracticeHoldingMessageFinished = false;

	// Time when Auraa finished the temporary Practice holding message.
	double PracticeHoldingFinishedTimeSeconds = 0.0;

	// True only when the silent wait after the holding message
	// was long enough to justify a patience acknowledgement.
	bool bPracticeUsePatienceAcknowledgement = false;

	// Unreal timer used for one-second interview updates.
	FTimerHandle InterviewTimerHandle;

	// Safety timer used if Auraa does not produce a Closing speech.
	FTimerHandle ClosingFallbackTimerHandle;

	EAnswerInputType LastAnswerInputType =
		EAnswerInputType::SpeechSTT;

	UPROPERTY()
	TArray<FInterviewAnswerRecord> AnswerHistory;

	UPROPERTY()
	TArray<FEvaluationResult> EvaluationHistory;

	// Stores the most recently generated final interview result.
	UPROPERTY()
	FEvaluationResult FinalInterviewReport;

	UPROPERTY()
	bool bInterviewFinalized = false;

	UFUNCTION()
	void HandleEvaluationCompleted(FEvaluationResult Result);

	UFUNCTION()
	void HandleSidecarEvaluationCompleted(
		FEvaluationResult Result,
		bool bRequestSucceeded,
		FString Source,
		FString ErrorMessage,
		int32 AnswerRecordIndex
	);

	UPROPERTY()
	UConversationalAIModule* ConversationalAI;

	void PrintCurrentState();
	void AskNextQuestionFromPool();

	// Continues Practice Interview only when BOTH the personalized
	// question pool is ready and Auraa's holding message has finished.
	void TryContinuePracticeAfterGenerationWait();

	void GeneratePracticeQuestionsFromCandidateProfile(
		const FString& CandidateProfile
	);

	bool ValidateGeneratedQuestionDistribution(
		const TArray<FInterviewQuestion>& GeneratedQuestions,
		FString& OutErrorMessage
	) const;

	void RegenerateQuestionsWithStrictDistribution(
		const FString& OriginalGenerationPrompt,
		const FString& FlowLabel,
		bool bStartInterviewAfterSuccess
	);

	// Returns Nawshin's focused prompt for Real Interview.
	// Returns empty for Practice Interview because Practice uses
	// its own API-driven candidate-profile flow.
	FString GetEffectiveJobPrompt() const;

	// Starts a new timer using InterviewDurationMinutes.
	void StartInterviewTimer();

	// Stops the active timer without changing its configured duration.
	void StopInterviewTimer();

	// Restores remaining time to the configured full duration.
	void ResetInterviewTimer();

	// Called once per second while the interview is running.
	void HandleInterviewTimerTick();

	// Advances the interview FSM after a successful answer.
	void AdvanceInterviewFSM();

	// =====================
	// FSM State Handlers
	// =====================

	void HandleGreetingState();
	void HandleWarmupState();
	void HandleTechnicalState();
	void HandleBehavioralState();
	void HandleClosingState();
	void HandleClosingFallbackTimeout();
	void HandleFinishedState();

	// Saves one question-answer-evaluation record into AnswerHistory.
	void SaveLightweightAnswerRecord(
		const FString& Question,
		const FString& CandidateAnswer,
		EAnswerInputType InputType,
		const FLightweightEvaluationResult& LightweightResult
	);

	// Local repeated-answer check using AnswerHistory.
	bool IsRepeatedAnswerLocal(const FString& CandidateAnswer) const;

};