#include "UI/MainMenu/Ch4MainMenuViewModel.h"

#include "Ch4_multiGame.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Kismet/KismetSystemLibrary.h"

// 패널 전환
void UCh4MainMenuViewModel::ShowRoomSelection()
{
	// 검색 중이었다면 검색 취소 및 로딩 해제 (버튼 잠김 방지)
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
		if (Session.IsValid())
		{
			if (SearchSettings.IsValid() &&
				SearchSettings->SearchState == EOnlineAsyncTaskState::InProgress)
			{
				Session->CancelFindSessions();
			}
			Session->OnFindSessionsCompleteDelegates.RemoveAll(this);
		}
	}
	SetbIsLoading(false);
	SetCurrentPanel(EMenuPanel::RoomSelection);
	SetStatusText(FText::GetEmpty());
}

void UCh4MainMenuViewModel::ShowMainMenu()
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
		if (Session.IsValid())
		{
			if (SearchSettings.IsValid() &&
				SearchSettings->SearchState == EOnlineAsyncTaskState::InProgress)
			{
				Session->CancelFindSessions();
			}
			Session->OnFindSessionsCompleteDelegates.RemoveAll(this);
		}
	}
	SetbIsLoading(false);
	SetCurrentPanel(EMenuPanel::Main);
	SetStatusText(FText::GetEmpty());
}

// 세션 액션: 방 만들기 (Host)
void UCh4MainMenuViewModel::HostGame()
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (!Subsystem)
	{
		SetStatusText(NSLOCTEXT("Ch4MainMenu", "NoSubsystem", "Online subsystem not found."));
		return;
	}
	
	IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
	if (!Session.IsValid())
	{
		return;
	}
	
	SetbIsLoading(true);
	SetStatusText(NSLOCTEXT("Ch4MainMenu", "HostingGame", "Creating room..."));
	
	// 기존에 남아있는 세션이 있다면 먼저 정리
	if (Session->GetNamedSession(NAME_GameSession) != nullptr)
	{
		Session->DestroySession(NAME_GameSession);
	}
	
	// 세션 설정
	FOnlineSessionSettings HostSettings;
	HostSettings.bIsLANMatch = true;			// LAN 매치 (Steam 전환 시 false)
	HostSettings.bShouldAdvertise = true;		// 다른 사람에게 방 검색 허용
	HostSettings.bUsesPresence = false;
	HostSettings.bAllowJoinInProgress = true;
	HostSettings.bAllowJoinViaPresence = false;
	HostSettings.NumPublicConnections = 4;		// 최대 4명
	HostSettings.Set(FName(TEXT("MAPNAME")), FString(TEXT("L_Lobby")), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	
	Session->OnCreateSessionCompleteDelegates.AddUObject(
		this, &UCh4MainMenuViewModel::OnCreateSessionComplete);
	
	Session->CreateSession(0, NAME_GameSession, HostSettings);
}

void UCh4MainMenuViewModel::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
		if (Session.IsValid())
		{
			Session->OnCreateSessionCompleteDelegates.RemoveAll(this);
		}
	}

	if (bWasSuccessful)
	{
		if (UWorld* World = GetWorld())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				PC->ClientTravel(TEXT("/Game/Lobby/L_Lobby?listen"), TRAVEL_Absolute);
			}
		}
	}
	else
	{
		SetbIsLoading(false);
		SetStatusText(NSLOCTEXT("Ch4MainMenu", "CreateFailed", "Failed to create room."));
	}
}

