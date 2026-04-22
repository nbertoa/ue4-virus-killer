#include "PlayerPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "PlayerProjectile.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogPlayerPawn);

APlayerPawn::APlayerPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
    SphereCollision->ShapeColor = FColor(0, 255, 0);
    SphereCollision->SetSphereRadius(160.0f);
    SetRootComponent(SphereCollision);

    CellMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CellMesh"));
    CellMesh->SetupAttachment(GetRootComponent());

    // Spring arm: pitched -80° for a top-down perspective with a slight angle.
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(GetRootComponent());
    SpringArm->TargetArmLength = 1200.0f;
    SpringArm->SetRelativeRotation(FRotator(-80.0f, 0.0f, 0.0f));
    SpringArm->bEnableCameraLag = true;
    SpringArm->CameraLagSpeed   = 30.0f;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

    // Arrow indicates aim direction — visible in-game so the player has visual feedback.
    ProjectileDirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("ProjectileDirectionArrow"));
    ProjectileDirectionArrow->bHiddenInGame = false;
    ProjectileDirectionArrow->SetupAttachment(GetRootComponent());
}

void APlayerPawn::BeginPlay()
{
    Super::BeginPlay();

    PlayerControllerRef = Cast<APlayerController>(Controller);
    if (!ensureMsgf(IsValid(PlayerControllerRef),
                    TEXT("APlayerPawn::BeginPlay — PlayerController is null on '%s'. "
                         "Ensure this pawn is possessed by a PlayerController."), *GetName()))
    {
        return;
    }

    PlayerControllerRef->bShowMouseCursor = false;
    PlayerControllerRef->SetInputMode(FInputModeGameOnly());

    // Derive health values from the actor's scale at spawn time.
    // MaxScale / 10 gives 10 effective hit points expressed as scale increments.
    MaxScale    = GetActorScale3D().Size();
    CurrentScale = MaxScale;
    ScaleFactor  = MaxScale / 10.0f;

    CreateAndShowPlayerHUD();

    UE_LOG(LogPlayerPawn, Log,
           TEXT("%s: BeginPlay — MaxScale: %.2f | ScaleFactor: %.2f."),
           *GetName(), MaxScale, ScaleFactor);
}

void APlayerPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (IsDead())
    {
        return;
    }

    UpdateCellLocation(DeltaTime);
    UpdateScale(DeltaTime);
    UpdateProjectileDirectionArrow();
    TryToSpawnProjectile();
}

void APlayerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (!ensureMsgf(IsValid(PlayerInputComponent),
                    TEXT("APlayerPawn::SetupPlayerInputComponent — PlayerInputComponent is null on '%s'."),
                    *GetName()))
    {
        return;
    }

    PlayerInputComponent->BindAction("Fire", IE_Pressed,  this, &ThisClass::FireButtonPressed);
    PlayerInputComponent->BindAction("Fire", IE_Released, this, &ThisClass::FireButtonReleased);
    PlayerInputComponent->BindAxis("MoveForward");
    PlayerInputComponent->BindAxis("MoveRight");
}

// ---------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------

float APlayerPawn::GetTargetXSpeed() const
{
    if (!IsValid(InputComponent))
    {
        return 0.0f;
    }

    const float AxisValue = InputComponent->GetAxisValue("MoveForward");
    if (AxisValue > 0.0f)  return  MaxSpeed;
    if (AxisValue < 0.0f)  return -MaxSpeed;
    return 0.0f;
}

float APlayerPawn::GetTargetYSpeed() const
{
    if (!IsValid(InputComponent))
    {
        return 0.0f;
    }

    const float AxisValue = InputComponent->GetAxisValue("MoveRight");
    if (AxisValue > 0.0f)  return  MaxSpeed;
    if (AxisValue < 0.0f)  return -MaxSpeed;
    return 0.0f;
}

void APlayerPawn::UpdateSpeed(const float DeltaTime)
{
    // Interpolate toward target speed for smooth acceleration/deceleration.
    CurrentXSpeed = FMath::FInterpTo(CurrentXSpeed, GetTargetXSpeed(), DeltaTime, 2.0f);
    CurrentYSpeed = FMath::FInterpTo(CurrentYSpeed, GetTargetYSpeed(), DeltaTime, 2.0f);
}

void APlayerPawn::UpdateCellLocation(const float DeltaTime)
{
    UpdateSpeed(DeltaTime);
    AddActorWorldOffset(FVector(CurrentXSpeed, CurrentYSpeed, 0.0f));
}

// ---------------------------------------------------------------------------
// Aiming
// ---------------------------------------------------------------------------

void APlayerPawn::UpdateProjectileDirectionArrow()
{
    if (!ensureMsgf(IsValid(ProjectileDirectionArrow),
                    TEXT("APlayerPawn::UpdateProjectileDirectionArrow — Arrow is null on '%s'."),
                    *GetName()))
    {
        return;
    }

    const FVector MouseLocation = GetMouseCursorLocation();

    // Find the yaw angle from the pawn's position to the mouse world position,
    // then zero out pitch and roll so the arrow stays flat (top-down game).
    const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(
        GetActorLocation(), MouseLocation);

    ProjectileDirectionArrow->SetWorldRotation(FRotator(0.0f, LookAtRotation.Yaw, 0.0f));
}

