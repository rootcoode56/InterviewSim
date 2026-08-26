// Fill out your copyright notice in the Description page of Project Settings.


#include "OpenAIQuestionEngine.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

bool UOpenAIQuestionEngine::LoadGeminiApiKeyFromConfig()
{
	const FString ConfigPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT(
			"ThirdParty/InterviewEvalServer/"
			"evaluation_config.json"
		)
	);

	FString ConfigText;

	if (!FFileHelper::LoadFileToString(ConfigText, *ConfigPath))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Could not load Gemini configuration file: %s"
			),
			*ConfigPath
		);

		return false;
	}

	TSharedPtr<FJsonObject> ConfigObject;

	TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(ConfigText);

	if (
		!FJsonSerializer::Deserialize(Reader, ConfigObject) ||
		!ConfigObject.IsValid()
		)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Gemini configuration file contains invalid JSON."
			)
		);

		return false;
	}

	FString LoadedApiKey;

	if (
		!ConfigObject->TryGetStringField(
			TEXT("gemini_api_key"),
			LoadedApiKey
		)
		)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Gemini configuration does not contain "
				"the gemini_api_key field."
			)
		);

		return false;
	}

	LoadedApiKey.TrimStartAndEndInline();

	if (LoadedApiKey.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Gemini configuration contains an empty API key."
			)
		);

		return false;
	}

	GeminiApiKey = LoadedApiKey;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Gemini API key loaded from external configuration."
		)
	);

	return true;
}

bool UOpenAIQuestionEngine::LoadOpenRouterApiKeyFromConfig()
{
	const FString ConfigPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT(
			"ThirdParty/InterviewEvalServer/"
			"evaluation_config.json"
		)
	);

	FString ConfigText;

	if (!FFileHelper::LoadFileToString(ConfigText, *ConfigPath))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Could not load OpenRouter configuration file: %s"
			),
			*ConfigPath
		);

		return false;
	}

	TSharedPtr<FJsonObject> ConfigObject;

	TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(ConfigText);

	if (
		!FJsonSerializer::Deserialize(Reader, ConfigObject) ||
		!ConfigObject.IsValid()
		)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"OpenRouter configuration file contains invalid JSON."
			)
		);

		return false;
	}

	FString LoadedApiKey;

	if (
		!ConfigObject->TryGetStringField(
			TEXT("openrouter_api_key"),
			LoadedApiKey
		)
		)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"OpenRouter configuration does not contain "
				"the openrouter_api_key field."
			)
		);

		return false;
	}

	LoadedApiKey.TrimStartAndEndInline();

	if (LoadedApiKey.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"OpenRouter configuration contains an empty API key."
			)
		);

		return false;
	}

	OpenRouterApiKey = LoadedApiKey;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"OpenRouter API key loaded from external configuration."
		)
	);

	return true;
}

void UOpenAIQuestionEngine::GenerateQuestionsFromPrompt(
	const FString& JobPrompt,
	FOnQuestionsGenerated OnComplete
)
{
	// A completed generation cycle may be followed by a later
	// regeneration request. Reset the protection state only when
	// the previous cycle has already finished.
	if (bQuestionsAlreadyReturned)
	{
		bQuestionsAlreadyReturned = false;
		GeminiRetryCount = 0;

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Starting a new question-generation cycle. "
				"Generation state reset."
			)
		);
	}

	if (!LoadGeminiApiKeyFromConfig())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Gemini API key could not be loaded. "
				"Skipping Gemini and using the fallback path."
			)
		);

		GenerateQuestionsFromOpenRouter(
			JobPrompt,
			OnComplete
		);

		return;
	}

	TSharedRef<IHttpRequest> Request =
		FHttpModule::Get().CreateRequest();

	const FString Url = TEXT(
		"https://generativelanguage.googleapis.com/v1beta/"
		"models/gemini-3.6-flash:generateContent"
	);

	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));

	Request->SetHeader(
		TEXT("Content-Type"),
		TEXT("application/json")
	);

	Request->SetHeader(
		TEXT("x-goog-api-key"),
		GeminiApiKey
	);

	const FString Prompt = FString::Printf(
		TEXT(
			"Generate 10 interview questions from this job prompt.\n"
			"Return ONLY valid JSON. No markdown.\n"
			"Format: "
			"{\"questions\":[{\"question\":\"...\","
			"\"stage\":\"Technical\","
			"\"difficulty\":\"Medium\","
			"\"score\":8.5}]}\n"
			"Stages must be Warmup, Technical, Behavioral, or Closing.\n"
			"Difficulty must be Easy, Medium, or Hard.\n"
			"Score must be between 1 and 10.\n\n"
			"Job Prompt:\n%s"
		),
		*JobPrompt
	);

	TSharedPtr<FJsonObject> RootObject =
		MakeShareable(new FJsonObject);

	TArray<TSharedPtr<FJsonValue>> ContentsArray;

	TSharedPtr<FJsonObject> ContentObject =
		MakeShareable(new FJsonObject);

	TArray<TSharedPtr<FJsonValue>> PartsArray;

	TSharedPtr<FJsonObject> TextPart =
		MakeShareable(new FJsonObject);

	TextPart->SetStringField(
		TEXT("text"),
		Prompt
	);

	PartsArray.Add(
		MakeShareable(
			new FJsonValueObject(TextPart)
		)
	);

	ContentObject->SetArrayField(
		TEXT("parts"),
		PartsArray
	);

	ContentsArray.Add(
		MakeShareable(
			new FJsonValueObject(ContentObject)
		)
	);

	RootObject->SetArrayField(
		TEXT("contents"),
		ContentsArray
	);

	FString Body;

	TSharedRef<TJsonWriter<>> Writer =
		TJsonWriterFactory<>::Create(&Body);

	FJsonSerializer::Serialize(
		RootObject.ToSharedRef(),
		Writer
	);

	Request->SetContentAsString(Body);

	Request->OnProcessRequestComplete().BindLambda(
		[this, OnComplete, JobPrompt](
			FHttpRequestPtr Req,
			FHttpResponsePtr Response,
			bool bSuccess
			)
		{
			// Another valid source may already have completed this
			// generation cycle.
			if (bQuestionsAlreadyReturned)
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT(
						"Gemini response ignored because "
						"questions were already returned."
					)
				);
				return;
			}

			TArray<FInterviewQuestion> ParsedQuestions;

			if (bSuccess && Response.IsValid())
			{
				const FString ResponseString =
					Response->GetContentAsString();

				UE_LOG(
					LogTemp,
					Warning,
					TEXT("Gemini Response Code: %d"),
					Response->GetResponseCode()
				);

				UE_LOG(
					LogTemp,
					Warning,
					TEXT(
						"Gemini response received. "
						"Body length: %d characters."
					),
					ResponseString.Len()
				);

				if (
					Response->GetResponseCode() >= 200 &&
					Response->GetResponseCode() < 300
					)
				{
					ParsedQuestions =
						ParseQuestionsFromResponse(
							ResponseString
						);

					if (ParsedQuestions.Num() > 0)
					{
						bQuestionsAlreadyReturned = true;

						OnComplete.ExecuteIfBound(
							ParsedQuestions
						);

						return;
					}
				}
			}
			else
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT(
						"Gemini HTTP request failed or "
						"returned no valid response."
					)
				);
			}

			// Retry Gemini sequentially. Do not start OpenRouter
			// while another Gemini retry is still pending.
			if (GeminiRetryCount < MaxGeminiRetries)
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT(
						"Gemini failed or returned no usable "
						"questions. Retrying before fallback."
					)
				);

				RetryGeminiRequest(
					JobPrompt,
					OnComplete
				);

				return;
			}

			// All Gemini attempts have failed. Start exactly one
			// OpenRouter request, which will use the local fallback
			// if OpenRouter also fails.
			UE_LOG(
				LogTemp,
				Error,
				TEXT(
					"Gemini max retry reached. "
					"Calling OpenRouter fallback once."
				)
			);

			GenerateQuestionsFromOpenRouter(
				JobPrompt,
				OnComplete
			);
		}
	);

	Request->ProcessRequest();
}