void UCh4MainMenuViewModel::FindRooms()
{
	if (bIsLoading)
	{
		UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] Duplicate room search ignored"));
		return;
	}

	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (!Subsystem)
	{
		SetStatusText(NSLOCTEXT("Ch4MainMenu", "NoSubsystem", "Online subsystem not found."));
		return;
	}
	
	IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
	if (!Session.IsValid())
	{
		return;
	}
	
	SetbIsLoading(true);
	SetStatusText(NSLOCTEXT("Ch4MainMenu", "SearchingRooms", "Searching for rooms..."));
	SetCurrentPanel(EMenuPanel::RoomList);
	
	// 기존 목록 및 선택 상태 초기화
	RoomList.Empty();
	SelectRoom(-1);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RoomList);
	
	// 세션 검색 설정
	SearchSettings = MakeShareable(new FOnlineSessionSearch());
	SearchSettings->bIsLanQuery = true;		// LAN / VPN 매치
	SearchSettings->MaxSearchResults = 20;
	SearchSettings->TimeoutInSeconds = 3.0f; // LAN / VPN 검색 대기 시간 3초로 안정화
	
	// 검색 완료 콜백 등록 (구독)
	Session->OnFindSessionsCompleteDelegates.RemoveAll(this);
	Session->OnFindSessionsCompleteDelegates.AddUObject(
		this, &UCh4MainMenuViewModel::OnFindSessionsComplete);
	
	if (!Session->FindSessions(0, SearchSettings.ToSharedRef()))
	{
		Session->OnFindSessionsCompleteDelegates.RemoveAll(this);
		SetbIsLoading(false);
		SetStatusText(NSLOCTEXT("Ch4MainMenu", "SearchStartFailed", "Could not start room search."));
	}
}



void UCh4MainMenuViewModel::OnFindSessionsComplete(bool bWasSuccessful)
{
	SetbIsLoading(false);
	
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (!Subsystem ) return;
	
	// 콜백 구독 해제 (중복 호출 방지)
	IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
	if (Session.IsValid())
	{
		Session->OnFindSessionsCompleteDelegates.RemoveAll(this);
	}
	
	if (!bWasSuccessful || !SearchSettings.IsValid())
	{
		SetStatusText(NSLOCTEXT("Ch4MainMenu", "SearchFailed", "Search failed. Please try again."));
		return;
	}
	
	// 검색 결과 파싱하여 RoomList 채우기
	RoomList.Empty();
	SelectRoom(-1);
	const int32 ResultCount = SearchSettings->SearchResults.Num();
	
	for (int32 i = 0; i < ResultCount; ++i)
	{
		const FOnlineSessionSearchResult& Result = SearchSettings->SearchResults[i];
		
		UCh4RoomEntryData* NewEntry = NewObject<UCh4RoomEntryData>(this);
		
		// 호스트 이름 (없으면 기본값)
		NewEntry->ServerName = Result.Session.OwningUserName.IsEmpty()
			? FString::Printf(TEXT("Server #%d"), i + 1)
			: Result.Session.OwningUserName;
		
		NewEntry->MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
		NewEntry->CurrentPlayers = NewEntry->MaxPlayers - Result.Session.NumOpenPublicConnections;
		NewEntry->PingInMs = Result.PingInMs;
		NewEntry->SearchResultIndex = i;
		
		RoomList.Add(NewEntry);
	}
	
	if (ResultCount == 0)
	{
		SetStatusText(NSLOCTEXT("Ch4MainMenu", "NoRooms", "No rooms found. Click Refresh to try again."));
	}
	else
	{
		SetStatusText(FText::Format(NSLOCTEXT("Ch4MainMenu", "RoomsFound", "Found {0} room(s)"), FText::AsNumber(ResultCount)));
	}
	
	// UI에 방 목록 갱신 알림 방송
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RoomList);
}

// --------------------------------------
// 세션 액션: 방 선택 & 참가 (Select & Join)
// --------------------------------------

void UCh4MainMenuViewModel::SelectRoom(int32 Index)
{
	int32 FoundIdx = -1;
	if (RoomList.IsValidIndex(Index))
	{
		FoundIdx = Index;
	}
	else
	{
		for (int32 i = 0; i < RoomList.Num(); ++i)
		{
			if (RoomList[i] && RoomList[i]->SearchResultIndex == Index)
			{
				FoundIdx = i;
				break;
			}
		}
	}
	
	if (RoomList.IsValidIndex(FoundIdx))
	{
		SelectedRoomIndex = FoundIdx;
		SetbCanJoinRoom(true);
	}
	else
	{
		SelectedRoomIndex = -1;
		SetbCanJoinRoom(false);
	}
}

