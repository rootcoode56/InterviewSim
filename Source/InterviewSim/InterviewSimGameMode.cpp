#include "InterviewSimGameMode.h"
#include "InterviewSessionManager.h"
#include "TimerManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFileManager.h"


AInterviewSimGameMode::AInterviewSimGameMode()
{
}

void AInterviewSimGameMode::BeginPlay()
{
	Super::BeginPlay();

	SessionManager = NewObject<UInterviewSessionManager>(this);

	if (SessionManager)
	{
		SessionManager->Initialize();

		SessionManager->OnInterviewFinalizedNative.AddUObject(
			this,
			&AInterviewSimGameMode::HandleSessionInterviewFinalized
		);

		SessionManager->OnQuestionsReadyNative.AddUObject(
			this,
			&AInterviewSimGameMode::HandleQuestionsReady
		);

		SessionManager->OnQuestionGenerationFailedNative.AddUObject(
			this,
			&AInterviewSimGameMode::HandleQuestionGenerationFailed
		);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"SessionManager initialized in GameMode. "
				"Waiting for Convai chatbot assignment."
			)
		);
	}
}

void AInterviewSimGameMode::HandleSessionInterviewFinalized()
{
	GetWorldTimerManager().ClearTimer(
		InterviewLoadingTimeoutHandle
	);

	InterviewLoadingTimeoutHandle.Invalidate();

	bInterviewLoading = false;
	bInterviewStarted = false;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Interview finalization received by GameMode. "
			"Requesting final results UI."
		)
	);

	ShowFinalInterviewResults();
}

void AInterviewSimGameMode::HandleQuestionsReady(
	int32 GeneratedQuestionCount
)
{
	GetWorldTimerManager().ClearTimer(
		InterviewLoadingTimeoutHandle
	);

	InterviewLoadingTimeoutHandle.Invalidate();

	bInterviewLoading = false;

	ShowInterviewReadyUI(
		GeneratedQuestionCount
	);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Question generation completed successfully. "
			"Generated Question Count: %d | "
			"Interview loading state cleared."
		),
		GeneratedQuestionCount
	);
}

void AInterviewSimGameMode::HandleQuestionGenerationFailed(
	const FString& ErrorMessage
)
{
	GetWorldTimerManager().ClearTimer(
		InterviewLoadingTimeoutHandle
	);

	InterviewLoadingTimeoutHandle.Invalidate();

	bInterviewLoading = false;
	bInterviewStarted = false;

	ShowQuestionGenerationFailureUI(
		ErrorMessage
	);

	UE_LOG(
		LogTemp,
		Error,
		TEXT(
			"Question generation failed. "
			"Loading state cleared. Error: %s"
		),
		*ErrorMessage
	);
}


void AInterviewSimGameMode::SubmitConvaiSTTAnswer(
	const FString& CandidateAnswer
)
{
	if (!SessionManager)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("SessionManager is not initialized")
		);
		return;
	}

	FString CleanAnswer = CandidateAnswer;
	CleanAnswer.TrimStartAndEndInline();

	if (CleanAnswer.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Verbose,
			TEXT("Empty Convai STT callback ignored")
		);

		return;
	}

	if (bIgnoreConvaiSTTCallbacksUntilNextSession)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Late Convai STT callback ignored: %s"),
			*CleanAnswer
		);

		return;
	}

	if (bConvaiSTTSessionActive)
	{
		if (!CleanAnswer.IsEmpty())
		{
			if (!ConvaiSTTSessionBuffer.IsEmpty())
			{
				ConvaiSTTSessionBuffer += TEXT(" ");
			}

			ConvaiSTTSessionBuffer += CleanAnswer;

			if (bConvaiSTTEndRequested)
			{
				GetWorldTimerManager().ClearTimer(
					ConvaiSTTFinalizeTimerHandle
				);

				FinalizeConvaiSTTSession();
			}
		}

		return;
	}

	// During Greeting, reject very short finalized STT fragments
	// such as "Hello, Aura." before they can advance the FSM.
	// Typed answers and later interview stages are unaffected.
	if (
		SessionManager->GetStateAsString().Equals(
			TEXT("Greeting"),
			ESearchCase::IgnoreCase
		)
		)
	{
		TArray<FString> GreetingWords;
		CleanAnswer.ParseIntoArrayWS(GreetingWords);

		if (GreetingWords.Num() < 4)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"Incomplete Greeting STT fragment ignored: %s"
				),
				*CleanAnswer
			);
			return;
		}
	}

	//DisplayFinalizedConvaiSTTAnswer(CleanAnswer);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Convai STT Answer Submitted: %s"),
		*CleanAnswer
	);

	SessionManager->SubmitCandidateAnswer(
		CleanAnswer,
		EAnswerInputType::SpeechSTT
	);
	DisplayFinalizedConvaiSTTAnswer(CleanAnswer);

}

