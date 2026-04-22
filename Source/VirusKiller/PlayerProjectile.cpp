#include "PlayerProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "VirusEnemy.h"

DEFINE_LOG_CATEGORY(LogPlayerProjectile);

APlayerProjectile::APlayerProjectile()
{
    PrimaryActorTick.bCanEverTick = true;

    SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
    SetRootComponent(SphereCollision);
    SphereCollision->SetSphereRadius(50.0f);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(GetRootComponent());
    // Mesh has no collision — SphereCollision handles all overlap detection.
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->CastShadow = false;
}

void APlayerProjectile::BeginPlay()
{
    Super::BeginPlay();

    if (!ensureMsgf(IsValid(SphereCollision),
                    TEXT("APlayerProjectile::BeginPlay — SphereCollision is null on '%s'."), *GetName()))
    {
        return;
    }

    SphereCollision->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnBeginOverlap);
}

void APlayerProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Advance along local X each tick. The projectile's rotation is set at
    // spawn time by SpawnActor to match the aim direction arrow, so local X
    // always points toward the target.
    AddActorLocalOffset(FVector(Speed, 0.0f, 0.0f));
}

void APlayerProjectile::Shot()
{
    UWorld* World = GetWorld();
    if (!ensureMsgf(IsValid(World),
                    TEXT("APlayerProjectile::Shot — World is null on '%s'."), *GetName()))
    {
        return;
    }

    World->GetTimerManager().SetTimer(DestroyTimer,
                                      this,
                                      &ThisClass::DestroyProjectile,
                                      Lifetime);

    UE_LOG(LogPlayerProjectile, Log,
           TEXT("%s: Shot — Will self-destruct in %.1f s."), *GetName(), Lifetime);
}

void APlayerProjectile::DestroyProjectile()
{
    UE_LOG(LogPlayerProjectile, Log,
           TEXT("%s: DestroyProjectile — Lifetime expired."), *GetName());
    Destroy();
}

void APlayerProjectile::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent,
                                       AActor* OtherActor,
                                       UPrimitiveComponent* OtherComp,
                                       int32 OtherBodyIndex,
                                       bool bFromSweep,
                                       const FHitResult& SweepResult)
{
    AVirusEnemy* VirusEnemy = Cast<AVirusEnemy>(OtherActor);

    // Ignore non-enemy overlaps and already-dead enemies.
    if (!IsValid(VirusEnemy) || VirusEnemy->IsDead())
    {
        return;
    }

    UE_LOG(LogPlayerProjectile, Log,
           TEXT("%s: Hit enemy '%s'."), *GetName(), *VirusEnemy->GetName());

    VirusEnemy->OnDamage();
    Destroy();
}
