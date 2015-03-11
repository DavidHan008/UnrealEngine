// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "LocalizationConfigurationScript.h"

namespace LocalizationCommandletExecution
{
	struct FTask
	{
		FTask() {}

		FTask(const FText& InName, const FString& InScriptPath)
			: Name(InName)
			, ScriptPath(InScriptPath)
		{}

		FText Name;
		FString ScriptPath;
	};

	bool Execute(const TSharedRef<SWindow>& ParentWindow, const FText& Title, const TArray<FTask>& Tasks);
};

class FLocalizationCommandletProcess : public TSharedFromThis<FLocalizationCommandletProcess>
{
public:
	static TSharedPtr<FLocalizationCommandletProcess> Execute(const FString& ConfigFilePath);

private:
	FLocalizationCommandletProcess(void* const InReadPipe, void* const InWritePipe, const FProcHandle InProcessHandle, const FString& InProcessArguments)
		: ReadPipe(InReadPipe)
		, WritePipe(InWritePipe)
		, ProcessHandle(InProcessHandle)
		, ProcessArguments(InProcessArguments)
	{
	}

public:
	~FLocalizationCommandletProcess();

	void* GetReadPipe() const
	{
		return ReadPipe;
	}

	FProcHandle& GetHandle()
	{
		return ProcessHandle;
	}

	const FString& GetProcessArguments() const
	{
		return ProcessArguments;
	}

private:
	void* const ReadPipe;
	void* const WritePipe;
	FProcHandle ProcessHandle;
	FString ProcessArguments;
};