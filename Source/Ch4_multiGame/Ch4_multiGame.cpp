// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ch4_multiGame.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "GameDelegates.h"
#endif

class FCh4MultiGameModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
#if WITH_EDITOR
		// These widgets load lazily to avoid constructing MVVM UI during native
		// CDO initialization. Include their exact packages without resaving a Blueprint
		// or making unrelated UI directories always cook.
		ModifyCookHandle = FGameDelegates::Get().GetModifyCookDelegate().AddLambda(
			[](TConstArrayView<const ITargetPlatform*>, TArray<FName>& PackagesToCook, TArray<FName>&)
			{
				PackagesToCook.AddUnique(FName(TEXT("/Game/UI/WBP_PauseMenu")));
				PackagesToCook.AddUnique(FName(TEXT("/Game/UI/WBP_HUD")));
				UE_LOG(LogCh4_multiGame, Log, TEXT("[PauseMenuCook] Including /Game/UI/WBP_PauseMenu"));
				UE_LOG(LogCh4_multiGame, Log, TEXT("[HUDCook] Including /Game/UI/WBP_HUD"));
			});
#endif
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
		FGameDelegates::Get().GetModifyCookDelegate().Remove(ModifyCookHandle);
#endif
	}

private:
#if WITH_EDITOR
	FDelegateHandle ModifyCookHandle;
#endif
};

IMPLEMENT_PRIMARY_GAME_MODULE(FCh4MultiGameModule, Ch4_multiGame, "Ch4_multiGame");

DEFINE_LOG_CATEGORY(LogCh4_multiGame)
