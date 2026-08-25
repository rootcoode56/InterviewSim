// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FileDialogHelper.generated.h"

/**
 * 
 */
UCLASS()
class INTERVIEWSIM_API UFileDialogHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "File")
	static bool OpenFileDialog(FString& OutFilePath);
	
};
