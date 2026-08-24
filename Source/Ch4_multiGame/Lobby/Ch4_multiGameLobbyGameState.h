// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Ch4_multiGameLobbyGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FCh4LobbyPlayerCountChangedSignature,
	int32, CurrentPlayerCount,
	int32, MaxPlayerCount);

/** Replicated player-count data for the IP-based test lobby. */
UCLASS()
class ACh4_multiGameLobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

	friend class ACh4_multiGameLobbyGameMode;

public:
	/** Fired on the server and clients whenever either lobby count changes. */
	UPROPERTY(BlueprintAssignable, Category="Lobby|Events")
	FCh4LobbyPlayerCountChangedSignature OnPlayerCountChanged;

	UFUNCTION(BlueprintPure, Category="Lobby")
	int32 GetCurrentPlayerCount() const { return CurrentPlayerCount; }

	UFUNCTION(BlueprintPure, Category="Lobby")
	int32 GetMaxPlayerCount() const { return MaxPlayerCount; }

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	bool SetPlayerCounts(int32 NewCurrentPlayerCount, int32 NewMaxPlayerCount);

	UFUNCTION()
	void OnRep_CurrentPlayerCount();

	void BroadcastPlayerCountChanged();
	void ShowClientDebugStatus() const;

private:
	UPROPERTY(ReplicatedUsing=OnRep_CurrentPlayerCount, BlueprintReadOnly, Category="Lobby", meta=(AllowPrivateAccess="true"))
	int32 CurrentPlayerCount = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby", meta=(AllowPrivateAccess="true"))
	int32 MaxPlayerCount = 1;
};
