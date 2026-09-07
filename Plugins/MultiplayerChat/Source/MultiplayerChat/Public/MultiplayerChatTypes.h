#pragma once

#include "CoreMinimal.h"
#include "MultiplayerChatTypes.generated.h"

// 채팅 메시지가 전달되는 채널의 종류입니다.
//
// BlueprintType을 사용했기 때문에 블루프린트와 UMG에서도
// 채널 값을 선택하고 확인할 수 있습니다.
UENUM(BlueprintType)
enum class EMultiplayerChatChannel : uint8
{
	Global  UMETA(DisplayName = "Global"),
	Team    UMETA(DisplayName = "Team"),
	Party   UMETA(DisplayName = "Party"),
	Whisper UMETA(DisplayName = "Whisper"),
	System  UMETA(DisplayName = "System")
};

// 서버가 처리한 닉네임 변경 요청의 결과
UENUM(BlueprintType)
enum class EMultiplayerChatNicknameResult : uint8
{
	Success           UMETA(DisplayName = "Success"),
	ChangesDisabled   UMETA(DisplayName = "Changes Disabled"),
	ManagedExternally UMETA(DisplayName = "Managed Externally"),
	InvalidLength     UMETA(DisplayName = "Invalid Length"),
	InvalidCharacters UMETA(DisplayName = "Invalid Characters"),
	AlreadyInUse      UMETA(DisplayName = "Already In Use"),
	ReservedName      UMETA(DisplayName = "Reserved Name"),
	RateLimited       UMETA(DisplayName = "Rate Limited"),
	Unavailable		  UMETA(DisplayName = "Unavailable"),
	ChangeLimitReached UMETA(DisplayName = "Change Limit Reached")
};

USTRUCT(BlueprintType)
struct MULTIPLAYERCHAT_API FMultiplayerChatMessage
{
	GENERATED_BODY()

	// 각각의 메시지를 구분하기 위한 고유 식별자
	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer Chat")
	FGuid MessageId;

	// 메시지를 보낸 플레이어의 고유 식별자
	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer Chat")
	FString SenderId;

	// 채팅창에 표시할 플레이어 이름
	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer Chat")
	FString SenderName;

	// 메시지가 전달될 채팅 채널
	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer Chat")
	EMultiplayerChatChannel Channel = EMultiplayerChatChannel::Global;

	// 팀, 파티 또는 귓속말 대상을 구분하기 위한 값
	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer Chat")
	FString ChannelId;

	// 채널을 UI에 표시할 때 사용할 이름
	// 귓속말에서는 수신자 이름, 파티에서는 파티 이름이나 참가 코드를 사용합니다.
	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer Chat")
	FString ChannelDisplayName;

	// 실제로 채팅창에 표시할 메시지 내용
	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer Chat")
	FString MessageText;
	
	// 서버가 메시지를 승인한 시각
	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer Chat")
	FDateTime ServerTimestamp;
};
