// Fill out your copyright notice in the Description page of Project Settings.


#include "FileDialogHelper.h"

#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"
#include <commdlg.h>
#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif


bool UFileDialogHelper::OpenFileDialog(FString& OutFilePath)
{
    OutFilePath.Empty();

#if PLATFORM_WINDOWS

    WCHAR FileBuffer[4096] = { 0 };

    const FString InitialDirectory =
        FPaths::ProjectDir();

    OPENFILENAMEW DialogInfo;
    FMemory::Memzero(DialogInfo);

    DialogInfo.lStructSize =
        sizeof(OPENFILENAMEW);

    DialogInfo.hwndOwner =
        nullptr;

    DialogInfo.lpstrFile =
        FileBuffer;

    DialogInfo.nMaxFile =
        UE_ARRAY_COUNT(FileBuffer);

    DialogInfo.lpstrFilter =
        L"Supported Documents (*.pdf;*.txt;*.json)\0"
        L"*.pdf;*.txt;*.json\0"
        L"PDF Files (*.pdf)\0"
        L"*.pdf\0"
        L"Text Files (*.txt)\0"
        L"*.txt\0"
        L"JSON Files (*.json)\0"
        L"*.json\0\0";

    DialogInfo.lpstrInitialDir =
        *InitialDirectory;

    DialogInfo.lpstrTitle =
        L"Select CV, Resume, or Job Circular";

    DialogInfo.Flags =
        OFN_PATHMUSTEXIST |
        OFN_FILEMUSTEXIST |
        OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&DialogInfo))
    {
        return false;
    }

    OutFilePath =
        FString(FileBuffer);

    return !OutFilePath.IsEmpty();

#else

    return false;

#endif
}