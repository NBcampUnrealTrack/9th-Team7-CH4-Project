#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MultiplayerChatAutoAttachSubsystem.generated.h"

class AActor;
class APlayerController;
class UWorld;

// 월드에서 PlayerController를 감지하고 채팅 컴포넌트를 자동 장착합니다.
UCLASS()
class MULTIPLAYERCHAT_API UMultiplayerChatAutoAttachSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 게임 서버 월드에서만 이 Subsystem을 생성할지 결정합니다.
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// 월드가 시작되면 기존 PlayerController를 처리하고 생성 이벤트를 구독합니다.
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// 월드가 종료될 때 등록한 이벤트를 해제합니다.
	virtual void Deinitialize() override;

private:
	// 월드에 새 Actor가 생성됐을 때 PlayerController인지 확인합니다.
	void HandleActorSpawned(AActor* SpawnedActor);

	// 필요한 경우 PlayerController에 채팅 컴포넌트를 장착합니다.
	void AttachChatComponent(APlayerController* PlayerController);

	// Actor 생성 이벤트를 안전하게 해제하기 위한 핸들입니다.
	FDelegateHandle ActorSpawnedDelegateHandle;
};