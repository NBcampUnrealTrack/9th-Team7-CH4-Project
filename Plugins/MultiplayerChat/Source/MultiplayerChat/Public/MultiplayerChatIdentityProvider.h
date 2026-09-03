#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MultiplayerChatIdentityProvider.generated.h"

// 프로젝트의 기존 플레이어 식별 정보를 채팅 플러그인에 제공하는 인터페이스
UINTERFACE(BlueprintType, Blueprintable)
class MULTIPLAYERCHAT_API UMultiplayerChatIdentityProvider
	: public UInterface
{
	GENERATED_BODY()
};

// 채팅 플러그인이 플레이어의 고유 ID와 표시 이름을 조회하는 인터페이스
class MULTIPLAYERCHAT_API IMultiplayerChatIdentityProvider
{
	GENERATED_BODY()

public:
	// 귓속말과 Party 멤버 식별에 사용할 안정적인 플레이어 ID를 반환
	UFUNCTION(BlueprintCallable,BlueprintNativeEvent,Category = "Multiplayer Chat|Identity")
	FString GetMultiplayerChatPlayerId() const;

	virtual FString GetMultiplayerChatPlayerId_Implementation() const
	{
		return FString();
	}

	// 채팅창에 표시하고 닉네임 검색에 사용할 이름을 반환
	UFUNCTION(BlueprintCallable,BlueprintNativeEvent,Category = "Multiplayer Chat|Identity")
	FString GetMultiplayerChatDisplayName() const;

	virtual FString GetMultiplayerChatDisplayName_Implementation() const
	{
		return FString();
	}
};
