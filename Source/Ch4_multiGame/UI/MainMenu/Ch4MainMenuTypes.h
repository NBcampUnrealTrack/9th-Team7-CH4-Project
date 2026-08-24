#pragma once

#include "CoreMinimal.h"
#include "Ch4MainMenuTypes.generated.h"

// 메인 메뉴에서 현재 표시 중인 패널 상태
UENUM(BlueprintType)
enum class EMenuPanel : uint8
{
	Main			UMETA(DisplayName = "Main"),
	RoomSelection	UMETA(DisplayName = "Room Selection"),
	RoomList		UMETA(DisplayName = "Room List"),
};

// 방 목록의 각 아이템 정보를 담는 데이터 객체
UCLASS(BlueprintType)
class CH4_MULTIGAME_API UCh4RoomEntryData : public UObject
{
	GENERATED_BODY()
	
public:
	// 호스트 이름 또는 방 이름
	UPROPERTY(BlueprintReadOnly, Category = "Room")
	FString ServerName;
	
	// 현재 인원 수
	UPROPERTY(BlueprintReadOnly, Category = "Room")
	int32 CurrentPlayers = 0;
	
	// 최대 인원 수
	UPROPERTY(BlueprintReadOnly, Category = "Room")
	int32 MaxPlayers = 4;
	
	// 핑 (ms)
	UPROPERTY(BlueprintReadOnly, Category = "Room")
	int32 PingInMs = 0;
	
	// SearchResults 배열 내 인덱스
	UPROPERTY(BlueprintReadOnly, Category = "Room")
	int32 SearchResultIndex = 0;
	
	// UI에 표시할 포맷 텍스트 (예 : "HostName (1/4) - 20ms")
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Room")
	FText GetDisplayName() const
	{
		return FText::Format(
			NSLOCTEXT("Ch4MainMenu", "RoomEntryFormat", "{0} ({1}/{2}) - {3}ms"),
			FText::FromString(ServerName),
			FText::AsNumber(CurrentPlayers),
			FText::AsNumber(MaxPlayers),
			FText::AsNumber(PingInMs)
			);
	}
};