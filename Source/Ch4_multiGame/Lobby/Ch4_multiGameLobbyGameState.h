// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Ch4_multiGameLobbyGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FCh4LobbyPlayerCountChangedSignature,
	int32, CurrentPlayerCount,
	int32, MaxPlayerCount);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FCh4LobbyReadySummaryChangedSignature,
	int32, ReadyPlayerCount,
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

	/** Fired when the authoritative number of Ready players or lobby capacity changes. */
	UPROPERTY(BlueprintAssignable, Category="Lobby|Ready|Events")
	FCh4LobbyReadySummaryChangedSignature OnReadySummaryChanged;

	UFUNCTION(BlueprintPure, Category="Lobby")
	int32 GetCurrentPlayerCount() const { return CurrentPlayerCount; }

	UFUNCTION(BlueprintPure, Category="Lobby")
	int32 GetMaxPlayerCount() const { return MaxPlayerCount; }

	UFUNCTION(BlueprintPure, Category="Lobby|Ready")
	int32 GetReadyPlayerCount() const { return ReadyPlayerCount; }

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FCh4LobbyReadyStatusContract;
#endif
	bool SetLobbyCounts(
		int32 NewCurrentPlayerCount,
		int32 NewReadyPlayerCount,
		int32 NewMaxPlayerCount);

	UFUNCTION()
	void OnRep_CurrentPlayerCount();

	UFUNCTION()
	void OnRep_ReadyPlayerCount();

	UFUNCTION()
	void OnRep_MaxPlayerCount();

	void BroadcastPlayerCountChanged();
	void BroadcastReadySummaryChanged();
	void ShowClientDebugStatus() const;

private:
	UPROPERTY(ReplicatedUsing=OnRep_CurrentPlayerCount, BlueprintReadOnly, Category="Lobby", meta=(AllowPrivateAccess="true"))
	int32 CurrentPlayerCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_ReadyPlayerCount, BlueprintReadOnly, Category="Lobby|Ready", meta=(AllowPrivateAccess="true"))
	int32 ReadyPlayerCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_MaxPlayerCount, BlueprintReadOnly, Category="Lobby", meta=(AllowPrivateAccess="true"))
	int32 MaxPlayerCount = 1;
};
