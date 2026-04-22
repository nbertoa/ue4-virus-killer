#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HealthPickup.generated.h"

/** Custom log category for AHealthPickup. */
DECLARE_LOG_CATEGORY_EXTERN(LogHealthPickup, Log, All);

/**
 * A drifting pickup that restores one ScaleFactor unit of health to the player.
 *
 * Spawned in two situations:
 *   - By AVirusEnemy::OnDamage() every time an enemy takes a hit (always).
 *   - By APlayerPawn::TryToSpawnProjectile() when the player fires their last
 *     safe shot (i.e. firing another would kill them).
 *
 * On spawn the pickup picks a random target location within a 5000-unit radius
 * and drifts toward it using VInterpTo. Speed and rotation rate are randomized
 * in BeginPlay so each pickup moves differently.
 *
 * When the player's sphere overlaps the pickup, APlayerPawn::OnHealthPickup()
 * is called and the pickup destroys itself.
 */
UCLASS()
class VIRUSKILLER_API AHealthPickup : public AActor
{
    GENERATED_BODY()

public:
    AHealthPickup();

    virtual void Tick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;

private:
    // -----------------------------------------------------------------------
    // Components
    // -----------------------------------------------------------------------

    /** Visible pickup mesh. Collision disabled — all detection via SphereCollision. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UStaticMeshComponent> Mesh;

    /** Sphere trigger that detects player overlap. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class USphereComponent> SphereCollision;

    // -----------------------------------------------------------------------
    // Movement
    // -----------------------------------------------------------------------

    /**
     * Moves the pickup toward TargetLocation using VInterpTo and applies
     * a constant yaw rotation each tick.
     *
     * @param DeltaTime  Frame delta for interpolation.
     */
    void MoveToTargetLocation(const float DeltaTime);

    /**
     * Picks a random world-space XY position within ±5000 units as the
     * drift destination. Called once in BeginPlay.
     */
    void InitTargetLocation();

    /** World-space destination the pickup drifts toward. */
    FVector TargetLocation = FVector::ZeroVector;

    /** Interpolation speed toward TargetLocation. Randomized in BeginPlay. */
    float Speed = 0.0f;

    /** Yaw degrees applied per tick for the spinning effect. Randomized in BeginPlay. */
    float RotationRate = 0.0f;

    // -----------------------------------------------------------------------
    // Overlap
    // -----------------------------------------------------------------------

    /**
     * Called when any actor enters SphereCollision.
     * Calls APlayerPawn::OnHealthPickup() if the overlapping actor is a
     * living player, then destroys this pickup.
     */
    UFUNCTION()
    void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent,
                        AActor* OtherActor,
                        UPrimitiveComponent* OtherComp,
                        int32 OtherBodyIndex,
                        bool bFromSweep,
                        const FHitResult& SweepResult);
};