void AInterviewSimGameMode::BeginConvaiSTTSession()
{
	if (!CanCandidateInteractNow())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Convai STT session start blocked because "
				"candidate interaction is not currently allowed."
			)
		);

		return;
	}

	bConvaiSTTSessionActive = true;
	bConvaiSTTEndRequested = false;
	bIgnoreConvaiSTTCallbacksUntilNextSession = false;

	ConvaiSTTSessionBuffer.Empty();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Convai STT session started")
	);
}

void AInterviewSimGameMode::EndConvaiSTTSession()
{
	bConvaiSTTEndRequested = true;

	GetWorldTimerManager().SetTimer(
		ConvaiSTTFinalizeTimerHandle,
		this,
		&AInterviewSimGameMode::FinalizeConvaiSTTSession,
		5.0f,
		false
	);
}

void AInterviewSimGameMode::FinalizeConvaiSTTSession()
{
	bConvaiSTTSessionActive = false;

	FString FinalAnswer = ConvaiSTTSessionBuffer;
	ConvaiSTTSessionBuffer.Empty();

	FinalAnswer.TrimStartAndEndInline();

	if (FinalAnswer.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Convai STT session ended with no speech")
		);
		bIgnoreConvaiSTTCallbacksUntilNextSession = true;
		return;
	}

	SubmitConvaiSTTAnswer(FinalAnswer);
	bIgnoreConvaiSTTCallbacksUntilNextSession = true;
}

void AInterviewSimGameMode::SubmitTypedAnswer(
	const FString& CandidateAnswer
)
{
	if (!SessionManager)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("SessionManager is not initialized")
		);
		return;
	}

	if (!CanCandidateInteractNow())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Typed answer blocked because candidate "
				"interaction is not currently allowed."
			)
		);

		return;
	}

	FString CleanAnswer = CandidateAnswer;
	CleanAnswer.TrimStartAndEndInline();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Typed Answer Submitted: %s"),
		*CleanAnswer
	);

	SessionManager->SubmitCandidateAnswer(
		CleanAnswer,
		EAnswerInputType::WrittenText
	);
}

void AInterviewSimGameMode::SubmitWrittenCodeAnswer(const FString& CodeAnswer)
{
	if (!SessionManager)
	{
		UE_LOG(LogTemp, Error, TEXT("SessionManager is not initialized"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Written Code Answer Submitted: %s"), *CodeAnswer);

	SessionManager->SubmitCandidateAnswer(
		CodeAnswer,
		EAnswerInputType::WrittenCode
	);
}

UInterviewSessionManager* AInterviewSimGameMode::GetSessionManager() const
{
	return SessionManager;
}

void AInterviewSimGameMode::PauseCurrentInterview()
{
	if (!SessionManager)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Cannot pause interview because "
				"SessionManager is not initialized."
			)
		);

		return;
	}

	if (!CanPauseCurrentInterview())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"PauseCurrentInterview rejected. "
				"Pausing is only allowed during an active "
				"Practice Interview."
			)
		);

		return;
	}

	SessionManager->PauseInterview();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("PauseCurrentInterview request sent.")
	);
}

