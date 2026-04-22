#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VirusKillerGameModeBase.generated.h"

/**
 * Default GameMode for the Virus Killer project.
 *
 * No custom GameMode logic is needed — all game flow is managed by
 * APlayerPawn (health, HUD lifecycle) and AVirusEnemy (spawning chain).
 * This class exists to satisfy the engine's GameMode requirement and to
 * allow future overrides without modifying the project template base.
 */
UCLASS()
class VIRUSKILLER_API AVirusKillerGameModeBase : public AGameModeBase
{
    GENERATED_BODY()
};
