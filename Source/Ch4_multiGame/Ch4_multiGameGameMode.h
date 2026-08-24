// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Ch4_multiGameGameMode.generated.h"

/**
 * Authoritative rules owner for the shared game flow.
 */
UCLASS(abstract)
class ACh4_multiGameGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	ACh4_multiGameGameMode();

	/** Registers the initial cargo count while the game is waiting to start. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game Flow|Cargo")
	bool InitializeCargoCount(int32 CargoCount);

	/** Transitions the game from Waiting to Playing after cargo is initialized. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game Flow")
	bool StartGame();

	/** Integration point called by the external Cargo system when its count changes. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game Flow|Cargo")
	bool UpdateRemainingCargo(int32 NewRemainingCargo);

	/** Requests a clear after a valid Cart reaches the Final Delivery Zone. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game Flow")
	bool TryCompleteGame();

protected:
	virtual void BeginPlay() override;

	/** Test-only convenience. Production flow should call StartGame explicitly. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game Flow|Debug")
	bool bAutoStartGame = false;

	/** Test-only initial count used when bAutoStartGame is enabled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game Flow|Debug", meta=(EditCondition="bAutoStartGame", ClampMin="1"))
	int32 DebugInitialCargoCount = 20;

private:
	class ACh4_multiGameGameState* GetGameFlowGameState() const;
	void EndGameAsClear();
	void EndGameAsGameOver();
};