FString UOpenAIQuestionEngine::NormalizeStageValue(const FString& RawStage) const
{
	FString CleanedStage = RawStage.ToLower();
	CleanedStage.TrimStartAndEndInline();

	CleanedStage.ReplaceInline(TEXT("-"), TEXT(""));
	CleanedStage.ReplaceInline(TEXT("_"), TEXT(""));
	CleanedStage.ReplaceInline(TEXT(" "), TEXT(""));

	if (CleanedStage.Contains(TEXT("warm")) || CleanedStage.Contains(TEXT("intro")) || CleanedStage.Contains(TEXT("greeting")))
	{
		return TEXT("Warmup");
	}

	if (CleanedStage.Contains(TEXT("tech")) || CleanedStage.Contains(TEXT("coding")) || CleanedStage.Contains(TEXT("programming")))
	{
		return TEXT("Technical");
	}

	if (CleanedStage.Contains(TEXT("behavior")) || CleanedStage.Contains(TEXT("behaviour")) || CleanedStage.Contains(TEXT("soft")) || CleanedStage.Contains(TEXT("hr")))
	{
		return TEXT("Behavioral");
	}

	if (CleanedStage.Contains(TEXT("closing")) || CleanedStage.Contains(TEXT("final")) || CleanedStage.Contains(TEXT("summary")))
	{
		return TEXT("Closing");
	}

	return TEXT("Technical");
}

FString UOpenAIQuestionEngine::NormalizeDifficultyValue(const FString& RawDifficulty) const
{
	FString CleanedDifficulty = RawDifficulty.ToLower();
	CleanedDifficulty.TrimStartAndEndInline();

	if (CleanedDifficulty.Contains(TEXT("easy")) || CleanedDifficulty.Contains(TEXT("beginner")) || CleanedDifficulty.Contains(TEXT("basic")) || CleanedDifficulty.Contains(TEXT("junior")))
	{
		return TEXT("Easy");
	}

	if (CleanedDifficulty.Contains(TEXT("medium")) || CleanedDifficulty.Contains(TEXT("intermediate")) || CleanedDifficulty.Contains(TEXT("moderate")))
	{
		return TEXT("Medium");
	}

	if (CleanedDifficulty.Contains(TEXT("hard")) || CleanedDifficulty.Contains(TEXT("advanced")) || CleanedDifficulty.Contains(TEXT("senior")) || CleanedDifficulty.Contains(TEXT("difficult")))
	{
		return TEXT("Hard");
	}

	return TEXT("Medium");
}

float UOpenAIQuestionEngine::NormalizeScoreValue(float RawScore) const
{
	return FMath::Clamp(RawScore, 1.0f, 10.0f);
}