void AInterviewSimGameMode::ResumeCurrentInterview()
{
	if (!SessionManager)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Cannot resume interview because "
				"SessionManager is not initialized."
			)
		);

		return;
	}

	if (!CanResumeCurrentInterview())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"ResumeCurrentInterview rejected. "
				"Resuming is only allowed during a paused "
				"Practice Interview."
			)
		);

		return;
	}

	SessionManager->ResumeInterview();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("ResumeCurrentInterview request sent.")
	);
}

void AInterviewSimGameMode::FinishCurrentInterview()
{
	if (!SessionManager)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Cannot finish interview because "
				"SessionManager is not initialized."
			)
		);

		return;
	}

	SessionManager->FinalizeInterview();

	GetWorldTimerManager().ClearTimer(
		InterviewLoadingTimeoutHandle
	);

	InterviewLoadingTimeoutHandle.Invalidate();

	// The interview has ended, so no loading state should remain.
	bInterviewLoading = false;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("FinishCurrentInterview request sent.")
	);
}

void AInterviewSimGameMode::ResetCurrentInterview()
{
	if (!SessionManager)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Cannot reset interview because "
				"SessionManager is not initialized."
			)
		);

		return;
	}

	if (!CanResetCurrentInterview())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"ResetCurrentInterview rejected. "
				"Resetting is only allowed for a "
				"Practice Interview."
			)
		);

		return;
	}

	SessionManager->ResetInterview();

	GetWorldTimerManager().ClearTimer(
		InterviewLoadingTimeoutHandle
	);

	InterviewLoadingTimeoutHandle.Invalidate();

	// Allow a new interview to start after reset.
	bInterviewStarted = false;

	// Ensure any loading overlay is also cleared.
	bInterviewLoading = false;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("ResetCurrentInterview request sent.")
	);
}

bool AInterviewSimGameMode::IsCurrentInterviewFinalized() const
{
	if (!SessionManager)
	{
		return false;
	}

	return SessionManager->IsInterviewFinalized();
}

float AInterviewSimGameMode::GetCurrentInterviewRemainingTimeSeconds() const
{
	if (!SessionManager)
	{
		return 0.0f;
	}

	return SessionManager->GetRemainingInterviewTimeSeconds();
}

bool AInterviewSimGameMode::IsCurrentInterviewPaused() const
{
	if (!SessionManager)
	{
		return false;
	}

	return SessionManager->IsInterviewPaused();
}

bool AInterviewSimGameMode::IsCurrentInterviewActive() const
{
	if (!SessionManager)
	{
		return false;
	}

	return SessionManager->IsInterviewActive();
}

bool AInterviewSimGameMode::CanCandidateInteractNow() const
{
	if (!SessionManager)
	{
		return false;
	}

	if (!SessionManager->IsInterviewActive())
	{
		return false;
	}

	if (SessionManager->IsInterviewPaused())
	{
		return false;
	}

	if (SessionManager->IsAuraaTurnPending())
	{
		return false;
	}

	if (
		SessionManager->GetStateAsString().Equals(
			TEXT("Closing"),
			ESearchCase::IgnoreCase
		)
		)
	{
		return false;
	}

	return true;
}


bool AInterviewSimGameMode::CanPauseCurrentInterview() const
{
	if (!SessionManager)
	{
		return false;
	}

	return
		SessionManager->GetInterviewMode() ==
		EInterviewMode::PracticeInterview &&
		SessionManager->IsInterviewActive() &&
		!SessionManager->IsInterviewPaused() &&
		!SessionManager->IsAuraaTurnPending();
}

