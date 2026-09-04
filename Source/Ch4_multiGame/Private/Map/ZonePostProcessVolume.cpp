#include "Map/ZonePostProcessVolume.h"

AZonePostProcessVolume::AZonePostProcessVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);
}