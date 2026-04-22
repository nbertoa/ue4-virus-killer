#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameOverHUD.h"
#include "PlayerHUD.h"
#include "PlayerPawn.generated.h"

/** Custom log category for APlayerPawn. */
DECLARE_LOG_CATEGORY_EXTERN(LogPlayerPawn, Log, All);

// Forward declarations
class AHealthPickup;
class APlayerProjectile;
class UCameraComponent;
class USpringArmComponent;
class USphereComponent;
class UStaticMeshComponent;
class UArrowComponent;

/**
 * The player-controlled cell pawn.
 *
 * Core mechanic: the player's visual scale IS their health. The cell starts at
 * a designer-defined scale and shrinks each time it fires a projectile
 * (ScaleFactor per shot) or takes damage from an enemy. When scale reaches zero
 * the player is dead. Health pickups restore one ScaleFactor unit.
 *
 * Aiming: the player rotates the ProjectileDirectionArrow each tick to point
 * toward the mouse cursor (via a screen-to-world raycast). Projectiles spawn
 * at the arrow's tip and travel along its local X axis.
 *
 * Input uses the legacy axis/action binding system (UE4-style) to match the
 * Unreal 4 engine version this project was built on.
 */
UCLASS()
class VIRUSKILLER_API APlayerPawn : public APawn
{
    GENERATED_BODY()

public:
    APlayerPawn();

    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    /**
     * Returns true when CurrentScale has reached zero.
     * Used as a guard by AVirusEnemy and AHealthPickup before interacting with the player.
     */
    bool IsDead() const;

    /**
     * Applies one unit of damage (subtracts one ScaleFactor from CurrentScale).
     * Triggers the HUD damage animation, starts the invulnerability timer, and
     * shows the GameOverHUD if the player dies.
     * Must only be called when IsDead() is false and bIsVulnerable is true.
     */
    void OnDamage();

    /**
     * Restores one unit of health (adds one ScaleFactor to CurrentScale),
     * clamped to MaxScale. Called by AHealthPickup::OnBeginOverlap.
     */
    void OnHealthPickup();

protected:
    virtual void BeginPlay() override;

private:
    // -----------------------------------------------------------------------
    // Components
    // -----------------------------------------------------------------------

    /** The player's cell body mesh. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> CellMesh;

    /** Root sphere used for enemy and health-pickup overlap detection. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USphereComponent> SphereCollision;

    /** Top-down follow camera. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> Camera;

    /** Spring arm holding the camera 1200 units above and behind the pawn. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components",
              meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USpringArmComponent> SpringArm;

    /**
     * Arrow that rotates each tick to face the mouse cursor.
     * Projectiles spawn at its tip and travel along its forward vector.
     * Not a UPROPERTY — managed entirely in C++, not exposed to Blueprint.
     */
    TObjectPtr<UArrowComponent> ProjectileDirectionArrow;

    // -----------------------------------------------------------------------
    // Movement
    // -----------------------------------------------------------------------

    /** Updates CurrentXSpeed and CurrentYSpeed by interpolating toward the axis input. */
    void UpdateSpeed(const float DeltaTime);

    /** Applies the current speed vector as a world offset each tick. */
    void UpdateCellLocation(const float DeltaTime);

    /** Returns the target X speed based on the MoveForward axis input value. */
    float GetTargetXSpeed() const;

    /** Returns the target Y speed based on the MoveRight axis input value. */
    float GetTargetYSpeed() const;

    /** Maximum movement speed in cm/tick. Set in Blueprint defaults. */
    UPROPERTY(EditDefaultsOnly, Category = "Settings")
    float MaxSpeed = 0.0f;

    float CurrentXSpeed = 0.0f;
    float CurrentYSpeed = 0.0f;

    // -----------------------------------------------------------------------
    // Aiming & shooting
    // -----------------------------------------------------------------------

    /**
     * Rotates ProjectileDirectionArrow each tick to point toward the mouse cursor
     * using a screen-to-world visibility raycast.
     */
    void UpdateProjectileDirectionArrow();

