#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerHUD.generated.h"

/** Custom log category for UPlayerHUD. */
DECLARE_LOG_CATEGORY_EXTERN(LogPlayerHUD, Log, All);

/**
 * In-game HUD widget displayed while the player is alive.
 *
 * The C++ side defines only the contract: PlayDamageAnimation is a
 * BlueprintImplementableEvent so the Blueprint subclass controls the
 * animation curve, duration, and visual style without touching C++.
 *
 * APlayerPawn creates and owns this widget via CreateAndShowPlayerHUD().
 * It is replaced by UGameOverHUD when the player dies.
 */
UCLASS()
class VIRUSKILLER_API UPlayerHUD : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * Triggers the damage feedback animation (flash, shake, etc.).
     * Implemented entirely in the Blueprint subclass's animation timeline.
     * Called by APlayerPawn::OnDamage every time the player takes a hit.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void PlayDamageAnimation();
};
