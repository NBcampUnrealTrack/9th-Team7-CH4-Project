#include "UI/MainMenu/Ch4MainMenuViewModel.h"

#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Player/Ch4_multiGameGameInstance.h"

UCh4MainMenuViewModel::UCh4MainMenuViewModel()
{
	SelectedRoomInfoText = NSLOCTEXT("Ch4MainMenu", "NoRoomSelected", "선택된 방이 없습니다.");
}

UWorld* UCh4MainMenuViewModel::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject)) return nullptr;
	if (SessionGameInstance.IsValid()) return SessionGameInstance->GetWorld();
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void UCh4MainMenuViewModel::BeginDestroy()
{
	if (SessionGameInstance.IsValid()) SessionGameInstance->OnSteamSessionChanged.RemoveDynamic(this, &ThisClass::RefreshSteamState);
	Super::BeginDestroy();
}

void UCh4MainMenuViewModel::InitializeWithWorld(UWorld* World)
{
	if (!World) return;
	UCh4_multiGameGameInstance* GI = World->GetGameInstance<UCh4_multiGameGameInstance>();
	if (SessionGameInstance.Get() != GI && SessionGameInstance.IsValid())
	{
		SessionGameInstance->OnSteamSessionChanged.RemoveDynamic(this, &ThisClass::RefreshSteamState);
	}
	SessionGameInstance = GI;
	if (GI)
	{
		GI->OnSteamSessionChanged.AddUniqueDynamic(this, &ThisClass::RefreshSteamState);
		RefreshSteamState();
	}
}

UCh4_multiGameGameInstance* UCh4MainMenuViewModel::ResolveSteamGameInstance()
{
	if (!SessionGameInstance.IsValid()) InitializeWithWorld(GetWorld());
	if (!SessionGameInstance.IsValid())
	{
		SetStatusText(NSLOCTEXT("Ch4MainMenu", "NoGameInstance", "Game instance unavailable. Initialize this menu with its world."));
	}
	return SessionGameInstance.Get();
}

void UCh4MainMenuViewModel::RefreshSteamState()
{
	UCh4_multiGameGameInstance* GI = SessionGameInstance.Get();
	if (!GI) return;
	UCh4RoomEntryData* Selection = RoomList.IsValidIndex(SelectedRoomIndex) ? RoomList[SelectedRoomIndex].Get() : nullptr;
	RoomList.Reset();
	for (UCh4RoomEntryData* Room : GI->GetSteamRooms()) RoomList.Add(Room);
	SelectedRoomIndex = RoomList.IndexOfByKey(Selection);
	SelectedRoomEntry = (SelectedRoomIndex != INDEX_NONE) ? RoomList[SelectedRoomIndex].Get() : nullptr;

	for (const TObjectPtr<UCh4RoomEntryData>& Room : RoomList)
	{
		if (Room)
		{
			Room->bIsSelected = (Room == SelectedRoomEntry);
		}
	}

	if (SelectedRoomEntry)
	{
		SelectedRoomInfoText = FText::Format(
			NSLOCTEXT("Ch4MainMenu", "SelectedRoomFormat", "선택된 방: {0} ({1}/{2})"),
			FText::FromString(SelectedRoomEntry->ServerName),
			FText::AsNumber(SelectedRoomEntry->CurrentPlayers),
			FText::AsNumber(SelectedRoomEntry->MaxPlayers));
	}
	else
	{
		SelectedRoomInfoText = NSLOCTEXT("Ch4MainMenu", "NoRoomSelected", "선택된 방이 없습니다.");
	}

	SetbIsLoading(GI->IsSteamSessionBusy());
	SetbCanJoinRoom(!bIsLoading && SelectedRoomIndex != INDEX_NONE
		&& SelectedRoomEntry && SelectedRoomEntry->CurrentPlayers < SelectedRoomEntry->MaxPlayers);
	SetStatusText(GI->GetSteamSessionStatus());

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RoomList);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedRoomIndex);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedRoomEntry);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedRoomInfoText);
}

void UCh4MainMenuViewModel::ShowRoomSelection()
{
	ResolveSteamGameInstance();
	if (bIsLoading) return;
	SetCurrentPanel(EMenuPanel::RoomSelection);
}

void UCh4MainMenuViewModel::ShowMainMenu()
{
	ResolveSteamGameInstance();
	if (bIsLoading) return;
	SetCurrentPanel(EMenuPanel::Main);
}

void UCh4MainMenuViewModel::HostGame()
{
	if (UCh4_multiGameGameInstance* GI = ResolveSteamGameInstance()) GI->HostSteamGame();
}

void UCh4MainMenuViewModel::FindRooms()
{
	if (UCh4_multiGameGameInstance* GI = ResolveSteamGameInstance())
	{
		if (GI->IsSteamSessionBusy()) return;
		SetCurrentPanel(EMenuPanel::RoomList);
		GI->FindSteamGames();
	}
}

void UCh4MainMenuViewModel::SelectRoom(int32 Index)
{
	// Keep the existing list-index API. Entry selection is preferred when the UI passes an object.
	int32 FoundIndex = RoomList.IsValidIndex(Index) ? Index : INDEX_NONE;
	if (FoundIndex == INDEX_NONE)
	{
		FoundIndex = RoomList.IndexOfByPredicate([Index](const UCh4RoomEntryData* Room)
		{
			return Room && Room->SearchResultIndex == Index;
		});
	}
	SelectRoomEntry(RoomList.IsValidIndex(FoundIndex) ? RoomList[FoundIndex].Get() : nullptr);
}

void UCh4MainMenuViewModel::SelectRoomEntry(UCh4RoomEntryData* RoomEntry)
{
	SelectedRoomEntry = RoomEntry;
	SelectedRoomIndex = RoomEntry ? RoomList.IndexOfByKey(RoomEntry) : INDEX_NONE;

	for (const TObjectPtr<UCh4RoomEntryData>& Room : RoomList)
	{
		if (Room)
		{
			Room->bIsSelected = (Room == RoomEntry);
		}
	}

	if (RoomEntry)
	{
		SelectedRoomInfoText = FText::Format(
			NSLOCTEXT("Ch4MainMenu", "SelectedRoomFormat", "선택된 방: {0} ({1}/{2})"),
			FText::FromString(RoomEntry->ServerName),
			FText::AsNumber(RoomEntry->CurrentPlayers),
			FText::AsNumber(RoomEntry->MaxPlayers));
	}
	else
	{
		SelectedRoomInfoText = NSLOCTEXT("Ch4MainMenu", "NoRoomSelected", "선택된 방이 없습니다.");
	}

	SetbCanJoinRoom(!bIsLoading && SelectedRoomIndex != INDEX_NONE
		&& RoomEntry && RoomEntry->CurrentPlayers < RoomEntry->MaxPlayers);

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedRoomIndex);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedRoomEntry);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedRoomInfoText);
}

void UCh4MainMenuViewModel::JoinSelectedRoom()
{
	if (UCh4_multiGameGameInstance* GI = ResolveSteamGameInstance())
	{
		GI->JoinSteamGame(RoomList.IsValidIndex(SelectedRoomIndex) ? RoomList[SelectedRoomIndex].Get() : nullptr);
	}
}

void UCh4MainMenuViewModel::QuitGame()
{
	// GameInstance::Shutdown owns final best-effort session cleanup.
	UKismetSystemLibrary::QuitGame(GetWorld(), nullptr, EQuitPreference::Quit, false);
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
