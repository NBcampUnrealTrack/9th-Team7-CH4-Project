#include "Player/Ch4_PlayerController.h"

ACh4_PlayerController::ACh4_PlayerController()
{
	// The gameplay pawn owns IMC_Player. Keep only the existing pause/voice
	// mappings from IMC_Default so its template actions cannot consume WASD.
	bUseTemplateInputMappings = false;
}