TArray<FInterviewQuestion> UOpenAIQuestionEngine::CreateLocalFallbackQuestions(
	const FString& JobPrompt
) const
{
	TArray<FInterviewQuestion> FallbackQuestions;

	FString CleanPrompt = JobPrompt;
	CleanPrompt.TrimStartAndEndInline();

	FString DocumentType;
	FString DetectedField;

	TArray<FString> DetectedTopics;
	TArray<FString> EvidenceItems;
	TArray<FString> PracticeProfileLines;

	enum class ELocalPromptSection
	{
		None,
		DocumentType,
		Field,
		Topics,
		Evidence,
		PracticeProfile
	};

	ELocalPromptSection CurrentSection =
		ELocalPromptSection::None;

	auto CleanExtractedText =
		[](FString Value) -> FString
		{
			Value.TrimStartAndEndInline();

			while (
				Value.StartsWith(TEXT("-")) ||
				Value.StartsWith(TEXT("*"))
				)
			{
				Value = Value.Mid(1);
				Value.TrimStartAndEndInline();
			}

			Value.ReplaceInline(TEXT("\""), TEXT(""));
			Value.ReplaceInline(TEXT("["), TEXT(""));
			Value.ReplaceInline(TEXT("]"), TEXT(""));

			Value.TrimStartAndEndInline();

			while (
				Value.EndsWith(TEXT(",")) ||
				Value.EndsWith(TEXT(";"))
				)
			{
				Value = Value.LeftChop(1);
				Value.TrimStartAndEndInline();
			}

			return Value;
		};

	auto AddUniqueText =
		[&CleanExtractedText](
			TArray<FString>& TargetArray,
			const FString& RawValue,
			const int32 MaximumItems
			)
		{
			if (TargetArray.Num() >= MaximumItems)
			{
				return;
			}

			FString CleanValue =
				CleanExtractedText(RawValue);

			if (
				CleanValue.IsEmpty() ||
				CleanValue.Len() < 2
				)
			{
				return;
			}

			for (const FString& Existing : TargetArray)
			{
				if (
					Existing.Equals(
						CleanValue,
						ESearchCase::IgnoreCase
					)
					)
				{
					return;
				}
			}

			TargetArray.Add(CleanValue);
		};

	const bool bIsMultiDocumentPrompt =
		CleanPrompt.Contains(
			TEXT("MULTI-DOCUMENT REAL INTERVIEW CONTEXT"),
			ESearchCase::IgnoreCase
		);

	int32 EvidenceSectionCount = 0;

	TArray<FString> PromptLines;

	CleanPrompt.ParseIntoArrayLines(
		PromptLines,
		false
	);

	for (const FString& RawLine : PromptLines)
	{
		FString Line = RawLine;
		Line.TrimStartAndEndInline();

		if (Line.IsEmpty())
		{
			continue;
		}

		if (
			Line.Equals(
				TEXT("SELECTED SOURCE EVIDENCE:"),
				ESearchCase::IgnoreCase
			)
			)
		{
			CurrentSection =
				ELocalPromptSection::Evidence;

			if (bIsMultiDocumentPrompt)
			{
				++EvidenceSectionCount;
			}

			continue;
		}

		if (
			Line.Equals(
				TEXT("RULE-BASED DETECTED FIELD:"),
				ESearchCase::IgnoreCase
			)
			)
		{
			CurrentSection =
				ELocalPromptSection::Field;

			continue;
		}

		if (
			Line.Equals(
				TEXT("DOCUMENTED TOPICS AND EXPERTISE:"),
				ESearchCase::IgnoreCase
			)
			)
		{
			CurrentSection =
				ELocalPromptSection::Topics;

			continue;
		}

		if (
			Line.Equals(
				TEXT("SELECTED SOURCE EVIDENCE:"),
				ESearchCase::IgnoreCase
			)
			)
		{
			CurrentSection =
				ELocalPromptSection::Evidence;

			continue;
		}

		if (
			Line.Equals(
				TEXT(
					"The candidate gave this "
					"introduction/background:"
				),
				ESearchCase::IgnoreCase
			)
			)
		{
			CurrentSection =
				ELocalPromptSection::PracticeProfile;

			continue;
		}

		if (
			Line.StartsWith(
				TEXT("GENERATION RULES:"),
				ESearchCase::IgnoreCase
			)
			)
		{
			CurrentSection =
				ELocalPromptSection::None;

			continue;
		}

		if (
			Line.StartsWith(
				TEXT(
					"Use this real candidate information"
				),
				ESearchCase::IgnoreCase
			)
			)
		{
			CurrentSection =
				ELocalPromptSection::None;

			continue;
		}

		switch (CurrentSection)
		{
		case ELocalPromptSection::DocumentType:
		{
			if (DocumentType.IsEmpty())
			{
				DocumentType =
					CleanExtractedText(Line);

				CurrentSection =
					ELocalPromptSection::None;
			}

			break;
		}

		case ELocalPromptSection::Field:
		{
			if (DetectedField.IsEmpty())
			{
				DetectedField =
					CleanExtractedText(Line);

				if (
					DetectedField.Contains(
						TEXT("unknown"),
						ESearchCase::IgnoreCase
					)
					)
				{
					DetectedField.Empty();
				}

				CurrentSection =
					ELocalPromptSection::None;
			}

			break;
		}

		case ELocalPromptSection::Topics:
		{
			if (
				Line.Contains(
					TEXT("No predefined topic was detected"),
					ESearchCase::IgnoreCase
				)
				)
			{
				CurrentSection =
					ELocalPromptSection::None;

				break;
			}

			TArray<FString> SplitTopics;

			Line.ParseIntoArray(
				SplitTopics,
				TEXT(","),
				true
			);

			for (const FString& Topic : SplitTopics)
			{
				AddUniqueText(
					DetectedTopics,
					Topic,
					10
				);
			}

			CurrentSection =
				ELocalPromptSection::None;

			break;
		}

		case ELocalPromptSection::Evidence:
		{
			FString CleanEvidence =
				CleanExtractedText(Line);

			if (
				!CleanEvidence.IsEmpty() &&
				!CleanEvidence.EndsWith(TEXT(":")) &&
				!CleanEvidence.Equals(
					TEXT(
						"No high-confidence evidence "
						"lines were selected."
					),
					ESearchCase::IgnoreCase
				)
				)
			{
				const int32 MaximumEvidenceItems =
					bIsMultiDocumentPrompt &&
					EvidenceSectionCount == 1
					? 5
					: 10;

				AddUniqueText(
					EvidenceItems,
					CleanEvidence,
					MaximumEvidenceItems
				);
			}

			break;
		}

		case ELocalPromptSection::PracticeProfile:
		{
			AddUniqueText(
				PracticeProfileLines,
				Line,
				8
			);

			break;
		}

		default:
			break;
		}
	}

	const bool bIsPracticePrompt =
		CleanPrompt.Contains(
			TEXT("Practice Interview"),
			ESearchCase::IgnoreCase
		) ||
		PracticeProfileLines.Num() > 0;

	if (bIsPracticePrompt)
	{
		const TArray<FString> PracticeTopicMarkers =
		{
			TEXT("department of"),
			TEXT("majoring in"),
			TEXT("major in"),
			TEXT("specializing in"),
			TEXT("specialized in"),
			TEXT("experience in"),
			TEXT("experienced in"),
			TEXT("worked with"),
			TEXT("working with")
		};

		for (const FString& ProfileLine : PracticeProfileLines)
		{
			for (const FString& Marker : PracticeTopicMarkers)
			{
				const int32 MarkerIndex =
					ProfileLine.Find(
						Marker,
						ESearchCase::IgnoreCase
					);

				if (MarkerIndex == INDEX_NONE)
				{
					continue;
				}

				FString Topic =
					ProfileLine.Mid(
						MarkerIndex + Marker.Len()
					);

				int32 EndIndex = INDEX_NONE;

				if (Topic.FindChar(TEXT(','), EndIndex))
				{
					Topic = Topic.Left(EndIndex);
				}

				if (Topic.FindChar(TEXT('.'), EndIndex))
				{
					Topic = Topic.Left(EndIndex);
				}

				AddUniqueText(
					DetectedTopics,
					Topic,
					10
				);
			}
		}
	}

	const bool bIsJobCircular =
		DocumentType.Contains(
			TEXT("Job Circular"),
			ESearchCase::IgnoreCase
		);

	const bool bHasDocumentGrounding =
		DetectedTopics.Num() > 0 ||
		EvidenceItems.Num() > 0 ||
		!DetectedField.IsEmpty();

	// ---------------------------------------------------------
	// Build a smaller set of role-specific topics.
	//
	// Generic soft-skill labels are valid behavioral concepts,
	// but they should not dominate technical fallback questions.
	// ---------------------------------------------------------

	TArray<FString> RoleSpecificTopics;

	for (const FString& Topic : DetectedTopics)
	{
		const bool bGenericSoftSkill =
			Topic.Equals(
				TEXT("Communication"),
				ESearchCase::IgnoreCase
			) ||
			Topic.Equals(
				TEXT("Problem Solving"),
				ESearchCase::IgnoreCase
			) ||
			Topic.Equals(
				TEXT("Leadership"),
				ESearchCase::IgnoreCase
			) ||
			Topic.Equals(
				TEXT("Teamwork"),
				ESearchCase::IgnoreCase
			);

		if (!bGenericSoftSkill)
		{
			RoleSpecificTopics.AddUnique(
				Topic
			);
		}
	}

	// ---------------------------------------------------------
	// Helpers for safely selecting grounded topics/evidence.
	//
	// IMPORTANT:
	// Topics are no longer recycled with modulo.
	// Once unique role-specific topics are exhausted, an empty
	// topic is returned so the question uses source evidence.
	// ---------------------------------------------------------

	auto GetTopic =
		[&RoleSpecificTopics](
			const int32 Index
			) -> FString
		{
			if (
				Index < 0 ||
				Index >= RoleSpecificTopics.Num()
				)
			{
				return FString();
			}

			return RoleSpecificTopics[Index];
		};

	auto GetEvidence =
		[&EvidenceItems](
			const int32 Index
			) -> FString
		{
			if (EvidenceItems.IsEmpty())
			{
				return FString();
			}

			FString Evidence =
				EvidenceItems[
					Index % EvidenceItems.Num()
				];

			if (Evidence.Len() > 180)
			{
				Evidence = Evidence.Left(180);
				Evidence += TEXT("...");
			}

			return Evidence;
		};

	auto AddQuestion =
		[&FallbackQuestions](
			const FString& QuestionText,
			const FString& Stage,
			const FString& Difficulty,
			const float Score
			)
		{
			FInterviewQuestion Q;

			Q.QuestionText = QuestionText;
			Q.Stage = Stage;
			Q.Difficulty = Difficulty;
			Q.Score = Score;
			Q.SourceModel = TEXT("LocalFallback");
			Q.bUsed = false;

			FallbackQuestions.Add(Q);
		};

	// ---------------------------------------------------------
	// CASE 1:
	// Real Interview with locally filtered document context.
	// ---------------------------------------------------------

	if (!bIsPracticePrompt && bHasDocumentGrounding)
	{
		const FString Topic1 = GetTopic(0);
		const FString Topic2 = GetTopic(1);
		const FString Topic3 = GetTopic(2);
		const FString Topic4 = GetTopic(3);
		const FString Topic5 = GetTopic(4);

		const FString Evidence1 = GetEvidence(0);
		const FString Evidence2 = GetEvidence(1);
		const FString Evidence3 = GetEvidence(2);
		const FString Evidence4 = GetEvidence(3);
		const FString Evidence5 = GetEvidence(4);

		// ---------------- Warmup: 2 Easy ----------------

		if (!DetectedField.IsEmpty())
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"What interested you in the %s field, "
						"and which part of your background best "
						"prepared you for this kind of work?"
					),
					*DetectedField
				),
				TEXT("Warmup"),
				TEXT("Easy"),
				6.0f
			);
		}
		else
		{
			AddQuestion(
				TEXT(
					"What interested you in this professional "
					"area, and which part of your background "
					"best prepared you for it?"
				),
				TEXT("Warmup"),
				TEXT("Easy"),
				6.0f
			);
		}

		if (!Topic1.IsEmpty())
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"Can you describe your experience or "
						"understanding of %s and explain how it "
						"relates to the role you are pursuing?"
					),
					*Topic1
				),
				TEXT("Warmup"),
				TEXT("Easy"),
				6.5f
			);
		}
		else if (!Evidence1.IsEmpty())
		{
			const FString Prefix =
				bIsJobCircular
				? TEXT("The role description highlights")
				: TEXT("Your background mentions");

			AddQuestion(
				FString::Printf(
					TEXT(
						"%s \"%s\". Can you explain how this "
						"area relates to the position?"
					),
					*Prefix,
					*Evidence1
				),
				TEXT("Warmup"),
				TEXT("Easy"),
				6.5f
			);
		}
		else
		{
			AddQuestion(
				TEXT(
					"Which part of your education, experience, "
					"or training is most relevant to this role?"
				),
				TEXT("Warmup"),
				TEXT("Easy"),
				6.5f
			);
		}

		// ---------------- Technical: 2 Easy ----------------

		if (!Topic1.IsEmpty())
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"What fundamental principles do you "
						"consider when working with %s?"
					),
					*Topic1
				),
				TEXT("Technical"),
				TEXT("Easy"),
				7.0f
			);
		}
		else
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"The document highlights \"%s\". "
						"What basic knowledge or judgment is "
						"important in this area?"
					),
					*Evidence1
				),
				TEXT("Technical"),
				TEXT("Easy"),
				7.0f
			);
		}

		if (!Topic2.IsEmpty())
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"What common challenges can arise when "
						"working with %s, and how would you "
						"address them?"
					),
					*Topic2
				),
				TEXT("Technical"),
				TEXT("Easy"),
				7.0f
			);
		}
		else
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"The document highlights \"%s\". "
						"What common challenges could arise in "
						"this area, and how would you handle them?"
					),
					*Evidence2
				),
				TEXT("Technical"),
				TEXT("Easy"),
				7.0f
			);
		}

		// ---------------- Technical: 2 Medium ----------------

		if (!Topic3.IsEmpty())
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"How would you approach a practical task "
						"involving %s from planning through "
						"completion?"
					),
					*Topic3
				),
				TEXT("Technical"),
				TEXT("Medium"),
				8.0f
			);
		}
		else
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"Consider this documented area: \"%s\". "
						"How would you approach a practical task "
						"involving it from planning through "
						"completion?"
					),
					*Evidence3
				),
				TEXT("Technical"),
				TEXT("Medium"),
				8.0f
			);
		}

		if (!Topic4.IsEmpty())
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"How would you evaluate, troubleshoot, "
						"or improve a difficult situation "
						"involving %s?"
					),
					*Topic4
				),
				TEXT("Technical"),
				TEXT("Medium"),
				8.0f
			);
		}
		else
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"Consider this documented area: \"%s\". "
						"How would you evaluate a problem and "
						"decide on an appropriate solution?"
					),
					*Evidence4
				),
				TEXT("Technical"),
				TEXT("Medium"),
				8.0f
			);
		}

		// ---------------- Technical: 1 Hard ----------------

		if (!Topic5.IsEmpty())
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"Suppose you were responsible for a "
						"complex, high-impact task involving %s. "
						"How would you make decisions, manage "
						"risks, and verify the final outcome?"
					),
					*Topic5
				),
				TEXT("Technical"),
				TEXT("Hard"),
				9.0f
			);
		}
		else
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"Suppose you were responsible for a "
						"complex task related to \"%s\". "
						"How would you make decisions, manage "
						"risks, and verify the final outcome?"
					),
					*Evidence5
				),
				TEXT("Technical"),
				TEXT("Hard"),
				9.0f
			);
		}

		// ---------------- Behavioral: 3 Medium ----------------

		if (!Topic1.IsEmpty())
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"Describe a challenging situation "
						"involving %s. What did you do, and "
						"what did you learn from it?"
					),
					*Topic1
				),
				TEXT("Behavioral"),
				TEXT("Medium"),
				7.5f
			);
		}
		else
		{
			AddQuestion(
				TEXT(
					"Describe a challenging situation related "
					"to your field. What did you do, and what "
					"did you learn from it?"
				),
				TEXT("Behavioral"),
				TEXT("Medium"),
				7.5f
			);
		}

		if (!Topic2.IsEmpty())
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"Tell me about a time you had to "
						"collaborate with others while working "
						"on something involving %s. How did you "
						"coordinate the work?"
					),
					*Topic2
				),
				TEXT("Behavioral"),
				TEXT("Medium"),
				7.5f
			);
		}
		else
		{
			AddQuestion(
				TEXT(
					"Tell me about a time you had to collaborate "
					"with others on an important task. How did "
					"you coordinate the work?"
				),
				TEXT("Behavioral"),
				TEXT("Medium"),
				7.5f
			);
		}

		AddQuestion(
			TEXT(
				"Describe a time you had to learn something "
				"new, adapt your approach, or respond to "
				"constructive feedback in your field. "
				"How did you apply what you learned?"
			),
			TEXT("Behavioral"),
			TEXT("Medium"),
			7.5f
		);
	}
	else if (bIsPracticePrompt)
	{
		const FString PracticeTopic1 = GetTopic(0);
		const FString PracticeTopic2 = GetTopic(1);

		if (!PracticeTopic1.IsEmpty())
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"You mentioned %s in your background. "
						"What interested you in this area, and how "
						"does it relate to the kind of work you want "
						"to pursue?"
					),
					*PracticeTopic1
				),
				TEXT("Warmup"),
				TEXT("Easy"),
				6.0f
			);
		}
		else
		{
			AddQuestion(
				TEXT(
					"Which part of the background you described "
					"best represents the kind of work or field you "
					"want to focus on?"
				),
				TEXT("Warmup"),
				TEXT("Easy"),
				6.0f
			);
		}

		if (!PracticeTopic2.IsEmpty())
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"You also mentioned %s. Can you briefly explain "
						"your experience or understanding of this area "
						"and why it is important to you?"
					),
					*PracticeTopic2
				),
				TEXT("Warmup"),
				TEXT("Easy"),
				6.5f
			);
		}
		else
		{
			AddQuestion(
				TEXT(
					"Which skill, subject, responsibility, or "
					"experience from your background are you most "
					"confident discussing, and why?"
				),
				TEXT("Warmup"),
				TEXT("Easy"),
				6.5f
			);
		}

		if (!PracticeTopic1.IsEmpty())
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"What are the fundamental principles or core "
						"concepts you consider important when working "
						"with %s?"
					),
					*PracticeTopic1
				),
				TEXT("Technical"),
				TEXT("Easy"),
				7.0f
			);
		}
		else
		{
			AddQuestion(
				TEXT(
					"Choose one important skill or area from the "
					"background you described. What are its basic "
					"principles?"
				),
				TEXT("Technical"),
				TEXT("Easy"),
				7.0f
			);
		}

			if (!PracticeTopic2.IsEmpty())
			{
				AddQuestion(
					FString::Printf(
						TEXT(
							"What common problems or challenges can arise "
							"when working with %s, and how would you "
							"approach solving them?"
						),
						*PracticeTopic2
					),
					TEXT("Technical"),
					TEXT("Easy"),
					7.0f
				);
			}
			else
			{
				AddQuestion(
					TEXT(
						"What is a common problem or challenge in one "
						"of the professional or technical areas you "
						"mentioned, and how would you address it?"
					),
					TEXT("Technical"),
					TEXT("Easy"),
					7.0f
				);
			}

		if (!PracticeTopic1.IsEmpty())
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"Suppose you were given a practical task "
						"involving %s. How would you approach it "
						"from planning through completion?"
					),
					*PracticeTopic1
				),
				TEXT("Technical"),
				TEXT("Medium"),
				8.0f
			);
		}
		else
		{
			AddQuestion(
				TEXT(
					"Choose one area from your stated background. "
					"How would you approach a practical task in "
					"that area from planning through completion?"
				),
				TEXT("Technical"),
				TEXT("Medium"),
				8.0f
			);
		}

		if (!PracticeTopic2.IsEmpty())
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"How would you analyze and solve a difficult "
						"problem involving %s?"
					),
					*PracticeTopic2
				),
				TEXT("Technical"),
				TEXT("Medium"),
				8.0f
			);
		}
		else
		{
			AddQuestion(
				TEXT(
					"Describe how you would analyze and solve a "
					"difficult problem related to one of the main "
					"areas you mentioned in your background."
				),
				TEXT("Technical"),
				TEXT("Medium"),
				8.0f
			);
		}

		if (!PracticeTopic1.IsEmpty())
		{
			AddQuestion(
				FString::Printf(
					TEXT(
						"Imagine you are responsible for a complex, "
						"high-impact task involving %s. How would you "
						"make decisions, manage risks, and verify the "
						"quality of the final outcome?"
					),
					*PracticeTopic1
				),
				TEXT("Technical"),
				TEXT("Hard"),
				9.0f
			);
		}
		else
		{
			AddQuestion(
				TEXT(
					"Imagine you are responsible for a complex and "
					"high-impact task in your stated field. "
					"How would you make decisions, manage risks, "
					"and verify the quality of the outcome?"
				),
				TEXT("Technical"),
				TEXT("Hard"),
				9.0f
			);
		}

		AddQuestion(
			TEXT(
				"Describe a challenging situation from your "
				"academic, professional, or project experience. "
				"How did you handle it?"
			),
			TEXT("Behavioral"),
			TEXT("Medium"),
			7.5f
		);

		AddQuestion(
			TEXT(
				"Tell me about a time you worked with other "
				"people to complete an important task. "
				"How did you communicate and coordinate?"
			),
			TEXT("Behavioral"),
			TEXT("Medium"),
			7.5f
		);

		AddQuestion(
			TEXT(
				"Tell me about a time you received feedback or "
				"had to learn something new. How did you apply "
				"what you learned?"
			),
			TEXT("Behavioral"),
			TEXT("Medium"),
			7.5f
		);
	}
	else
	{
		AddQuestion(
			TEXT(
				"What interested you in this opportunity, "
				"and what part of your background is most "
				"relevant to it?"
			),
			TEXT("Warmup"),
			TEXT("Easy"),
			6.0f
		);

		AddQuestion(
			TEXT(
				"Can you describe an academic, professional, "
				"or personal experience that helped prepare "
				"you for this kind of role?"
			),
			TEXT("Warmup"),
			TEXT("Easy"),
			6.5f
		);

		AddQuestion(
			TEXT(
				"What are the most important basic principles "
				"you apply when working in your field?"
			),
			TEXT("Technical"),
			TEXT("Easy"),
			7.0f
		);

		AddQuestion(
			TEXT(
				"What is a common challenge in your field, "
				"and how would you normally approach it?"
			),
			TEXT("Technical"),
			TEXT("Easy"),
			7.0f
		);

		AddQuestion(
			TEXT(
				"How would you plan and complete a practical "
				"task in your field while maintaining quality?"
			),
			TEXT("Technical"),
			TEXT("Medium"),
			8.0f
		);

		AddQuestion(
			TEXT(
				"How would you investigate and solve a difficult "
				"problem when the correct solution is not "
				"immediately obvious?"
			),
			TEXT("Technical"),
			TEXT("Medium"),
			8.0f
		);

		AddQuestion(
			TEXT(
				"Imagine you are responsible for a complex, "
				"high-impact task in your field. How would you "
				"make decisions, manage risks, and verify the "
				"final outcome?"
			),
			TEXT("Technical"),
			TEXT("Hard"),
			9.0f
		);

		AddQuestion(
			TEXT(
				"Describe a difficult situation you encountered "
				"in a project, academic task, or workplace. "
				"How did you handle it?"
			),
			TEXT("Behavioral"),
			TEXT("Medium"),
			7.5f
		);

		AddQuestion(
			TEXT(
				"Tell me about a time you worked with others "
				"to complete an important task. How did you "
				"coordinate with the team?"
			),
			TEXT("Behavioral"),
			TEXT("Medium"),
			7.5f
		);

		AddQuestion(
			TEXT(
				"Tell me about a time you received constructive "
				"feedback or had to learn something new. "
				"How did you respond?"
			),
			TEXT("Behavioral"),
			TEXT("Medium"),
			7.5f
		);
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Using document-grounded local fallback. "
			"Questions: %d | Document Type: %s | "
			"Detected Field: %s | Topics: %d | "
			"Role Topics: %d | Evidence: %d | "
			"Practice Prompt: %s"
		),
		FallbackQuestions.Num(),
		DocumentType.IsEmpty()
		? TEXT("Unknown")
		: *DocumentType,
		DetectedField.IsEmpty()
		? TEXT("Unknown")
		: *DetectedField,
		DetectedTopics.Num(),
		RoleSpecificTopics.Num(),
		EvidenceItems.Num(),
		bIsPracticePrompt
		? TEXT("Yes")
		: TEXT("No")
	);

	return FallbackQuestions;
}


