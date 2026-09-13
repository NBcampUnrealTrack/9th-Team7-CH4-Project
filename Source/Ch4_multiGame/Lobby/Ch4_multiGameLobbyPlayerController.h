// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Ch4_multiGamePlayerController.h"
#include "Ch4_multiGameLobbyPlayerController.generated.h"

class UInputAction;
class UCh4LobbyReadyWidget;
class ACh4_multiGameLobbyGameState;
class ACh4_multiGameLobbyPlayerState;

/** Lobby-only PlayerController that forwards the local Ready input to the server. */
UCLASS()
class ACh4_multiGameLobbyPlayerController : public ACh4_multiGamePlayerController
{
	GENERATED_BODY()

public:
	ACh4_multiGameLobbyPlayerController();

	/** Diagnostic equivalent of pressing R, useful in headless multiplayer tests. */
	UFUNCTION(Exec)
	void LobbyReady();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnRep_PlayerState() override;
	virtual void AcknowledgePossession(APawn* InPawn) override;
	virtual void SetupInputComponent() override;

	/** Assign WBP_LobbyReadyStatus on the active Lobby PlayerController Blueprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Lobby Ready")
	TSubclassOf<UCh4LobbyReadyWidget> LobbyReadyWidgetClass;

private:
	void InitializeLobbyReadyUI();
	void BindLobbyReadyState();
	void RefreshLobbyReadyUI();
	void RemoveLobbyReadyUI();
	void HandleReadyInput();

	UFUNCTION()
	void HandleReadySummaryChanged(int32 ReadyPlayerCount, int32 CurrentPlayerCount);

	UFUNCTION()
	void HandleLocalReadyStateChanged(bool bIsReady);

	UFUNCTION(Server, Reliable)
	void ServerSetReady();

private:
	UPROPERTY()
	TObjectPtr<UInputAction> LobbyReadyAction;

	UPROPERTY()
	TObjectPtr<UCh4LobbyReadyWidget> LobbyReadyWidget;

	TWeakObjectPtr<ACh4_multiGameLobbyGameState> BoundLobbyGameState;
	TWeakObjectPtr<ACh4_multiGameLobbyPlayerState> BoundLobbyPlayerState;
	bool bMissingLobbyReadyWidgetClassLogged = false;
};
