// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "InterviewNetworkSubsystem.generated.h"

class IHttpRequest;
class IHttpResponse;

/**
 * Fires whenever an individual internet check finishes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnInternetCheckCompleted,
	bool,
	bIsConnected
);

/**
 * Fires only when:
 * 1. The first internet check finishes, or
 * 2. The connection changes between online and offline.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnInternetStatusChanged,
	bool,
	bIsConnected
);

UCLASS()
class INTERVIEWSIM_API UInterviewNetworkSubsystem
	: public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/**
	 * Automatically called when this subsystem is created.
	 */
	virtual void Initialize(
		FSubsystemCollectionBase& Collection
	) override;

	/**
	 * Automatically called when this subsystem is destroyed.
	 */
	virtual void Deinitialize() override;

	/**
	 * Starts an immediate asynchronous internet check.
	 * The Retry button will also use this function later.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interview|Network")
	void CheckInternetConnection();

	/**
	 * Returns the most recently confirmed connection state.
	 */
	UFUNCTION(BlueprintPure, Category = "Interview|Network")
	bool IsInternetAvailable() const
	{
		return bInternetAvailable;
	}

	/**
	 * Returns true while an internet request is running.
	 */
	UFUNCTION(BlueprintPure, Category = "Interview|Network")
	bool IsNetworkCheckInProgress() const
	{
		return bNetworkCheckInProgress;
	}

	/**
	 * Prevents us from treating the default false value as a confirmed
	 * offline result before the first network check has completed.
	 */
	UFUNCTION(BlueprintPure, Category = "Interview|Network")
	bool HasCompletedInitialCheck() const
	{
		return bHasCompletedInitialCheck;
	}

	/**
	 * Broadcast after every completed internet check.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Interview|Network")
	FOnInternetCheckCompleted OnInternetCheckCompleted;

	/**
	 * Broadcast after the first check and whenever the connection
	 * changes between online and offline.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Interview|Network")
	FOnInternetStatusChanged OnInternetStatusChanged;

private:

	/**
	 * Starts the recurring five-second monitoring timer.
	 */
	void StartNetworkMonitoring();

	/**
	 * Stops and clears the recurring monitoring timer.
	 */
	void StopNetworkMonitoring();

	/**
	 * Handles the asynchronous HTTP response.
	 */
	void HandleInternetCheckCompleted(
		TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request,
		TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> Response,
		bool bWasSuccessful
	);

private:

	/**
	 * Handle for the repeating monitoring timer.
	 */
	FTimerHandle NetworkCheckTimerHandle;

	/**
	 * Internet checks occur once every five seconds.
	 */
	float NetworkCheckIntervalSeconds = 5.0f;

	/**
	 * Most recently confirmed connection state.
	 */
	bool bInternetAvailable = false;

	/**
	 * Prevents multiple HTTP checks from running simultaneously.
	 */
	bool bNetworkCheckInProgress = false;

	/**
	 * Becomes true after the first check finishes.
	 */
	bool bHasCompletedInitialCheck = false;
};