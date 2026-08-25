// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "PromptEngineSubsystem.generated.h"

/**
 * Identifies what kind of document was uploaded.
 */
UENUM(BlueprintType)
enum class EPromptDocumentType : uint8
{
	JobCircular UMETA(DisplayName = "Job Circular"),
	CVResume    UMETA(DisplayName = "CV / Resume")
};

/**
 * Triggered when prompt generation fails.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnPromptGenerationFailed,
	const FString&,
	ErrorMessage
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnQuestionGenerationPromptReady,
	const FString&,
	QuestionGenerationPrompt
);

/**
 * Nawshin Kabir's Prompt Generation and
 * Job Circular Analysis subsystem.
 */
UCLASS()
class INTERVIEWSIM_API UPromptEngineSubsystem
	: public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(
		FSubsystemCollectionBase& Collection
	) override;

	virtual void Deinitialize() override;

	/**
 * Reads a CV, resume, or job-circular file and builds
 * a focused prompt without using a local AI model.
 */
	UFUNCTION(
		BlueprintCallable,
		Category = "AI|Prompt Engine"
	)
	void GenerateQuestionPromptFromFile(
		const FString& FilePath,
		EPromptDocumentType DocumentType
	);

	UFUNCTION(
		BlueprintCallable,
		Category = "AI|Prompt Engine"
	)
	void GenerateQuestionPromptFromFiles(
		const FString& CVFilePath,
		const FString& JobCircularFilePath
	);

	UPROPERTY(
		BlueprintAssignable,
		Category = "Prompt Engine"
	)
	FOnQuestionGenerationPromptReady
		OnQuestionGenerationPromptReady;


	/**
	 * Blueprint event for failed generation.
	 */
	UPROPERTY(
		BlueprintAssignable,
		Category = "AI|Prompt Engine"
	)
	FOnPromptGenerationFailed OnPromptGenerationFailed;

private:

	/**
 * Analyzes sanitized document text using deterministic rules
 * and builds a focused prompt for Nazia's Question Engine.
 */
	FString BuildFocusedQuestionGenerationPrompt(
		const FString& SanitizedDocumentText,
		EPromptDocumentType DocumentType
	) const;

	/**
 * Removes personal identity and contact information
 * before CV/resume text is processed.
 */
	FString SanitizeCVResumeText(
		const FString& CVResumeText
	) const;

	bool ValidateExpectedDocumentType(
		const FString& DocumentText,
		EPromptDocumentType ExpectedType,
		FString& OutErrorMessage
	) const;

	bool LoadDocumentTextForMultiFile(
		const FString& FilePath,
		FString& OutExtractedText,
		FString& OutErrorMessage
	) const;

	/**
 * Extracts plain text from a PDF file using Poppler pdftotext.
 */
	bool ExtractTextFromPdf(
		const FString& PdfFilePath,
		FString& OutExtractedText,
		FString& OutErrorMessage
	) const;

	FString GetPdfToTextExecutablePath() const;
};