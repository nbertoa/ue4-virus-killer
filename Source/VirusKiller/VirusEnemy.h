#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VirusEnemy.generated.h"

/** Custom log category for AVirusEnemy. */
DECLARE_LOG_CATEGORY_EXTERN(LogVirusEnemy, Log, All);

// Forward declarations
class AHealthPickup;

/**
 * An enemy cell that wanders the arena, shrinks when hit, and splits on death.
 *
 * Health is represented by MaxScale — the enemy's visual size IS its health bar.
 * Each call to OnDamage() reduces MaxScale by ScaleReductionFactor (half of the
 * starting scale), so an enemy always dies in exactly two hits regardless of its
 * initial size.
 *
 * On death the enemy spawns EnemyToSpawnCount smaller enemies (via EnemyClass)
 * at its location, creating an exponential split mechanic that keeps the arena
 * populated. A HealthPickup is also spawned on every hit (damage or death) to
 * give the player a chance to recover.
 *
 * Movement uses random patrol points distributed across a 5000-unit XY field.
 * Speed and rotation rate are randomized per-instance in BeginPlay so enemies
 * feel organic rather than uniform.
 */
UCLASS()
class VIRUSKILLER_API AVirusEnemy : public AActor
{
    GENERATED_BODY()

public:
    AVirusEnemy();

    virtual void Tick(float DeltaTime) override;

    /**
     * Applies one hit to this enemy.
     * Reduces MaxScale by ScaleReductionFactor. If MaxScale reaches zero,
     * spawns child enemies and a health pickup, then destroys self.
     * Must only be called when IsDead() is false.
     */
    void OnDamage();

    /**
     * Returns true when MaxScale has been reduced to zero or below.
     * Used as a guard in OnDamage and by APlayerProjectile before dealing damage.
     */
    bool IsDead() const;

protected:
    virtual void BeginPlay() override;

private:
    // -----------------------------------------------------------------------
    // Components
    // -----------------------------------------------------------------------

    /** Visible enemy mesh. Collision disabled — detection goes through SphereCollision. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UStaticMeshComponent> Mesh;

    /** Sphere trigger for detecting player overlap (deals damage to player on contact). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class USphereComponent> SphereCollision;

    // -----------------------------------------------------------------------
    // Patrol movement
    // -----------------------------------------------------------------------

    /**
     * Selects the next patrol point if the enemy has arrived at the current one,
     * then moves toward it using VInterpTo and applies a yaw rotation each tick.
     *
     * @param DeltaTime  Frame delta for interpolation.
     */
    void MoveToPatrolPoint(const float DeltaTime);

    /**
     * Populates PatrolPoints with 100 random XY positions within ±5000 units.
     * Called once in BeginPlay — points never change after initialization.
     */
    void InitPatrolPoints();

    /**
     * Advances CurrentPatrolPointIndex to a new random point when the enemy
     * is within 50 units of the current target.
     */
    void UpdateCurrentPatrolPointIndex();

    /** Pre-generated list of random world-space XY patrol destinations. */
    TArray<FVector> PatrolPoints;

    /** Index into PatrolPoints for the current movement target. */
    int32 CurrentPatrolPointIndex = 0;

    /** VInterpTo interpolation speed. Randomized in BeginPlay (0.007–0.05). */
    float Speed = 0.0f;

    /** Yaw degrees applied per tick for the spinning effect. Randomized in BeginPlay. */
    float RotationRate = 0.0f;

    // -----------------------------------------------------------------------
    // Scale-as-health
    // -----------------------------------------------------------------------

    /**
     * Smoothly interpolates the actor's visual scale toward MaxScale each tick.
     * This gives hits a satisfying "shrink" feel rather than instant resizing.
     *
     * @param DeltaTime  Frame delta for FInterpTo.
     */
    void UpdateScale(const float DeltaTime);

    /**
     * The enemy's current maximum (target) scale — also represents remaining health.
     * Reduced by ScaleReductionFactor on each hit.
     * EditDefaultsOnly so the designer can tune the starting size per Blueprint subclass.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Settings",
              meta = (AllowPrivateAccess = "true"))
    float MaxScale = 1.0f;

    /**
     * Amount subtracted from MaxScale per hit.
     * Set in BeginPlay as MaxScale * 0.5f so enemies always die in exactly two hits.
     */
    float ScaleReductionFactor = 0.0f;

    /** Number of child enemies spawned when this enemy dies. */
    const int32 EnemyToSpawnCount = 5;

    // -----------------------------------------------------------------------
    // Damage — player contact
    // -----------------------------------------------------------------------

    /**
     * Called when SphereCollision overlaps another actor.
     * If the actor is a living APlayerPawn, calls APlayerPawn::OnDamage().
     */
    UFUNCTION()
    void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent,
                        AActor* OtherActor,
                        UPrimitiveComponent* OtherComp,
                        int32 OtherBodyIndex,
                        bool bFromSweep,
                        const FHitResult& SweepResult);

    // -----------------------------------------------------------------------
    // Spawning
    // -----------------------------------------------------------------------

    /**
     * Spawns EnemyToSpawnCount instances of EnemyClass at this actor's location.
     * Only called when IsDead() is true (asserted inside).
     */
    void SpawnNewEnemy();

    /**
     * Blueprint subclass of AVirusEnemy to spawn on death.
     * Assign in the Blueprint defaults panel.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Settings",
              meta = (AllowPrivateAccess = "true"))
    TSubclassOf<AVirusEnemy> EnemyClass;

    /**
     * Spawns one AHealthPickup at this actor's location.
     * Called both on damage (if still alive) and on death.
     */
    void SpawnNewHealthPickup();

    /**
     * Blueprint subclass of AHealthPickup to spawn on hit/death.
     * Assign in the Blueprint defaults panel.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Settings",
              meta = (AllowPrivateAccess = "true"))
    TSubclassOf<AHealthPickup> HealthPickupClass;
};
