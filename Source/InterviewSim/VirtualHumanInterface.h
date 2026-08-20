// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "InterviewTypes.h"
#include "VirtualHumanInterface.generated.h"

UCLASS()
class INTERVIEWSIM_API UVirtualHumanInterface : public UObject
{
	GENERATED_BODY()

public:

	void InitializeAvatar();

	void PlayGreetingAnimation();

	void PlayTechnicalExpression();

	void OnInterviewStateChanged(EInterviewState NewState);
};