// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFlowDebugDriver.generated.h"

class IGameFlowRuleInterface;

/**
 * Optional development-only startup driver that exercises the same public
 * GameFlow boundary as a future gameplay Cargo system.
 */
UCLASS(Blueprintable)
class CH4_MULTIGAME_API AGameFlowDebugDriver : public AActor
{
	GENERATED_BODY()

public:
	AGameFlowDebugDriver();

#if WITH_DEV_AUTOMATION_TESTS
	/** Configures the driver without requiring a Blueprint asset in automation tests. */
	void ConfigureForTesting(
		bool bNewAutoStartDebugGame,
		int32 NewDebugInitialCargoCount,
		IGameFlowRuleInterface* NewGameFlowRuleOverride = nullptr);

	/** Executes the same one-shot startup path used by BeginPlay. */
	bool RunDebugStartupForTesting();

	/** Allows the non-authority guard to be verified without constructing a client world. */
	static bool CanAttemptDebugStartupForTesting(bool bEnabled, bool bHasServerAuthority, int32 InitialCargoCount);
#endif

protected:
	virtual void BeginPlay() override;

	/** When enabled, the server initializes debug Cargo and requests Playing once at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Game Flow|Debug")
	bool bAutoStartDebugGame = false;

	/** Development-only Cargo count supplied through IGameFlowRuleInterface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Game Flow|Debug",
		meta=(EditCondition="bAutoStartDebugGame", ClampMin="1", UIMin="1"))
	int32 DebugInitialCargoCount = 20;

private:
	bool TryStartDebugGame();
	static bool CanAttemptDebugStartup(bool bEnabled, bool bHasServerAuthority, int32 InitialCargoCount);

#if WITH_DEV_AUTOMATION_TESTS
	IGameFlowRuleInterface* GameFlowRuleOverrideForTesting = nullptr;
#endif
};
