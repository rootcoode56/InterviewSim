// Fill out your copyright notice in the Description page of Project Settings.


#include "VirtualHumanInterface.h"

void UVirtualHumanInterface::InitializeAvatar()
{
	UE_LOG(LogTemp, Warning, TEXT("Virtual Human Initialized"));
}

void UVirtualHumanInterface::PlayGreetingAnimation()
{
	UE_LOG(LogTemp, Warning, TEXT("Playing Greeting Animation"));
}

void UVirtualHumanInterface::PlayTechnicalExpression()
{
	UE_LOG(LogTemp, Warning, TEXT("Playing Technical Expression"));
}

void UVirtualHumanInterface::OnInterviewStateChanged(EInterviewState NewState)
{
	UE_LOG(LogTemp, Warning, TEXT("Virtual Human notified of state change: %d"), (uint8)NewState);
}
