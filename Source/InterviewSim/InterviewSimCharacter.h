// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InterviewSimCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;

UCLASS()
class INTERVIEWSIM_API AInterviewSimCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AInterviewSimCharacter();

protected:
	virtual void BeginPlay() override;

private:

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UCameraComponent* FollowCamera;
};