FVector APlayerPawn::GetMouseCursorLocation() const
{
    if (!ensureMsgf(IsValid(PlayerControllerRef),
                    TEXT("APlayerPawn::GetMouseCursorLocation — PlayerControllerRef is null on '%s'."),
                    *GetName()))
    {
        return GetActorLocation(); // Safe fallback: aim at self
    }

    FHitResult HitResult;
    PlayerControllerRef->GetHitResultUnderCursor(ECC_Visibility, false, HitResult);
    return HitResult.ImpactPoint;
}

// ---------------------------------------------------------------------------
// Shooting
// ---------------------------------------------------------------------------

void APlayerPawn::TryToSpawnProjectile()
{
    if (!CanSpawnProjectile())
    {
        return;
    }

    SpawnProjectile();

    // Each shot costs one ScaleFactor unit of health.
    CurrentScale -= ScaleFactor;

    // If the player can no longer fire safely, spawn compensatory health pickups
    // so they are not immediately trapped without any recovery option.
    if (!CanSpawnProjectile())
    {
        for (int32 i = 0; i < HealthPickupSpawnCount; ++i)
        {
            SpawnNewHealthPickup();
        }
    }

    UWorld* World = GetWorld();
    if (!ensureMsgf(IsValid(World),
                    TEXT("APlayerPawn::TryToSpawnProjectile — World is null on '%s'."), *GetName()))
    {
        return;
    }

    // Start fire-rate cooldown — bCanFire is re-enabled in FireTimerExpired.
    bCanFire = false;
    World->GetTimerManager().SetTimer(CanFireTimer, this, &ThisClass::FireTimerExpired, AutoFireRate);
}

void APlayerPawn::SpawnProjectile()
{
    if (!ensureMsgf(IsValid(ProjectileDirectionArrow),
                    TEXT("APlayerPawn::SpawnProjectile — Arrow is null on '%s'."), *GetName()))
    {
        return;
    }

    if (!ensureMsgf(ProjectileClass != nullptr,
                    TEXT("APlayerPawn::SpawnProjectile — ProjectileClass is null on '%s'. "
                         "Assign it in the Blueprint defaults panel."), *GetName()))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!ensureMsgf(IsValid(World),
                    TEXT("APlayerPawn::SpawnProjectile — World is null on '%s'."), *GetName()))
    {
        return;
    }

    // Spawn slightly ahead of the arrow tip to avoid self-overlap.
    const FVector   SpawnLocation =
        ProjectileDirectionArrow->GetComponentLocation() +
        ProjectileDirectionArrow->GetForwardVector() * 10.0f;
    const FRotator  SpawnRotation = ProjectileDirectionArrow->GetComponentRotation();

    APlayerProjectile* Projectile = World->SpawnActor<APlayerProjectile>(
        ProjectileClass.Get(), SpawnLocation, SpawnRotation);

    if (ensureMsgf(IsValid(Projectile),
                   TEXT("APlayerPawn::SpawnProjectile — SpawnActor returned null on '%s'."),
                   *GetName()))
    {
        Projectile->Shot();
    }
}

bool APlayerPawn::CanSpawnProjectile() const
{
    return bIsFirePressed &&
           bCanFire &&
           (CurrentScale - ScaleFactor > 0.0f); // Firing must not kill the player
}

void APlayerPawn::FireButtonPressed()
{
    bIsFirePressed = true;
    bCanFire       = true;
}

void APlayerPawn::FireButtonReleased()
{
    bIsFirePressed = false;
}

void APlayerPawn::FireTimerExpired()
{
    bCanFire = true;
}

// ---------------------------------------------------------------------------
// Scale-as-health
// ---------------------------------------------------------------------------

void APlayerPawn::UpdateScale(const float DeltaTime)
{
    const float TargetScale = FMath::FInterpTo(GetActorScale3D().X, CurrentScale, DeltaTime, 2.0f);
    SetActorScale3D(FVector(TargetScale, TargetScale, TargetScale));
}

bool APlayerPawn::IsDead() const
{
    return CurrentScale <= 0.0f;
}

