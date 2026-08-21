// Fill out your copyright notice in the Description page of Project Settings.

#include "InterviewNetworkSubsystem.h"

#include "Engine/GameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

void UInterviewNetworkSubsystem::Initialize(
	FSubsystemCollectionBase& Collection
)
{
	Super::Initialize(Collection);

	bInternetAvailable = false;
	bNetworkCheckInProgress = false;
	bHasCompletedInitialCheck = false;

	StartNetworkMonitoring();
}

void UInterviewNetworkSubsystem::Deinitialize()
{
	StopNetworkMonitoring();

	Super::Deinitialize();
}

void UInterviewNetworkSubsystem::StartNetworkMonitoring()
{
	UGameInstance* GameInstance = GetGameInstance();

	if (!GameInstance)
	{
		return;
	}

	// Perform the first check immediately.
	CheckInternetConnection();

	// Continue checking once every five seconds.
	GameInstance->GetTimerManager().SetTimer(
		NetworkCheckTimerHandle,
		this,
		&UInterviewNetworkSubsystem::CheckInternetConnection,
		NetworkCheckIntervalSeconds,
		true,
		NetworkCheckIntervalSeconds
	);
}

void UInterviewNetworkSubsystem::StopNetworkMonitoring()
{
	UGameInstance* GameInstance = GetGameInstance();

	if (!GameInstance)
	{
		return;
	}

	GameInstance->GetTimerManager().ClearTimer(
		NetworkCheckTimerHandle
	);
}

void UInterviewNetworkSubsystem::CheckInternetConnection()
{
	// Never allow two connection checks to run simultaneously.
	if (bNetworkCheckInProgress)
	{
		return;
	}

	bNetworkCheckInProgress = true;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();

	Request->SetURL(
		TEXT("https://connectivitycheck.gstatic.com/generate_204")
	);

	Request->SetVerb(TEXT("GET"));
	Request->SetTimeout(5.0f);

	Request->OnProcessRequestComplete().BindUObject(
		this,
		&UInterviewNetworkSubsystem::HandleInternetCheckCompleted
	);

	const bool bRequestStarted = Request->ProcessRequest();

	// The request could not even begin.
	if (!bRequestStarted)
	{
		bNetworkCheckInProgress = false;

		const bool bNewInternetAvailable = false;

		const bool bStatusChanged =
			!bHasCompletedInitialCheck ||
			bInternetAvailable != bNewInternetAvailable;

		bInternetAvailable = bNewInternetAvailable;
		bHasCompletedInitialCheck = true;

		OnInternetCheckCompleted.Broadcast(
			bInternetAvailable
		);

		if (bStatusChanged)
		{
			OnInternetStatusChanged.Broadcast(
				bInternetAvailable
			);
		}
	}
}

void UInterviewNetworkSubsystem::HandleInternetCheckCompleted(
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request,
	TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> Response,
	bool bWasSuccessful
)
{
	bNetworkCheckInProgress = false;

	bool bNewInternetAvailable = false;

	if (bWasSuccessful && Response.IsValid())
	{
		const int32 ResponseCode =
			Response->GetResponseCode();

		bNewInternetAvailable =
			ResponseCode == 204;
	}

	const bool bStatusChanged =
		!bHasCompletedInitialCheck ||
		bInternetAvailable != bNewInternetAvailable;

	bInternetAvailable = bNewInternetAvailable;
	bHasCompletedInitialCheck = true;

	// Broadcast after every completed check.
	OnInternetCheckCompleted.Broadcast(
		bInternetAvailable
	);

	// Broadcast after the first result and whenever status changes.
	if (bStatusChanged)
	{
		OnInternetStatusChanged.Broadcast(
			bInternetAvailable
		);
	}
}

