//Copyright @rootcode56
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "InterviewTypes.h"
#include "ConvaiChatbotComponent.h"
#include "ConversationalAIModule.generated.h"

UCLASS()
class INTERVIEWSIM_API UConversationalAIModule : public UObject
{
	GENERATED_BODY()

public:

	void InitializeAI();


	void SendQuestionToAI(const FString& Question);

	void SendAcknowledgement();

	// Legacy version kept temporarily for existing callers.
	void SendAcknowledgementAndQuestion(const FString& Question);

	// Sends one combined Convai trigger containing a score-aware
	// acknowledgement followed by the next interview question.
	void SendAcknowledgementAndQuestion(
		const FString& Question,
		float QuickScore
	);

	// Sends the next interview question while allowing Auraa to
	// warn the candidate only when the previous answer is clearly
	// unrelated to the previous question.
	void SendAcknowledgementAndQuestion(
		const FString& Question,
		float QuickScore,
		const FString& PreviousQuestion,
		const FString& CandidateAnswer
	);

	// Same derailment-aware next-question path, with an optional
	// patience acknowledgement for a genuinely long Practice wait.
	void SendAcknowledgementAndQuestion(
		const FString& Question,
		float QuickScore,
		const FString& PreviousQuestion,
		const FString& CandidateAnswer,
		bool bUsePatienceAcknowledgement
	);

	FString GetLastAIResponse() const;

	void OnInterviewStateChanged(EInterviewState NewState);

	void SetConvaiChatbot(UConvaiChatbotComponent* InChatbotComponent);

private:

	FString LastResponse;

	UPROPERTY()
	UConvaiChatbotComponent* ChatbotComponent = nullptr;
};