void APlayerPawn::OnDamage()
{
    if (!ensureMsgf(!IsDead(),
                    TEXT("APlayerPawn::OnDamage — Called on dead pawn '%s'."), *GetName()))
    {
        return;
    }

    // Invulnerability window prevents consecutive-frame damage stacking.
    if (!bIsVulnerable)
    {
        return;
    }

    CurrentScale -= ScaleFactor;

    UE_LOG(LogPlayerPawn, Log,
           TEXT("%s: OnDamage — CurrentScale now %.2f."), *GetName(), CurrentScale);

    if (ensureMsgf(IsValid(PlayerHUD),
                   TEXT("APlayerPawn::OnDamage — PlayerHUD is null on '%s'."), *GetName()))
    {
        PlayerHUD->PlayDamageAnimation();
    }

    if (IsDead())
    {
        UE_LOG(LogPlayerPawn, Log, TEXT("%s: Dead — Showing GameOverHUD."), *GetName());
        CreateAndShowGameOverHUD();
        return;
    }

    // Start invulnerability window so the player can recover before the next hit.
    bIsVulnerable = false;

    UWorld* World = GetWorld();
    if (ensureMsgf(IsValid(World),
                   TEXT("APlayerPawn::OnDamage — World is null on '%s'."), *GetName()))
    {
        World->GetTimerManager().SetTimer(VulnerabilityTimer,
                                          this,
                                          &ThisClass::MakeVulnerable,
                                          VulnerabilityTimeInterval);
    }
}

void APlayerPawn::MakeVulnerable()
{
    bIsVulnerable = true;
}

void APlayerPawn::OnHealthPickup()
{
    if (!ensureMsgf(!IsDead(),
                    TEXT("APlayerPawn::OnHealthPickup — Called on dead pawn '%s'."), *GetName()))
    {
        return;
    }

    CurrentScale = FMath::Clamp(CurrentScale + ScaleFactor, 0.0f, MaxScale);

    UE_LOG(LogPlayerPawn, Log,
           TEXT("%s: OnHealthPickup — CurrentScale now %.2f."), *GetName(), CurrentScale);
}

void APlayerPawn::SpawnNewHealthPickup()
{
    if (!ensureMsgf(HealthPickupClass != nullptr,
                    TEXT("APlayerPawn::SpawnNewHealthPickup — HealthPickupClass is null on '%s'. "
                         "Assign it in the Blueprint defaults panel."), *GetName()))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!ensureMsgf(IsValid(World),
                    TEXT("APlayerPawn::SpawnNewHealthPickup — World is null on '%s'."), *GetName()))
    {
        return;
    }

    // Spawn pickups in a random outward direction so they scatter around the player.
    FVector RandomDirection(FMath::FRand(), FMath::FRand(), 0.0f);
    RandomDirection.Normalize();

    const FVector  Location = GetActorLocation() + RandomDirection * 150.0f;
    const FRotator Rotation = GetActorRotation();
    World->SpawnActor(HealthPickupClass.Get(), &Location, &Rotation);
}

// ---------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------

void APlayerPawn::CreateAndShowPlayerHUD()
{
    if (!ensureMsgf(PlayerHUDClass != nullptr,
                    TEXT("APlayerPawn::CreateAndShowPlayerHUD — PlayerHUDClass is null on '%s'. "
                         "Assign it in the Blueprint defaults panel."), *GetName()))
    {
        return;
    }

    if (!ensureMsgf(!IsValid(PlayerHUD),
                    TEXT("APlayerPawn::CreateAndShowPlayerHUD — PlayerHUD already exists on '%s'. "
                         "This method should only be called once."), *GetName()))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!ensureMsgf(IsValid(World),
                    TEXT("APlayerPawn::CreateAndShowPlayerHUD — World is null on '%s'."), *GetName()))
    {
        return;
    }

    PlayerHUD = CreateWidget<UPlayerHUD>(World, PlayerHUDClass.Get());
    if (ensureMsgf(IsValid(PlayerHUD),
                   TEXT("APlayerPawn::CreateAndShowPlayerHUD — CreateWidget returned null on '%s'."),
                   *GetName()))
    {
        PlayerHUD->AddToViewport();
        UE_LOG(LogPlayerPawn, Log, TEXT("%s: PlayerHUD created and added to viewport."), *GetName());
    }
}

void APlayerPawn::CreateAndShowGameOverHUD()
{
    if (!ensureMsgf(GameOverHUDClass != nullptr,
                    TEXT("APlayerPawn::CreateAndShowGameOverHUD — GameOverHUDClass is null on '%s'. "
                         "Assign it in the Blueprint defaults panel."), *GetName()))
    {
        return;
    }

    if (!ensureMsgf(!IsValid(GameOverHUD),
                    TEXT("APlayerPawn::CreateAndShowGameOverHUD — GameOverHUD already exists on '%s'."),
                    *GetName()))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!ensureMsgf(IsValid(World),
                    TEXT("APlayerPawn::CreateAndShowGameOverHUD — World is null on '%s'."), *GetName()))
    {
        return;
    }

    GameOverHUD = CreateWidget<UGameOverHUD>(World, GameOverHUDClass.Get());
    if (ensureMsgf(IsValid(GameOverHUD),
                   TEXT("APlayerPawn::CreateAndShowGameOverHUD — CreateWidget returned null on '%s'."),
                   *GetName()))
    {
        GameOverHUD->AddToViewport();
        UE_LOG(LogPlayerPawn, Log, TEXT("%s: GameOverHUD created and added to viewport."), *GetName());
    }
}