void UCh4MainMenuViewModel::SelectRoomEntry(UCh4RoomEntryData* RoomEntry)
{
	if (RoomEntry && RoomList.Contains(RoomEntry))
	{
		SelectedRoomIndex = RoomList.IndexOfByKey(RoomEntry);
		SetbCanJoinRoom(true);
	}
	else
	{
		SelectedRoomIndex = -1;
		SetbCanJoinRoom(false);
	}
}

void UCh4MainMenuViewModel::JoinSelectedRoom()
{
	if (!RoomList.IsValidIndex(SelectedRoomIndex))
	{
		SetStatusText(NSLOCTEXT("Ch4MainMenu", "NoRoomsSelected", "Please select a room to join."));
		return;
	}
	
	int32 TargetResultIdx = RoomList[SelectedRoomIndex]->SearchResultIndex;
	
	if (!SearchSettings.IsValid() || !SearchSettings->SearchResults.IsValidIndex(TargetResultIdx))
	{
		SetStatusText(NSLOCTEXT("Ch4MainMenu", "InvalidRoom", "Invalid room selected."));
		return;
	}
	
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (!Subsystem) return;
	
	IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
	if (!Session.IsValid()) return;
	
	SetbIsLoading(true);
	SetStatusText(NSLOCTEXT("Ch4MainMenu", "JoiningRoom", "Joining room..."));
	
	Session->OnJoinSessionCompleteDelegates.AddUObject(
		this, &UCh4MainMenuViewModel::OnJoinSessionComplete);
	
	Session->JoinSession(0, NAME_GameSession, SearchSettings->SearchResults[TargetResultIdx]);
}

void UCh4MainMenuViewModel::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
		if (Session.IsValid())
		{
			Session->OnJoinSessionCompleteDelegates.RemoveAll(this);
			
			FString ConnectString;
			if (Session->GetResolvedConnectString(SessionName, ConnectString))
			{
				if (UWorld* World = GetWorld())
				{
					if (APlayerController* PC = World->GetFirstPlayerController())
					{
						PC->ClientTravel(ConnectString, TRAVEL_Absolute);
						return;
					}
				}
			}
		}
	}
	
	SetbIsLoading(false);
	SetStatusText(NSLOCTEXT("Ch4MainMenu", "JoinFailed", "Failed to join room."));
}

// ---------------
// 시스템 & Setters
// ---------------

void UCh4MainMenuViewModel::QuitGame()
{
	UKismetSystemLibrary::QuitGame(
		GetWorld(),
		nullptr,
		EQuitPreference::Quit,
		false);
}

// Private Setters (MVVM 내부 규칙)
void UCh4MainMenuViewModel::SetCurrentPanel(EMenuPanel NewPanel)
{
	if (CurrentPanel != NewPanel)
	{
		CurrentPanel = NewPanel;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CurrentPanel);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MainPanelVisibility);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RoomSelectionVisibility);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RoomListVisibility);
	}
}

void UCh4MainMenuViewModel::SetStatusText(FText NewText)
{
	UE_MVVM_SET_PROPERTY_VALUE(StatusText, NewText);
}

void UCh4MainMenuViewModel::SetbIsLoading(bool bNewIsLoading)
{
	if (bIsLoading != bNewIsLoading)
	{
		bIsLoading = bNewIsLoading;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsLoading);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanInteract);
	}
}

void UCh4MainMenuViewModel::SetbCanJoinRoom(bool bNewCanJoin)
{
	UE_MVVM_SET_PROPERTY_VALUE(bCanJoinRoom, bNewCanJoin);
}