TArray<FInterviewQuestion> UOpenAIQuestionEngine::ParseQuestionsFromResponse(
	const FString& ResponseString)
{
	TArray<FInterviewQuestion> ResultQuestions;

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(ResponseString);

	if (
		!FJsonSerializer::Deserialize(Reader, RootObject) ||
		!RootObject.IsValid()
		)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to parse Gemini root JSON"));
		return ResultQuestions;
	}

	const TArray<TSharedPtr<FJsonValue>>* CandidatesArray;

	if (
		!RootObject->TryGetArrayField(TEXT("candidates"), CandidatesArray) ||
		CandidatesArray->Num() == 0
		)
	{
		UE_LOG(LogTemp, Error, TEXT("No candidates found in Gemini response"));
		return ResultQuestions;
	}

	TSharedPtr<FJsonObject> CandidateObject =
		(*CandidatesArray)[0]->AsObject();

	if (!CandidateObject.IsValid())
	{
		return ResultQuestions;
	}

	TSharedPtr<FJsonObject> ContentObject =
		CandidateObject->GetObjectField(TEXT("content"));

	if (!ContentObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("No content object found"));
		return ResultQuestions;
	}

	const TArray<TSharedPtr<FJsonValue>>* PartsArray;

	if (
		!ContentObject->TryGetArrayField(TEXT("parts"), PartsArray) ||
		PartsArray->Num() == 0
		)
	{
		UE_LOG(LogTemp, Error, TEXT("No parts found"));
		return ResultQuestions;
	}

	TSharedPtr<FJsonObject> FirstPart =
		(*PartsArray)[0]->AsObject();

	if (!FirstPart.IsValid())
	{
		return ResultQuestions;
	}

	FString ContentText;

	if (!FirstPart->TryGetStringField(TEXT("text"), ContentText))
	{
		UE_LOG(LogTemp, Error, TEXT("No text found in Gemini response"));
		return ResultQuestions;
	}

	ContentText.ReplaceInline(TEXT("```json"), TEXT(""));
	ContentText.ReplaceInline(TEXT("```"), TEXT(""));
	ContentText.TrimStartAndEndInline();

	TSharedPtr<FJsonObject> QuestionsRoot;
	TSharedRef<TJsonReader<>> QuestionReader =
		TJsonReaderFactory<>::Create(ContentText);

	if (
		!FJsonSerializer::Deserialize(QuestionReader, QuestionsRoot) ||
		!QuestionsRoot.IsValid()
		)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Failed to parse generated questions JSON: %s"),
			*ContentText
		);

		return ResultQuestions;
	}

	const TArray<TSharedPtr<FJsonValue>>* QuestionsArray;

	if (
		!QuestionsRoot->TryGetArrayField(TEXT("questions"), QuestionsArray) &&
		!QuestionsRoot->TryGetArrayField(TEXT("Questions"), QuestionsArray)
		)
	{
		UE_LOG(LogTemp, Error, TEXT("No questions array found"));
		return ResultQuestions;
	}

	for (const TSharedPtr<FJsonValue>& Item : *QuestionsArray)
	{
		TSharedPtr<FJsonObject> QuestionObject =
			Item->AsObject();

		if (!QuestionObject.IsValid())
		{
			continue;
		}

		FInterviewQuestion Q;

		FString RawStage;
		FString RawDifficulty;

		// Support both Gemini formats:
		// lowercase: question, stage, difficulty, score
		// PascalCase: QuestionText, Stage, Difficulty, Score
		if (!QuestionObject->TryGetStringField(TEXT("question"), Q.QuestionText))
		{
			if (!QuestionObject->TryGetStringField(TEXT("QuestionText"), Q.QuestionText))
			{
				if (!QuestionObject->TryGetStringField(TEXT("Question"), Q.QuestionText))
				{
					QuestionObject->TryGetStringField(TEXT("questionText"), Q.QuestionText);
				}
			}
		}

		if (!QuestionObject->TryGetStringField(TEXT("stage"), RawStage))
		{
			QuestionObject->TryGetStringField(TEXT("Stage"), RawStage);
		}

		if (!QuestionObject->TryGetStringField(TEXT("difficulty"), RawDifficulty))
		{
			QuestionObject->TryGetStringField(TEXT("Difficulty"), RawDifficulty);
		}

		Q.QuestionText.TrimStartAndEndInline();
		Q.Stage = NormalizeStageValue(RawStage);
		Q.Difficulty = NormalizeDifficultyValue(RawDifficulty);
		Q.SourceModel = TEXT("Gemini");
		Q.bUsed = false;

		double ScoreValue = 5.0;

		if (
			QuestionObject->TryGetNumberField(TEXT("score"), ScoreValue) ||
			QuestionObject->TryGetNumberField(TEXT("Score"), ScoreValue)
			)
		{
			Q.Score =
				NormalizeScoreValue(
					static_cast<float>(ScoreValue)
				);
		}
		else
		{
			Q.Score = NormalizeScoreValue(5.0f);
		}

		if (!Q.QuestionText.IsEmpty())
		{
			ResultQuestions.Add(Q);

			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"Gemini Parsed Question: %s | Stage: %s | "
					"Difficulty: %s | Score: %.2f | Source: %s"
				),
				*Q.QuestionText,
				*Q.Stage,
				*Q.Difficulty,
				Q.Score,
				*Q.SourceModel
			);
		}
		else
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"Gemini question skipped because no question text "
					"field was found."
				)
			);
		}
	}

	return ResultQuestions;
}

