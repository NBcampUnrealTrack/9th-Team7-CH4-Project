// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Ch4_multiGameLobbyGameMode.generated.h"

/** Server-authoritative rules owner for the direct-IP multiplayer test lobby. */
UCLASS()
class ACh4_multiGameLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACh4_multiGameLobbyGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void InitSeamlessTravelPlayer(AController* NewController) override;
	virtual void StartPlay() override;
	virtual void PreLogin(
		const FString& Options,
		const FString& Address,
		const FUniqueNetIdRepl& UniqueId,
		FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	/** Accepts a Ready toggle request from the owning lobby controller on the server. */
	void HandlePlayerReady(APlayerController* RequestingPlayer);
	void ResetTravelAfterFailure();
	const FString& GetPendingTravelDestination() const { return PendingTravelDestination; }

	UFUNCTION(BlueprintPure, Category="Lobby")
	int32 GetMaxLobbyPlayers() const { return MaxLobbyPlayers; }

protected:
	/** Single source of truth for the lobby capacity, including the listen-server host. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lobby", meta=(ClampMin="2", UIMin="2"))
	int32 MaxLobbyPlayers = 4;

	/** Allows solo Ready by default; every active player must still be Ready. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lobby|Ready", meta=(ClampMin="1", UIMin="1"))
	int32 MinPlayersToStart = 1;

	/** Server-side random pool of gameplay maps configured through the Unreal asset picker. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lobby|Travel")
	TArray<TSoftObjectPtr<UWorld>> GameplayMaps;

	/** Join-order character slots. The array order is Cat, Dog, Gorilla, then Otter by default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lobby|Characters")
	TArray<TSubclassOf<APawn>> LobbyCharacterClasses;

	/** Reserves the first free character slot immediately before the player's initial pawn spawn. */
	virtual void OnPostLogin(AController* NewPlayer) override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FCh4LobbyReadyTravelRulesTest;
	friend class FCh4LobbyCharacterAssignmentTest;
	friend class FCh4LobbyCookedMapValidationTest;
#endif

	class ACh4_multiGameLobbyGameState* GetLobbyGameState() const;
	void UpdateLobbyCounts(
		int32 NewPlayerCount,
		const class ACh4_multiGameLobbyPlayerState* ExcludedPlayerState = nullptr);
	void HandleDeferredLobbyRosterChange();
	void CheckAllPlayersReady();
	void GetReadyPlayerCounts(
		int32& OutReadyPlayers,
		int32& OutTotalPlayers,
		const class ACh4_multiGameLobbyPlayerState* ExcludedPlayerState = nullptr) const;
	static int32 ClampMinimumPlayersToStart(int32 MinimumPlayers, int32 LobbyCapacity);
	static bool CanStartLobbyTravel(
		int32 ReadyPlayers,
		int32 TotalPlayers,
		int32 MinimumPlayers,
		bool bIsTravelInProgress);
	static bool TrySelectRandomGameplayMap(
		const TArray<TSoftObjectPtr<UWorld>>& GameplayMapCandidates,
		FString& OutMapPackage);
	static int32 FindFirstAvailableCharacterSlot(const TArray<bool>& UnavailableSlots);
	int32 AssignCharacterSlot(AController* Controller, bool bInitializeSelection = true);
	int32 ReleaseCharacterSlot(AController* Controller);
	int32 FindAssignedCharacterSlot(AController* Controller) const;
	void StartGameTravel();
	void HandleGameplaySessionAvailabilityUpdated(bool bSucceeded);
	void PerformGameTravel();
	int32 GetListenPort() const;
	FString GetPlayerLogLabel(const AController* Controller) const;

	bool bTravelStarted = false;
	FString PendingTravelDestination;
	TMap<TWeakObjectPtr<AController>, int32> CharacterSlotsByController;
	TSet<TWeakObjectPtr<AController>> ControllersAwaitingInitialCharacterSpawn;
};
