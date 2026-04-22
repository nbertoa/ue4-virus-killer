#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlayerProjectile.generated.h"

/** Custom log category for APlayerProjectile. */
DECLARE_LOG_CATEGORY_EXTERN(LogPlayerProjectile, Log, All);

/**
 * Projectile fired by APlayerPawn toward the mouse cursor.
 *
 * Movement is tick-driven: every frame the projectile advances along its
 * local X axis by Speed units via AddActorLocalOffset. This keeps the
 * implementation simple for a top-down prototype — no UProjectileMovementComponent
 * is needed since gravity and arc effects are not required.
 *
 * Health cost: firing a projectile consumes a ScaleFactor unit of the player's
 * scale. The player's size IS the health bar — smaller = lower health.
 *
 * Lifetime: Shot() starts a FTimerHandle that calls DestroyProjectile() after
 * Lifetime seconds, preventing stray projectiles from persisting indefinitely.
 *
 * On overlap with a living AVirusEnemy, it calls AVirusEnemy::OnDamage()
 * and immediately destroys itself.
 */
UCLASS()
class VIRUSKILLER_API APlayerProjectile : public AActor
{
    GENERATED_BODY()

public:
    APlayerProjectile();

    virtual void Tick(float DeltaTime) override;

    /**
     * Starts the lifetime destruction timer. Must be called immediately after
     * SpawnActor — the projectile does not self-arm in BeginPlay to allow
     * the caller to configure it before the timer starts.
     */
    void Shot();

protected:
    virtual void BeginPlay() override;

private:
    // -----------------------------------------------------------------------
    // Components
    // -----------------------------------------------------------------------

    /** Sphere trigger used for overlap detection against AVirusEnemy. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class USphereComponent> SphereCollision;

    /** Visual mesh. Collision disabled — detection goes through SphereCollision. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UStaticMeshComponent> Mesh;

    // -----------------------------------------------------------------------
    // Movement
    // -----------------------------------------------------------------------

    /**
     * Units advanced along local X per tick. Set via EditDefaultsOnly so it
     * can be tuned in the Blueprint subclass without recompiling.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Settings", meta = (AllowPrivateAccess = "true"))
    float Speed = 2.0f;

    // -----------------------------------------------------------------------
    // Lifetime
    // -----------------------------------------------------------------------

    /** Destroys the projectile. Called by DestroyTimer. */
    void DestroyProjectile();

    /** Seconds before the projectile self-destructs if it hits nothing. */
    UPROPERTY(EditDefaultsOnly, Category = "Settings", meta = (AllowPrivateAccess = "true"))
    float Lifetime = 5.0f;

    FTimerHandle DestroyTimer;

    // -----------------------------------------------------------------------
    // Damage
    // -----------------------------------------------------------------------

    /**
     * Called when SphereCollision overlaps another actor.
     * If the actor is a living AVirusEnemy, deals damage and destroys self.
     */
    UFUNCTION()
    void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent,
                        AActor* OtherActor,
                        UPrimitiveComponent* OtherComp,
                        int32 OtherBodyIndex,
                        bool bFromSweep,
                        const FHitResult& SweepResult);
};
