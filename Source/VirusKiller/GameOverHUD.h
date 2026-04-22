#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameOverHUD.generated.h"

/** Custom log category for UGameOverHUD. */
DECLARE_LOG_CATEGORY_EXTERN(LogGameOverHUD, Log, All);

/**
 * Game Over screen widget shown when the player's health (scale) reaches zero.
 *
 * Created and added to viewport by APlayerPawn::CreateAndShowGameOverHUD()
 * the moment IsDead() returns true after taking damage.
 *
 * All visual content (restart button, score, animations) is implemented in
 * the Blueprint subclass — this C++ class acts purely as the typed contract
 * that APlayerPawn references via TSubclassOf<UGameOverHUD>.
 */
UCLASS()
class VIRUSKILLER_API UGameOverHUD : public UUserWidget
{
    GENERATED_BODY()
};
