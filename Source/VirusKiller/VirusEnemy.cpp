#include "VirusEnemy.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PlayerPawn.h"

DEFINE_LOG_CATEGORY(LogVirusEnemy);

AVirusEnemy::AVirusEnemy()
{
    PrimaryActorTick.bCanEverTick = true;

    SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
    SetRootComponent(SphereCollision);
    SphereCollision->SetSphereRadius(160.0f);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(GetRootComponent());
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AVirusEnemy::BeginPlay()
{
    Super::BeginPlay();

    if (!ensureMsgf(IsValid(SphereCollision),
                    TEXT("AVirusEnemy::BeginPlay — SphereCollision is null on '%s'."), *GetName()))
    {
        return;
    }

    SphereCollision->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnBeginOverlap);

    // Randomize movement so each enemy feels distinct.
    Speed        = FMath::RandRange(0.007f, 0.05f);
    RotationRate = FMath::RandRange(-1.0f,  1.0f);

    // Each hit removes half the starting scale — enemies always die in exactly two hits.
    ScaleReductionFactor = MaxScale * 0.5f;

    InitPatrolPoints();

    UE_LOG(LogVirusEnemy, Log,
           TEXT("%s: BeginPlay — MaxScale: %.2f | Speed: %.3f | RotationRate: %.3f."),
           *GetName(), MaxScale, Speed, RotationRate);
}

void AVirusEnemy::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    MoveToPatrolPoint(DeltaTime);
    UpdateScale(DeltaTime);
}

// ---------------------------------------------------------------------------
// Patrol movement
// ---------------------------------------------------------------------------

void AVirusEnemy::InitPatrolPoints()
{
    const float LocationOffset = 5000.0f;
    PatrolPoints.Reserve(100);

    for (int32 i = 0; i < 100; ++i)
    {
        PatrolPoints.Add(FVector(FMath::FRandRange(-LocationOffset, LocationOffset),
                                 FMath::FRandRange(-LocationOffset, LocationOffset),
                                 0.0f));
    }
}

void AVirusEnemy::MoveToPatrolPoint(const float DeltaTime)
{
    if (PatrolPoints.IsEmpty())
    {
        return;
    }

    UpdateCurrentPatrolPointIndex();

    const FVector Target     = PatrolPoints[CurrentPatrolPointIndex];
    const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), Target, DeltaTime, Speed);
    SetActorLocation(NewLocation);

    // Constant yaw rotation gives the "spinning cell" visual effect.
    SetActorRotation(FRotator(0.0f, RotationRate, 0.0f));
}

void AVirusEnemy::UpdateCurrentPatrolPointIndex()
{
    if (!PatrolPoints.IsValidIndex(CurrentPatrolPointIndex))
    {
        CurrentPatrolPointIndex = 0;
        return;
    }

    const FVector ToTarget = GetActorLocation() - PatrolPoints[CurrentPatrolPointIndex];
    if (ToTarget.Size() < 50.0f)
    {
        // Arrived — pick a new random destination.
        CurrentPatrolPointIndex = FMath::RandRange(0, PatrolPoints.Num() - 1);
    }
}

// ---------------------------------------------------------------------------
// Scale-as-health
// ---------------------------------------------------------------------------

void AVirusEnemy::UpdateScale(const float DeltaTime)
{
    const float NewScale = FMath::FInterpTo(GetActorScale3D().X, MaxScale, DeltaTime, 0.2f);
    SetActorScale3D(FVector(NewScale, NewScale, NewScale));
}

// ---------------------------------------------------------------------------
// Damage
// ---------------------------------------------------------------------------

void AVirusEnemy::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent,
                                  AActor* OtherActor,
                                  UPrimitiveComponent* OtherComp,
                                  int32 OtherBodyIndex,
                                  bool bFromSweep,
                                  const FHitResult& SweepResult)
{
    APlayerPawn* PlayerPawn = Cast<APlayerPawn>(OtherActor);
    if (!IsValid(PlayerPawn) || PlayerPawn->IsDead())
    {
        return;
    }

    UE_LOG(LogVirusEnemy, Log,
           TEXT("%s: Contacted player '%s'."), *GetName(), *PlayerPawn->GetName());

    PlayerPawn->OnDamage();
}

void AVirusEnemy::OnDamage()
{
    if (!ensureMsgf(!IsDead(),
                    TEXT("AVirusEnemy::OnDamage — Called on already-dead enemy '%s'."), *GetName()))
    {
        return;
    }

    MaxScale -= ScaleReductionFactor;

    UE_LOG(LogVirusEnemy, Log,
           TEXT("%s: OnDamage — MaxScale now %.2f."), *GetName(), MaxScale);

    if (IsDead())
    {
        UE_LOG(LogVirusEnemy, Log,
               TEXT("%s: Died — Spawning %d children and health pickup."),
               *GetName(), EnemyToSpawnCount);

        for (int32 i = 0; i < EnemyToSpawnCount; ++i)
        {
            SpawnNewEnemy();
            SpawnNewHealthPickup();
        }

        Destroy();
    }
    else
    {
        // Damaged but not dead — still reward the player with a health pickup.
        SpawnNewHealthPickup();
    }
}

bool AVirusEnemy::IsDead() const
{
    return MaxScale <= 0.0f;
}

// ---------------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------------

void AVirusEnemy::SpawnNewEnemy()
{
    if (!ensureMsgf(IsDead(),
                    TEXT("AVirusEnemy::SpawnNewEnemy — Must only be called after death on '%s'."),
                    *GetName()))
    {
        return;
    }

    if (!ensureMsgf(EnemyClass != nullptr,
                    TEXT("AVirusEnemy::SpawnNewEnemy — EnemyClass is null on '%s'. "
                         "Assign it in the Blueprint defaults panel."),
                    *GetName()))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!ensureMsgf(IsValid(World),
                    TEXT("AVirusEnemy::SpawnNewEnemy — World is null on '%s'."), *GetName()))
    {
        return;
    }

    const FVector   Location = GetActorLocation();
    const FRotator  Rotation = GetActorRotation();
    World->SpawnActor(EnemyClass.Get(), &Location, &Rotation);
}

void AVirusEnemy::SpawnNewHealthPickup()
{
    if (!ensureMsgf(HealthPickupClass != nullptr,
                    TEXT("AVirusEnemy::SpawnNewHealthPickup — HealthPickupClass is null on '%s'. "
                         "Assign it in the Blueprint defaults panel."),
                    *GetName()))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!ensureMsgf(IsValid(World),
                    TEXT("AVirusEnemy::SpawnNewHealthPickup — World is null on '%s'."), *GetName()))
    {
        return;
    }

    const FVector   Location = GetActorLocation();
    const FRotator  Rotation = GetActorRotation();
    World->SpawnActor(HealthPickupClass.Get(), &Location, &Rotation);
}
