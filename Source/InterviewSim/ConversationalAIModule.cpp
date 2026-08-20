// Copyright @rootcode56, Inc. All Rights Reserved.

#include "ConversationalAIModule.h"

void UConversationalAIModule::InitializeAI()
{
	UE_LOG(LogTemp, Warning, TEXT("AI Module Initialized"));
}

void UConversationalAIModule::SetConvaiChatbot(UConvaiChatbotComponent* InChatbotComponent)
{
	ChatbotComponent = InChatbotComponent;
}

void UConversationalAIModule::SendQuestionToAI(const FString& Question)
{
	UE_LOG(LogTemp, Warning, TEXT("Sending Question To AI: %s"), *Question);

	if (ChatbotComponent)
	{
		ChatbotComponent->ExecuteNarrativeTrigger(
			Question,
			nullptr,
			false,
			true,
			false
		);

		UE_LOG(LogTemp, Warning, TEXT("Question sent to Convai."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ChatbotComponent is NULL."));
	}

	LastResponse = Question;
}

void UConversationalAIModule::SendAcknowledgement()
{
	UE_LOG(LogTemp, Warning, TEXT("========== ACKNOWLEDGEMENT FUNCTION ENTERED =========="));

	if (!ChatbotComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("ChatbotComponent is NULL. Cannot send acknowledgement."));
		return;
	}

	TArray<FString> Responses =
	{
		TEXT("Got it."),
		TEXT("Understood."),
		TEXT("I see."),
		TEXT("Alright."),
		TEXT("Thank you."),
		TEXT("Okay.")
	};

	int32 RandomIndex = FMath::RandRange(0, Responses.Num() - 1);

	const FString& SelectedResponse = Responses[RandomIndex];

	UE_LOG(LogTemp, Warning, TEXT("Acknowledgement selected: %s"), *SelectedResponse);
	UE_LOG(LogTemp, Warning, TEXT("Sending acknowledgement to Convai..."));

	ChatbotComponent->ExecuteNarrativeTrigger(
		SelectedResponse,
		nullptr,
		false,
		true,
		false
	);

	UE_LOG(LogTemp, Warning, TEXT("ExecuteNarrativeTrigger() finished."));

	LastResponse = SelectedResponse;
}

void UConversationalAIModule::SendAcknowledgementAndQuestion(
	const FString& Question
)
{
	// Legacy overload retained for compatibility.
	// Spoken acknowledgements are intentionally score-neutral.
	SendAcknowledgementAndQuestion(Question, 5.0f);
}

// 2 arguments
void UConversationalAIModule::SendAcknowledgementAndQuestion(
	const FString& Question,
	float QuickScore
)
{
	// Legacy path: no previous-answer context is available,
	// so use the normal neutral acknowledgement behavior.
	SendAcknowledgementAndQuestion(
		Question,
		QuickScore,
		FString(),
		FString()
	);
}

void UConversationalAIModule::SendAcknowledgementAndQuestion(
	const FString& Question,
	float QuickScore,
	const FString& PreviousQuestion,
	const FString& CandidateAnswer
)
{
	// Preserve all existing callers and behavior.
	SendAcknowledgementAndQuestion(
		Question,
		QuickScore,
		PreviousQuestion,
		CandidateAnswer,
		false
	);
}

void UConversationalAIModule::SendAcknowledgementAndQuestion(
	const FString& Question,
	float QuickScore,
	const FString& PreviousQuestion,
	const FString& CandidateAnswer,
	bool bUsePatienceAcknowledgement
)
{
	if (Question.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Cannot send acknowledgement and question: "
				"Question is empty."
			)
		);

		return;
	}

	if (!ChatbotComponent)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Cannot send acknowledgement and question: "
				"Convai ChatbotComponent is not assigned."
			)
		);

		return;
	}

	// QuickScore remains intentionally unused for spoken feedback.
	(void)QuickScore;

	const TArray<FString> NeutralAcknowledgements =
	{
		TEXT("Thank you for your response."),
		TEXT("Thank you."),
		TEXT("Alright, let us continue."),
		TEXT("Thank you. Let us move to the next question.")
	};

	const FString SelectedAcknowledgement =
		bUsePatienceAcknowledgement
		? TEXT("Thank you for your patience. Let us continue.")
		: NeutralAcknowledgements[
			FMath::RandRange(
				0,
				NeutralAcknowledgements.Num() - 1
			)
		];

	const FString CleanPreviousQuestion =
		PreviousQuestion.TrimStartAndEnd();

	const FString CleanCandidateAnswer =
		CandidateAnswer.TrimStartAndEnd();

	const bool bHasPreviousAnswerContext =
		!CleanPreviousQuestion.IsEmpty() &&
		!CleanCandidateAnswer.IsEmpty();

	FString InterviewerInstruction;

	if (bHasPreviousAnswerContext)
	{
		InterviewerInstruction = FString::Printf(
			TEXT(
				"You are the interviewer conducting the current interview. "

				"The candidate has just answered the previous interview question. "
				"Determine whether the candidate's answer is CLEARLY unrelated "
				"to what the previous question asked. "

				"A wrong answer is NOT derailment. "
				"A weak answer is NOT derailment. "
				"A short answer is NOT derailment. "
				"A partially correct answer is NOT derailment. "
				"An answer that discusses the same subject but contains mistakes "
				"is NOT derailment. "

				"Only treat the answer as derailment when it clearly moves to "
				"an unrelated subject and does not meaningfully address the "
				"previous question. "

				"If the answer is clearly derailing, briefly say: "
				"\"Your response seems to be moving away from the topic of the question. "
				"Please try to keep your answers relevant to the interview context.\" "

				"If the answer is not clearly derailing, briefly acknowledge "
				"the candidate by saying: \"%s\" "

				"After either response, ask the supplied next interview question. "

				"Do not reveal or mention any score. "
				"Do not say the candidate is correct or incorrect. "
				"Do not provide technical feedback or corrections. "
				"Do not ask the candidate to answer the previous question again. "
				"Do not stop, restart, or explain the interview. "
				"Ask exactly one next question and then wait for the candidate. "

				"Previous interview question: \"%s\" "
				"Candidate's answer: \"%s\" "
				"Next interview question: \"%s\""
			),
			*SelectedAcknowledgement,
			*CleanPreviousQuestion,
			*CleanCandidateAnswer,
			*Question
		);
	}
	else
	{
		InterviewerInstruction = FString::Printf(
			TEXT(
				"You are the interviewer conducting the current interview. "
				"Briefly acknowledge the candidate by saying: \"%s\" "
				"Then ask the supplied interview question. "
				"Keep the acknowledgement completely neutral. "
				"Do not evaluate the candidate. "
				"Do not mention any score. "
				"Do not provide feedback or corrections. "
				"Ask exactly one question and then wait for the candidate. "
				"Interview question: \"%s\""
			),
			*SelectedAcknowledgement,
			*Question
		);
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Sending acknowledgement/derailment-aware "
			"next-question turn to Convai | "
			"Previous answer context: %s"
		),
		bHasPreviousAnswerContext
		? TEXT("available")
		: TEXT("not available")
	);

	ChatbotComponent->ExecuteNarrativeTrigger(
		InterviewerInstruction,
		nullptr,
		false,
		true,
		false
	);

	LastResponse = Question;
}

FString UConversationalAIModule::GetLastAIResponse() const
{
	return LastResponse;
}

void UConversationalAIModule::OnInterviewStateChanged(EInterviewState NewState)
{
	UE_LOG(LogTemp, Warning, TEXT("AI notified of state change: %d"), (uint8)NewState);
}