bool AInterviewSimGameMode::CanResumeCurrentInterview() const
{
	if (!SessionManager)
	{
		return false;
	}

	return
		SessionManager->GetInterviewMode() ==
		EInterviewMode::PracticeInterview &&
		SessionManager->IsInterviewActive() &&
		SessionManager->IsInterviewPaused();
}

bool AInterviewSimGameMode::CanResetCurrentInterview() const
{
	if (!SessionManager)
	{
		return false;
	}

	return
		SessionManager->GetInterviewMode() ==
		EInterviewMode::PracticeInterview &&
		(
			SessionManager->IsInterviewActive() ||
			SessionManager->IsInterviewFinalized()
			);
}

bool AInterviewSimGameMode::CanStartCurrentConfiguredInterview() const
{
	if (!SessionManager)
	{
		return false;
	}

	return SessionManager->CanStartConfiguredInterview();
}

FString AInterviewSimGameMode::GetCurrentInterviewState() const
{
	if (!SessionManager)
	{
		return TEXT("None");
	}

	return SessionManager->GetStateAsString();
}

EInterviewMode AInterviewSimGameMode::GetCurrentInterviewMode() const
{
	if (!SessionManager)
	{
		return EInterviewMode::None;
	}

	return SessionManager->GetInterviewMode();
}

FEvaluationResult AInterviewSimGameMode::GetCurrentFinalInterviewReport() const
{
	if (!SessionManager)
	{
		return FEvaluationResult();
	}

	return SessionManager->GetFinalInterviewReport();
}

int32 AInterviewSimGameMode::GetCurrentAnsweredQuestionCount() const
{
	if (!SessionManager)
	{
		return 0;
	}

	return SessionManager->GetAnsweredQuestionCount();
}

int32 AInterviewSimGameMode::GetCurrentAskedQuestionCount() const
{
	if (!SessionManager)
	{
		return 0;
	}

	return SessionManager->GetAskedQuestionCount();
}

int32 AInterviewSimGameMode::GetCurrentExpectedAnswerCount() const
{
	if (!SessionManager)
	{
		return 0;
	}

	return SessionManager->GetExpectedAnswerCount();
}

FString AInterviewSimGameMode::GetCurrentQuestionText() const
{
	if (!SessionManager)
	{
		return FString();
	}

	return SessionManager->GetCurrentQuestionText();
}

FString AInterviewSimGameMode::GetCurrentInterviewDifficulty() const
{
	if (!SessionManager)
	{
		return TEXT("Easy");
	}

	return SessionManager->GetCurrentDifficulty();
}

bool AInterviewSimGameMode::IsCurrentInterviewTimerActive() const
{
	if (!SessionManager)
	{
		return false;
	}

	return SessionManager->IsInterviewTimerActive();
}

bool AInterviewSimGameMode::IsInterviewLoading() const
{
	return bInterviewLoading;
}

void AInterviewSimGameMode::NotifyInterviewReady()
{
	if (!bInterviewLoading)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"NotifyInterviewReady ignored because "
				"the interview is not currently loading."
			)
		);

		return;
	}

	if (
		SessionManager &&
		SessionManager->GetInterviewMode() ==
		EInterviewMode::RealInterview
		)
	{
		// Auraa is already ready, so the interview setup/Auraa
		// readiness timeout is no longer needed.
		// Question generation now controls its own success/failure
		// through the existing ready/failure callbacks.
		GetWorldTimerManager().ClearTimer(
			InterviewLoadingTimeoutHandle
		);

		InterviewLoadingTimeoutHandle.Invalidate();

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Auraa readiness received for Real Interview. "
				"Loading timeout cleared. "
				"Waiting for question-generation readiness."
			)
		);

		return;
	}

	GetWorldTimerManager().ClearTimer(
		PracticeReadyFallbackHandle
	);

	PracticeReadyFallbackHandle.Invalidate();

	GetWorldTimerManager().ClearTimer(
		InterviewLoadingTimeoutHandle
	);

	InterviewLoadingTimeoutHandle.Invalidate();

	bInterviewLoading = false;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Interview is ready. "
			"Loading state cleared."
		)
	);

	// Convai is genuinely ready now.