    /**
     * Performs a visibility-channel raycast under the mouse cursor and returns
     * the world-space impact point.
     */
    FVector GetMouseCursorLocation() const;

    /**
     * Checks CanSpawnProjectile() and, if true, spawns a projectile, deducts
     * health, starts the fire-rate timer, and spawns health pickups if the
     * player is about to fire their last safe shot.
     */
    void TryToSpawnProjectile();

    /** Spawns one APlayerProjectile at the arrow tip, oriented along the arrow. */
    void SpawnProjectile();

    /**
     * Returns true when all three conditions are met:
     *   1. Fire button is held.
     *   2. The fire-rate cooldown has expired.
     *   3. Firing would not reduce CurrentScale to zero or below.
     */
    bool CanSpawnProjectile() const;

    void FireButtonPressed();
    void FireButtonReleased();
    void FireTimerExpired();

    /** Seconds between automatic shots while the fire button is held. */
    UPROPERTY(EditDefaultsOnly, Category = "Projectile")
    float AutoFireRate = 0.2f;

    /** Blueprint subclass of APlayerProjectile to spawn. Set in Blueprint defaults. */
    UPROPERTY(EditDefaultsOnly, Category = "Projectile")
    TSubclassOf<APlayerProjectile> ProjectileClass;

    bool bIsFirePressed = false;
    bool bCanFire       = true;

    FTimerHandle CanFireTimer;

    // -----------------------------------------------------------------------
    // Scale-as-health
    // -----------------------------------------------------------------------

    /** Smoothly interpolates the actor's visual scale toward CurrentScale each tick. */
    void UpdateScale(const float DeltaTime);

    /** Starting scale, derived from the actor's scale at BeginPlay. */
    float MaxScale = 0.0f;

    /** Current health expressed as a scale value. Zero = dead. */
    float CurrentScale = 0.0f;

    /**
     * Amount of health gained/lost per shot or hit event.
     * Set in BeginPlay as MaxScale / 10 so the player has 10 effective "lives."
     */
    float ScaleFactor = 0.0f;

    /**
     * When false, incoming damage is ignored (invulnerability window after a hit).
     * Resets to true after VulnerabilityTimeInterval seconds.
     */
    bool bIsVulnerable = true;

    /** Duration of the post-hit invulnerability window in seconds. */
    float VulnerabilityTimeInterval = 1.0f;

    void MakeVulnerable();
    FTimerHandle VulnerabilityTimer;

    // -----------------------------------------------------------------------
    // Health pickup spawning
    // -----------------------------------------------------------------------

    /**
     * Spawns HealthPickupSpawnCount pickups in random directions around the player.
     * Called when firing would leave the player at zero health.
     */
    void SpawnNewHealthPickup();

    /** Number of health pickups spawned when the player is about to fire their last shot. */
    int32 HealthPickupSpawnCount = 5;

    /** Blueprint subclass of AHealthPickup to spawn. Set in Blueprint defaults. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Settings",
              meta = (AllowPrivateAccess = "true"))
    TSubclassOf<AHealthPickup> HealthPickupClass;

    // -----------------------------------------------------------------------
    // HUD
    // -----------------------------------------------------------------------

    void CreateAndShowPlayerHUD();
    void CreateAndShowGameOverHUD();

    /** Blueprint subclass of UPlayerHUD. Set in Blueprint defaults. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Settings",
              meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UPlayerHUD> PlayerHUDClass;

    /** Live instance of the player HUD widget. */
    TObjectPtr<UPlayerHUD> PlayerHUD;

    /** Blueprint subclass of UGameOverHUD. Set in Blueprint defaults. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Settings",
              meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UGameOverHUD> GameOverHUDClass;

    /** Live instance of the game over HUD widget. Null until the player dies. */
    TObjectPtr<UGameOverHUD> GameOverHUD;

    // -----------------------------------------------------------------------
    // References
    // -----------------------------------------------------------------------

    /** Cached player controller, set in BeginPlay. Raw pointer — not a UObject UPROPERTY. */
    APlayerController* PlayerControllerRef = nullptr;
};