void UOpenAIQuestionEngine::GenerateQuestionsFromOpenRouter(
	const FString& JobPrompt,
	FOnQuestionsGenerated OnComplete)
{
	UE_LOG(LogTemp, Warning, TEXT("Calling OpenRouter fallback..."));

	if (!LoadOpenRouterApiKeyFromConfig())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"OpenRouter API key could not be loaded. "
				"Using local fallback questions."
			)
		);

		if (!bQuestionsAlreadyReturned)
		{
			TArray<FInterviewQuestion> LocalFallbackQuestions =
				CreateLocalFallbackQuestions(JobPrompt);

			if (LocalFallbackQuestions.Num() > 0)
			{
				bQuestionsAlreadyReturned = true;

				OnComplete.ExecuteIfBound(
					LocalFallbackQuestions
				);
			}
		}

		return;
	}

	TSharedRef<IHttpRequest> Request =
		FHttpModule::Get().CreateRequest();

	Request->SetURL(TEXT("https://openrouter.ai/api/v1/chat/completions"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *OpenRouterApiKey));
	Request->SetHeader(TEXT("HTTP-Referer"), TEXT("https://interviewsim.local"));
	Request->SetHeader(TEXT("X-Title"), TEXT("InterviewSim"));

	FString Prompt = FString::Printf(TEXT(
		"Generate 10 interview questions from this job prompt.\n"
		"Return ONLY valid JSON. No markdown.\n"
		"Format: {\"questions\":[{\"question\":\"...\",\"stage\":\"Technical\",\"difficulty\":\"Medium\",\"score\":8.5}]}\n"
		"Stages must be Warmup, Technical, Behavioral, or Closing.\n"
		"Difficulty must be Easy, Medium, or Hard.\n"
		"Score must be between 1 and 10.\n\n"
		"Job Prompt:\n%s"
	), *JobPrompt);

	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
	RootObject->SetStringField(
		TEXT("model"),
		TEXT("dots-studio/dots-3-note-preview:free")
	);
	RootObject->SetNumberField(TEXT("temperature"), 0.3);
	RootObject->SetNumberField(TEXT("max_tokens"), 1200);

	TSharedPtr<FJsonObject> ReasoningObject =
		MakeShareable(new FJsonObject);

	ReasoningObject->SetStringField(
		TEXT("effort"),
		TEXT("none")
	);

	RootObject->SetObjectField(
		TEXT("reasoning"),
		ReasoningObject
	);

	TArray<TSharedPtr<FJsonValue>> Messages;

	TSharedPtr<FJsonObject> SystemMessage = MakeShareable(new FJsonObject);
	SystemMessage->SetStringField(TEXT("role"), TEXT("system"));
	SystemMessage->SetStringField(TEXT("content"), TEXT("You are an interview question generation engine. Return only valid JSON."));
	Messages.Add(MakeShareable(new FJsonValueObject(SystemMessage)));

	TSharedPtr<FJsonObject> UserMessage = MakeShareable(new FJsonObject);
	UserMessage->SetStringField(TEXT("role"), TEXT("user"));
	UserMessage->SetStringField(TEXT("content"), Prompt);
	Messages.Add(MakeShareable(new FJsonValueObject(UserMessage)));

	RootObject->SetArrayField(TEXT("messages"), Messages);

	FString Body;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	Request->SetContentAsString(Body);

	Request->OnProcessRequestComplete().BindLambda(
		[this, OnComplete, JobPrompt](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bSuccess)
		{
			if (bQuestionsAlreadyReturned)
			{
				UE_LOG(LogTemp, Warning, TEXT("OpenRouter response ignored because questions already returned."));
				return;
			}

			TArray<FInterviewQuestion> ParsedQuestions;

			if (bSuccess && Response.IsValid())
			{
				FString ResponseString = Response->GetContentAsString();

				UE_LOG(LogTemp, Warning, TEXT("OpenRouter Response Code: %d"), Response->GetResponseCode());
				UE_LOG(LogTemp, Warning, TEXT("OpenRouter Response: %s"), *ResponseString);

				if (Response->GetResponseCode() >= 200 && Response->GetResponseCode() < 300)
				{
					ParsedQuestions = ParseQuestionsFromOpenRouterResponse(ResponseString);

					if (ParsedQuestions.Num() > 0)
					{
						bQuestionsAlreadyReturned = true;
						OnComplete.ExecuteIfBound(ParsedQuestions);
						return;
					}
				}
			}

			UE_LOG(LogTemp, Error, TEXT("OpenRouter fallback failed or returned no usable questions. Using local fallback questions."));

			if (!bQuestionsAlreadyReturned)
			{
				TArray<FInterviewQuestion> LocalFallbackQuestions = CreateLocalFallbackQuestions(JobPrompt);

				if (LocalFallbackQuestions.Num() > 0)
				{
					bQuestionsAlreadyReturned = true;
					OnComplete.ExecuteIfBound(LocalFallbackQuestions);
				}
			}
		}
	);

	Request->ProcessRequest();
}

