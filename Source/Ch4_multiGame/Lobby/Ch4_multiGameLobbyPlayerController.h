// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Ch4_multiGamePlayerController.h"
#include "Ch4_multiGameLobbyPlayerController.generated.h"

class UInputAction;

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
	virtual void SetupInputComponent() override;

private:
	void HandleReadyInput();

	UFUNCTION(Server, Reliable)
	void ServerSetReady();

private:
	UPROPERTY()
	TObjectPtr<UInputAction> LobbyReadyAction;
};
