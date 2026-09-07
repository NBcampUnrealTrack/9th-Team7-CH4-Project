#include "Map/ZonePostProcessVolume.h"
#include "Kismet/GameplayStatics.h"

AZonePostProcessVolume::AZonePostProcessVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);
	
	
}