void UOpenAIQuestionEngine::RetryGeminiRequest(
	const FString& JobPrompt,
	FOnQuestionsGenerated OnComplete)
{
	if (GeminiRetryCount >= MaxGeminiRetries)
	{
		UE_LOG(LogTemp, Error, TEXT("Gemini max retry reached."));
		return;
	}

	GeminiRetryCount++;

	UE_LOG(LogTemp, Warning, TEXT("Retrying Gemini in background. Attempt: %d"), GeminiRetryCount);

	GenerateQuestionsFromPrompt(JobPrompt, OnComplete);
}

TArray<FInterviewQuestion> UOpenAIQuestionEngine::ParseQuestionsFromOpenRouterResponse(
	const FString& ResponseString)
{
	TArray<FInterviewQuestion> ResultQuestions;

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to parse OpenRouter root JSON"));
		return ResultQuestions;
	}

	const TArray<TSharedPtr<FJsonValue>>* ChoicesArray;
	if (!RootObject->TryGetArrayField(TEXT("choices"), ChoicesArray) || ChoicesArray->Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("No choices found in OpenRouter response"));
		return ResultQuestions;
	}

	TSharedPtr<FJsonObject> ChoiceObject = (*ChoicesArray)[0]->AsObject();
	if (!ChoiceObject.IsValid())
	{
		return ResultQuestions;
	}

	TSharedPtr<FJsonObject> MessageObject = ChoiceObject->GetObjectField(TEXT("message"));
	if (!MessageObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("No message object found in OpenRouter response"));
		return ResultQuestions;
	}

	FString ContentText;
	if (!MessageObject->TryGetStringField(TEXT("content"), ContentText))
	{
		UE_LOG(LogTemp, Error, TEXT("No content text found in OpenRouter response"));
		return ResultQuestions;
	}

	ContentText.ReplaceInline(TEXT("```json"), TEXT(""));
	ContentText.ReplaceInline(TEXT("```"), TEXT(""));
	ContentText.TrimStartAndEndInline();

	TSharedPtr<FJsonObject> QuestionsRoot;
	TSharedRef<TJsonReader<>> QuestionReader = TJsonReaderFactory<>::Create(ContentText);

	if (!FJsonSerializer::Deserialize(QuestionReader, QuestionsRoot) || !QuestionsRoot.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to parse OpenRouter questions JSON: %s"), *ContentText);
		return ResultQuestions;
	}

	const TArray<TSharedPtr<FJsonValue>>* QuestionsArray;
	if (!QuestionsRoot->TryGetArrayField(TEXT("questions"), QuestionsArray))
	{
		UE_LOG(LogTemp, Error, TEXT("No questions array found in OpenRouter JSON"));
		return ResultQuestions;
	}

	for (const TSharedPtr<FJsonValue>& Item : *QuestionsArray)
	{
		TSharedPtr<FJsonObject> QuestionObject = Item->AsObject();

		if (!QuestionObject.IsValid())
		{
			continue;
		}

		FInterviewQuestion Q;

		FString RawStage;
		FString RawDifficulty;

		if (
			!QuestionObject->TryGetStringField(
				TEXT("question"),
				Q.QuestionText
			)
			)
		{
			if (
				!QuestionObject->TryGetStringField(
					TEXT("QuestionText"),
					Q.QuestionText
				)
				)
			{
				if (
					!QuestionObject->TryGetStringField(
						TEXT("Question"),
						Q.QuestionText
					)
					)
				{
					QuestionObject->TryGetStringField(
						TEXT("questionText"),
						Q.QuestionText
					);
				}
			}
		}

		if (
			!QuestionObject->TryGetStringField(
				TEXT("stage"),
				RawStage
			)
			)
		{
			QuestionObject->TryGetStringField(
				TEXT("Stage"),
				RawStage
			);
		}

		if (
			!QuestionObject->TryGetStringField(
				TEXT("difficulty"),
				RawDifficulty
			)
			)
		{
			QuestionObject->TryGetStringField(
				TEXT("Difficulty"),
				RawDifficulty
			);
		}

		Q.QuestionText.TrimStartAndEndInline();

		Q.Stage =
			NormalizeStageValue(RawStage);

		Q.Difficulty =
			NormalizeDifficultyValue(RawDifficulty);

		Q.SourceModel = TEXT("OpenRouter");
		Q.bUsed = false;

		double ScoreValue = 5.0;

		if (
			QuestionObject->TryGetNumberField(
				TEXT("score"),
				ScoreValue
			) ||
			QuestionObject->TryGetNumberField(
				TEXT("Score"),
				ScoreValue
			)
			)
		{
			Q.Score =
				NormalizeScoreValue(
					static_cast<float>(ScoreValue)
				);
		}
		else
		{
			Q.Score =
				NormalizeScoreValue(5.0f);
		}

		if (!Q.QuestionText.IsEmpty())
		{
			ResultQuestions.Add(Q);

			UE_LOG(LogTemp, Warning, TEXT("OpenRouter Parsed Question: %s | Stage: %s | Difficulty: %s | Score: %.2f | Source: %s"),
				*Q.QuestionText,
				*Q.Stage,
				*Q.Difficulty,
				Q.Score,
				*Q.SourceModel);
		}
	}

	return ResultQuestions;
}