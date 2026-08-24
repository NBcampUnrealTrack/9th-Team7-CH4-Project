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
	virtual void StartPlay() override;
	virtual void PreLogin(
		const FString& Options,
		const FString& Address,
		const FUniqueNetIdRepl& UniqueId,
		FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Accepts a Ready request from the owning lobby controller on the server. */
	void HandlePlayerReady(APlayerController* RequestingPlayer);

	UFUNCTION(BlueprintPure, Category="Lobby")
	int32 GetMaxLobbyPlayers() const { return MaxLobbyPlayers; }

protected:
	/** Single source of truth for the lobby capacity, including the listen-server host. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lobby", meta=(ClampMin="2", UIMin="2"))
	int32 MaxLobbyPlayers = 4;

	/** Prevents a listen-server host from starting alone before remote players can join. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lobby|Ready", meta=(ClampMin="2", UIMin="2"))
	int32 MinPlayersToStart = 2;

	/** Verified package path for the existing gameplay map. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lobby|Travel")
	FName GameplayMapPackage = TEXT("/Game/ThirdPerson/Lvl_ThirdPerson");

private:
	class ACh4_multiGameLobbyGameState* GetLobbyGameState() const;
	void UpdateLobbyPlayerCount(int32 NewPlayerCount);
	void CheckAllPlayersReady();
	void GetReadyPlayerCounts(int32& OutReadyPlayers, int32& OutTotalPlayers) const;
	void StartGameTravel();
	void ShowServerDebugStatus(const FString& EventMessage, const FColor& Color, float Duration) const;
	int32 GetListenPort() const;
	FString GetPlayerLogLabel(const AController* Controller) const;

	bool bTravelStarted = false;
};
