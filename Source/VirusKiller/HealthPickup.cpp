#include "HealthPickup.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PlayerPawn.h"

DEFINE_LOG_CATEGORY(LogHealthPickup);

AHealthPickup::AHealthPickup()
{
    PrimaryActorTick.bCanEverTick = true;

    SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
    SetRootComponent(SphereCollision);
    SphereCollision->SetSphereRadius(80.0f);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(GetRootComponent());
    // Collision disabled on the mesh — all overlap detection goes through SphereCollision.
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHealthPickup::BeginPlay()
{
    Super::BeginPlay();

    if (!ensureMsgf(IsValid(SphereCollision),
                    TEXT("AHealthPickup::BeginPlay — SphereCollision is null on '%s'."), *GetName()))
    {
        return;
    }

    SphereCollision->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnBeginOverlap);

    // Randomize movement so each pickup drifts at a different speed and spins differently.
    Speed        = FMath::RandRange(0.007f, 0.05f);
    RotationRate = FMath::RandRange(-1.0f,  1.0f);

    InitTargetLocation();

    UE_LOG(LogHealthPickup, Log,
           TEXT("%s: BeginPlay — Speed: %.3f | RotationRate: %.3f | Target: %s."),
           *GetName(), Speed, RotationRate, *TargetLocation.ToString());
}

void AHealthPickup::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    MoveToTargetLocation(DeltaTime);
}

void AHealthPickup::InitTargetLocation()
{
    const float LocationOffset = 5000.0f;
    TargetLocation.X = FMath::FRandRange(-LocationOffset, LocationOffset);
    TargetLocation.Y = FMath::FRandRange(-LocationOffset, LocationOffset);
    TargetLocation.Z = 0.0f;
}

void AHealthPickup::MoveToTargetLocation(const float DeltaTime)
{
    const FVector NewLocation = FMath::VInterpTo(GetActorLocation(),
                                                  TargetLocation,
                                                  DeltaTime,
                                                  Speed);
    SetActorLocation(NewLocation);

    // Apply constant yaw rotation for the spinning effect.
    // FRotator constructor: (Pitch, Yaw, Roll)
    SetActorRotation(FRotator(0.0f, RotationRate, 0.0f));
}

void AHealthPickup::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent,
                                   AActor* OtherActor,
                                   UPrimitiveComponent* OtherComp,
                                   int32 OtherBodyIndex,
                                   bool bFromSweep,
                                   const FHitResult& SweepResult)
{
    APlayerPawn* PlayerPawn = Cast<APlayerPawn>(OtherActor);

    // Ignore non-player overlaps and dead players (dead = scale at zero).
    if (!IsValid(PlayerPawn) || PlayerPawn->IsDead())
    {
        return;
    }

    UE_LOG(LogHealthPickup, Log,
           TEXT("%s: Collected by player '%s'."), *GetName(), *PlayerPawn->GetName());

    PlayerPawn->OnHealthPickup();
    Destroy();
}
