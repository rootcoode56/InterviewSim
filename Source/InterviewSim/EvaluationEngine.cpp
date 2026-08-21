// Fill out your copyright notice in the Description page of Project Settings.


#include "EvaluationEngine.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"

void UEvaluationEngine::EvaluateAnswer(
	const FString& Question,
	const FString& Answer)
{
	TSharedRef<IHttpRequest> Request =
		FHttpModule::Get().CreateRequest();

	Request->SetURL("http://127.0.0.1:8000/evaluate");
	Request->SetVerb("POST");
	Request->SetHeader("Content-Type", "application/json");

	FString JsonPayload = FString::Printf(
		TEXT("{\"question\":\"%s\",\"answer\":\"%s\"}"),
		*Question,
		*Answer
	);

	Request->SetContentAsString(JsonPayload);

	Request->OnProcessRequestComplete().BindUObject(
		this,
		&UEvaluationEngine::OnEvaluationResponse
	);

	Request->ProcessRequest();
}

void UEvaluationEngine::OnEvaluationResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	bool bSuccess)
{
	FEvaluationResult Result;

	if (!bSuccess || !Response.IsValid())
	{
		Result.Feedback = TEXT("Evaluation request failed.");
		OnEvaluationCompleted.Broadcast(Result);
		return;
	}

	FString ResponseString = Response->GetContentAsString();
	UE_LOG(LogTemp, Warning, TEXT("Raw Evaluation Response: %s"), *ResponseString);

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(ResponseString);

	if (FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		Result.Score = JsonObject->GetNumberField("score");
		Result.Correctness = JsonObject->GetNumberField("correctness");
		Result.Clarity = JsonObject->GetNumberField("clarity");
		Result.Relevance = JsonObject->GetNumberField("relevance");
		Result.Confidence = JsonObject->GetNumberField("confidence");
		Result.Feedback = JsonObject->GetStringField("feedback");

		const TArray<TSharedPtr<FJsonValue>>* StrengthArray;
		if (JsonObject->TryGetArrayField("strengths", StrengthArray))
		{
			for (TSharedPtr<FJsonValue> Item : *StrengthArray)
			{
				Result.Strengths.Add(Item->AsString());
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* WeaknessArray;
		if (JsonObject->TryGetArrayField("weaknesses", WeaknessArray))
		{
			for (TSharedPtr<FJsonValue> Item : *WeaknessArray)
			{
				Result.Weaknesses.Add(Item->AsString());
			}
		}
	}
	else
	{
		Result.Feedback = TEXT("Failed to parse evaluation response.");
	}

	OnEvaluationCompleted.Broadcast(Result);
}