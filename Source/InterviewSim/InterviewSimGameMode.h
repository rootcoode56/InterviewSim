// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "GameFramework/GameModeBase.h"
#include "InterviewTypes.h"
#include "InterviewSimGameMode.generated.h"

class UInterviewSessionManager; // ← Forward declaration
class UConvaiChatbotComponent;  // Chatbot Component

UCLASS()
class INTERVIEWSIM_API AInterviewSimGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AInterviewSimGameMode();

	UFUNCTION(BlueprintCallable)
	void SubmitConvaiSTTAnswer(const FString& CandidateAnswer);

	UFUNCTION(BlueprintCallable)
	void BeginConvaiSTTSession();

	UFUNCTION(BlueprintCallable)
	void EndConvaiSTTSession();

	UFUNCTION(BlueprintImplementableEvent, Category = "Interview|STT")
	void DisplayFinalizedConvaiSTTAnswer(const FString& FinalAnswer);

	UFUNCTION(BlueprintCallable)
	void SubmitTypedAnswer(const FString& CandidateAnswer);

	UFUNCTION(BlueprintCallable)
	void SubmitWrittenCodeAnswer(const FString& CodeAnswer);


	UFUNCTION(BlueprintCallable, Category = "Interview")
	UInterviewSessionManager* GetSessionManager() const;

	UFUNCTION(BlueprintCallable, Category = "Interview")
	void SetInterviewConvaiChatbot(UConvaiChatbotComponent* InChatbotComponent);

	// Temporarily used until Nawshin's pause widget is integrated.
	UFUNCTION(BlueprintCallable, Category = "Interview Control")
	void PauseCurrentInterview();

	// Temporarily used until Nawshin's pause widget is integrated.
	UFUNCTION(BlueprintCallable, Category = "Interview Control")
	void ResumeCurrentInterview();

	UFUNCTION(BlueprintCallable, Category = "Interview Control")
	void FinishCurrentInterview();

	UFUNCTION(BlueprintCallable, Category = "Interview Control")
	void ResetCurrentInterview();

	UFUNCTION(BlueprintPure, Category = "Interview Control")
	bool IsCurrentInterviewFinalized() const;

	UFUNCTION(BlueprintPure, Category = "Interview Timer")
	float GetCurrentInterviewRemainingTimeSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Interview Control")
	bool IsCurrentInterviewPaused() const;

	UFUNCTION(BlueprintPure, Category = "Interview Control")
	bool IsCurrentInterviewActive() const;

	// Returns true only when the candidate is currently allowed
	// to type or begin a Push-To-Talk answer.
	UFUNCTION(BlueprintPure, Category = "Interview Control")
	bool CanCandidateInteractNow() const;

	UFUNCTION(BlueprintPure, Category = "Interview Control")
	bool CanPauseCurrentInterview() const;

	UFUNCTION(BlueprintPure, Category = "Interview Control")
	bool CanResumeCurrentInterview() const;

	UFUNCTION(BlueprintPure, Category = "Interview Control")
	bool CanResetCurrentInterview() const;

	UFUNCTION(BlueprintPure, Category = "Interview Setup")
	bool CanStartCurrentConfiguredInterview() const;

	UFUNCTION(BlueprintPure, Category = "Interview State")
	FString GetCurrentInterviewState() const;

	UFUNCTION(BlueprintPure, Category = "Interview Setup")
	EInterviewMode GetCurrentInterviewMode() const;

	UFUNCTION(BlueprintPure, Category = "Interview Report")
	FEvaluationResult GetCurrentFinalInterviewReport() const;

	UFUNCTION(BlueprintPure, Category = "Interview Progress")
	int32 GetCurrentAnsweredQuestionCount() const;

	UFUNCTION(BlueprintPure, Category = "Interview Progress")
	int32 GetCurrentAskedQuestionCount() const;

	UFUNCTION(BlueprintPure, Category = "Interview Progress")
	int32 GetCurrentExpectedAnswerCount() const;

	UFUNCTION(BlueprintPure, Category = "Interview Progress")
	FString GetCurrentQuestionText() const;

	UFUNCTION(BlueprintPure, Category = "Interview Progress")
	FString GetCurrentInterviewDifficulty() const;

	UFUNCTION(BlueprintPure, Category = "Interview Timer")
	bool IsCurrentInterviewTimerActive() const;

	UFUNCTION(BlueprintPure, Category = "Interview Timer")
	float GetCurrentInterviewDurationMinutes() const;

	UFUNCTION(BlueprintCallable, Category = "Interview Timer")
	void SetCurrentInterviewDurationMinutes(float InDurationMinutes);

	UFUNCTION(BlueprintCallable, Category = "Interview Setup")
	void SetCurrentInterviewMode(EInterviewMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Interview Setup")
	void SetCurrentJobPrompt(const FString& InJobPrompt);

	UFUNCTION(BlueprintCallable, Category = "Interview Setup")
	bool StartCurrentConfiguredInterview();

	UFUNCTION(BlueprintCallable, Category = "Interview Reports")
	void OpenInterviewReportsFolder();

	UFUNCTION(BlueprintPure, Category = "Interview Loading")
	bool IsInterviewLoading() const;

	UFUNCTION(BlueprintCallable, Category = "Interview Loading")
	void NotifyInterviewReady();

	UFUNCTION(BlueprintCallable, Category = "Interview")
	void NotifyInterviewSpeechFinished();

protected:
	virtual void BeginPlay() override;

	// Implemented in BP_InterviewSimGameMode to open WBP_scoreB.
	UFUNCTION(BlueprintImplementableEvent, Category = "Interview UI")
	void ShowFinalInterviewResults();

	UFUNCTION(BlueprintImplementableEvent, Category = "Interview UI")
	void ShowInterviewLoadingUI();

	UFUNCTION(BlueprintImplementableEvent, Category = "Interview UI")
	void ShowInterviewReadyUI(
		int32 GeneratedQuestionCount
	);

	UFUNCTION(BlueprintImplementableEvent, Category = "Interview UI")
	void ShowQuestionGenerationFailureUI(
		const FString& ErrorMessage
	);



private:
	UPROPERTY()
	UInterviewSessionManager* SessionManager;

	bool bConvaiSTTSessionActive = false;

	bool bConvaiSTTEndRequested = false;

	bool bIgnoreConvaiSTTCallbacksUntilNextSession = true;

	FString ConvaiSTTSessionBuffer;

	FTimerHandle ConvaiSTTFinalizeTimerHandle;

	void FinalizeConvaiSTTSession();

	// Receives the SessionManager finalization broadcast.
	void HandleSessionInterviewFinalized();

	// Called after generated questions are added to QuestionPool.
	void HandleQuestionsReady(
		int32 GeneratedQuestionCount
	);

	// Called when question generation fails.
	void HandleQuestionGenerationFailed(
		const FString& ErrorMessage
	);

	bool bInterviewStarted = false;

	bool bInterviewLoading = false;

	// Prevents the loading state from remaining active forever
	// when Auraa does not respond.
	FTimerHandle InterviewLoadingTimeoutHandle;

	FTimerHandle PracticeReadyFallbackHandle;
	
	// Called when Auraa does not become ready within the allowed time.
	void HandleInterviewLoadingTimeout();

	
};