// Keep WBP_Load visible briefly for a clean visual transition
// before opening the interview and starting Auraa's Greeting.
	FTimerHandle PracticeVisualLoadingHandle;

	GetWorldTimerManager().SetTimer(
		PracticeVisualLoadingHandle,
		FTimerDelegate::CreateWeakLambda(
			this,
			[this]()
			{
				if (
					!SessionManager ||
					SessionManager->GetInterviewMode() !=
					EInterviewMode::PracticeInterview
					)
				{
					return;
				}

				// Open WBP_Interview and remove WBP_Load.
				ShowInterviewReadyUI(0);

				// Only now start the timer and Auraa's Greeting.
				SessionManager->StartInterview();

				UE_LOG(
					LogTemp,
					Warning,
					TEXT(
						"Deferred Practice Interview started after "
						"the 4-second visual loading transition."
					)
				);
			}
		),
		4.0f,
		false
	);
}

void AInterviewSimGameMode::NotifyInterviewSpeechFinished()
{
	if (!SessionManager)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"NotifyInterviewSpeechFinished failed because "
				"SessionManager is not initialized."
			)
		);

		return;
	}

	const bool bActualClosingSpeechFinished =
		SessionManager->NotifyAuraaSpeechFinished();

	if (!bActualClosingSpeechFinished)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Interview speech finished notification processed | "
				"Current State: %s | "
				"Actual Closing Finished: false"
			),
			*SessionManager->GetStateAsString()
		);

		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Actual Auraa Closing speech finished. "
			"Finalizing the interview now."
		)
	);

	SessionManager->FinalizeInterview();
}

void AInterviewSimGameMode::HandleInterviewLoadingTimeout()
{
	GetWorldTimerManager().ClearTimer(
		InterviewLoadingTimeoutHandle
	);

	InterviewLoadingTimeoutHandle.Invalidate();

	if (!bInterviewLoading)
	{
		return;
	}

	bInterviewLoading = false;

	ShowQuestionGenerationFailureUI(
		TEXT(
			"Interview setup timed out before questions became ready."
		)
	);

	UE_LOG(
		LogTemp,
		Error,
		TEXT(
			"Interview loading timed out. "
			"Question-generation readiness was not received."
		)
	);
}

float AInterviewSimGameMode::GetCurrentInterviewDurationMinutes() const
{
	if (!SessionManager)
	{
		return 0.0f;
	}

	return SessionManager->GetInterviewDurationMinutes();
}

void AInterviewSimGameMode::SetCurrentInterviewDurationMinutes(
	float InDurationMinutes
)
{
	if (!SessionManager)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Cannot set interview duration because "
				"SessionManager is not initialized."
			)
		);

		return;
	}

	SessionManager->SetInterviewDurationMinutes(
		InDurationMinutes
	);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"SetCurrentInterviewDurationMinutes request sent | "
			"Requested Duration: %.2f minutes"
		),
		InDurationMinutes
	);
}

void AInterviewSimGameMode::SetCurrentInterviewMode(
	EInterviewMode NewMode
)
{
	if (!SessionManager)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Cannot set interview mode because "
				"SessionManager is not initialized."
			)
		);

		return;
	}

	SessionManager->SetInterviewMode(NewMode);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("SetCurrentInterviewMode request sent.")
	);
}

void AInterviewSimGameMode::SetCurrentJobPrompt(
	const FString& InJobPrompt
)
{
	if (!SessionManager)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Cannot set job prompt because "
				"SessionManager is not initialized."
			)
		);

		return;
	}

	SessionManager->SetJobPrompt(InJobPrompt);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"SetCurrentJobPrompt request sent | "
			"Prompt Length: %d characters"
		),
		InJobPrompt.Len()
	);
}

bool AInterviewSimGameMode::StartCurrentConfiguredInterview()
{
	if (!SessionManager)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Cannot start configured interview because "
				"SessionManager is not initialized."
			)
		);

		bInterviewLoading = false;
		return false;
	}

	// The UI can show its loading overlay from this point.
	bInterviewLoading = true;


	// Clear any previous loading timeout before starting a new one.
	GetWorldTimerManager().ClearTimer(
		InterviewLoadingTimeoutHandle
	);

	InterviewLoadingTimeoutHandle.Invalidate();

	// Allow up to 30 seconds for interview setup readiness.
	GetWorldTimerManager().SetTimer(
		InterviewLoadingTimeoutHandle,
		this,
		&AInterviewSimGameMode::HandleInterviewLoadingTimeout,
		30.0f,
		false
	);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Interview loading state started. "
			"Waiting up to 30 seconds for interview readiness."
		)
	);

	const bool bStarted =
		SessionManager->StartConfiguredInterview();

	if (bStarted)
	{
		bInterviewStarted = true;

		// Both Practice and Real Interview use WBP_Load.
		ShowInterviewLoadingUI();

		/*if (
			SessionManager->GetInterviewMode() ==
			EInterviewMode::PracticeInterview
			)
		{
			GetWorldTimerManager().ClearTimer(
				PracticeReadyFallbackHandle
			);

			GetWorldTimerManager().SetTimer(
				PracticeReadyFallbackHandle,
				this,
				&AInterviewSimGameMode::NotifyInterviewReady,
				2.5f,
				false
			);
		}*/

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"StartCurrentConfiguredInterview completed successfully. "
				"Waiting for the required readiness notification."
			)
		);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(
			InterviewLoadingTimeoutHandle
		);

		InterviewLoadingTimeoutHandle.Invalidate();

		bInterviewStarted = false;
		bInterviewLoading = false;

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"StartCurrentConfiguredInterview request was rejected. "
				"Loading state and timeout cleared."
			)
		);
	}

	return bStarted;
}

void AInterviewSimGameMode::SetInterviewConvaiChatbot(
	UConvaiChatbotComponent* InChatbotComponent
)
{
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("SET INTERVIEW CONVAI CHATBOT FUNCTION CALLED")
	);

	if (!SessionManager)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("SessionManager is NULL.")
		);

		return;
	}

	if (!InChatbotComponent)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Cannot assign interview Convai chatbot because "
				"the provided component is NULL."
			)
		);

		return;
	}

	SessionManager->SetConvaiChatbot(
		InChatbotComponent
	);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Convai Chatbot assigned to SessionManager. "
			"Waiting for the user to select an interview mode "
			"and start through the final UI."
		)
	);
}

void AInterviewSimGameMode::OpenInterviewReportsFolder()
{
	FString ReportsFolderPath =
		FPaths::ProjectSavedDir() / TEXT("InterviewReports");

	// Convert the Unreal relative path into an absolute Windows path.
	ReportsFolderPath =
		FPaths::ConvertRelativePathToFull(ReportsFolderPath);

	FPaths::NormalizeDirectoryName(ReportsFolderPath);

	IPlatformFile& PlatformFile =
		FPlatformFileManager::Get().GetPlatformFile();

	// Ensure the folder exists before asking Windows to open it.
	if (!PlatformFile.DirectoryExists(*ReportsFolderPath))
	{
		const bool bFolderCreated =
			PlatformFile.CreateDirectoryTree(*ReportsFolderPath);

		if (!bFolderCreated)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("Failed to create reports folder: %s"),
				*ReportsFolderPath
			);

			return;
		}
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Opening interview reports folder: %s"),
		*ReportsFolderPath
	);

	FPlatformProcess::ExploreFolder(*ReportsFolderPath);
}