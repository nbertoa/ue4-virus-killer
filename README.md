# ue4-virus-killer

A **top-down shooter prototype** built in Unreal Engine 4 C++ where you play as a cell fighting virus enemies. The core mechanic is that your **visual size is your health bar** — firing projectiles costs health, taking damage shrinks you, and picking up dropped health orbs restores you.

> Based on the Unreal Engine Top-Down Shooter course on Udemy.  
> 📺 [Video demo](https://www.youtube.com/watch?v=SdOsV93q1Ls) · 📝 [Blog post](https://nbertoa.wordpress.com/2023/05/11/unreal-4-c-virus-killer/)

---

## Core Mechanic: Scale as Health

The player has no explicit health integer. Instead, `CurrentScale` is derived from the actor's world scale at spawn time and divided into 10 increments (`ScaleFactor = MaxScale / 10`). Every event that deals or restores health simply adds or subtracts one `ScaleFactor`:

- **Firing a projectile** costs one ScaleFactor — shooting literally shrinks you.
- **Taking damage** from an enemy contact costs one ScaleFactor.
- **Collecting a health pickup** restores one ScaleFactor, clamped to MaxScale.
- **Death** is when `CurrentScale <= 0`.

The actor's visual scale interpolates toward `CurrentScale` each tick via `FInterpTo`, giving hits a satisfying "shrink" feel rather than instant resizing. This same pattern applies to enemies — `MaxScale` IS the enemy's health, and enemies visually grow toward their current max scale.

---

## Architecture

```
APlayerPawn
├── Tick: UpdateCellLocation → UpdateScale → UpdateProjectileDirectionArrow → TryToSpawnProjectile
├── OnDamage()        ← called by AVirusEnemy::OnBeginOverlap
├── OnHealthPickup()  ← called by AHealthPickup::OnBeginOverlap
│
├── APlayerProjectile (spawned per shot)
│   └── OnBeginOverlap → AVirusEnemy::OnDamage()
│
├── UPlayerHUD        (created in BeginPlay, alive until death)
└── UGameOverHUD      (created on death, replaces PlayerHUD)

AVirusEnemy
├── Tick: MoveToPatrolPoint → UpdateScale
├── OnDamage()        ← called by APlayerProjectile::OnBeginOverlap
│   ├── If alive: spawns AHealthPickup
│   └── If dead:  spawns N AVirusEnemy children + N AHealthPickup
└── OnBeginOverlap → APlayerPawn::OnDamage()

AHealthPickup
├── Tick: MoveToTargetLocation
└── OnBeginOverlap → APlayerPawn::OnHealthPickup() → Destroy()
```

---

## Classes

### `APlayerPawn`
The player-controlled cell. Uses legacy UE4 axis/action input bindings (`MoveForward`, `MoveRight`, `Fire`). Aiming works via a screen-to-world visibility raycast each tick that rotates a `UArrowComponent` to face the mouse cursor — projectiles spawn at the arrow's tip and travel along its local X axis.

A fire-rate cooldown is enforced via `FTimerHandle CanFireTimer`. `CanSpawnProjectile()` guards three conditions simultaneously: fire button held, cooldown expired, and firing would not reduce health to zero. When the third condition fails, compensatory health pickups are spawned so the player always has a recovery option before being locked out of firing.

A one-second invulnerability window (`VulnerabilityTimer`) prevents consecutive-frame damage stacking when an enemy stays in contact with the player.

### `AVirusEnemy`
Wanders between 100 pre-generated random patrol points in a 5000-unit field. Speed and rotation rate are randomized per-instance in `BeginPlay` so enemies feel organic. Each hit removes `MaxScale * 0.5f`, meaning enemies always die in exactly two hits regardless of starting size. On death, `EnemyToSpawnCount` (5) smaller enemies are spawned, creating an exponential population dynamic that keeps the arena active.

### `APlayerProjectile`
Tick-driven movement via `AddActorLocalOffset` along local X each frame — no `UProjectileMovementComponent` needed for a flat top-down game with no gravity. `Shot()` must be called by the spawner immediately after `SpawnActor` to start the lifetime destruction timer.

### `AHealthPickup`
Spawned by both `AVirusEnemy` (on any hit) and `APlayerPawn` (when the player is about to fire their last safe shot). Drifts toward a random world position using `VInterpTo` with a randomized speed. Destroys itself on player contact after calling `APlayerPawn::OnHealthPickup()`.

### `UPlayerHUD` / `UGameOverHUD`
Both are `UUserWidget` subclasses. `UPlayerHUD` exposes `PlayDamageAnimation()` as a `BlueprintImplementableEvent` so designers can implement the flash/shake effect without touching C++. `UGameOverHUD` is a pure typed contract — all its content (restart button, score) lives in the Blueprint subclass.

---

## Key Technical Decisions

**Why is scale the health representation?**  
Using a continuous float (`CurrentScale`) instead of integer hit points allows the visual feedback — the cell physically shrinking — to be driven by the same value that determines death. There is no separate "health int → scale mapping" to maintain. The designer tunes `MaxScale` in the Blueprint subclass and gets both the visual size and the health budget for free.

**Why tick-based projectile movement instead of `UProjectileMovementComponent`?**  
`UProjectileMovementComponent` adds gravity simulation, bounciness, and homing capabilities that are irrelevant for a flat top-down game. A single `AddActorLocalOffset` call per tick is simpler, more predictable, and easier to tune via the `Speed` property.

**Why pre-generate 100 patrol points in `BeginPlay` instead of picking one at a time?**  
Having a fixed array means `UpdateCurrentPatrolPointIndex` can jump to any entry in O(1) via `FMath::RandRange`. There is no re-generation cost on arrival. The 100-point pool is large enough that enemies never obviously loop, giving the appearance of continuous wandering.

**Why `CanSpawnProjectile()` as a separate method?**  
The three conditions (fire pressed, cooldown expired, health safe) are checked in two distinct call sites: once before spawning and once before deciding whether to spawn compensatory pickups. Extracting the guard into a named method makes both call sites readable and eliminates the risk of the conditions diverging if one is updated.

---

## Defensive Programming Patterns Used

- `ensureMsgf(IsValid(Ptr), TEXT("..."))` with early return on all UObject pointer accesses.
- Custom log category per system — `LogPlayerPawn`, `LogVirusEnemy`, `LogPlayerProjectile`, `LogHealthPickup`. `LogTemp` does not appear anywhere.
- `TObjectPtr<T>` for all `UPROPERTY` class member pointers.
- Boolean members follow UE naming convention — `bIsFirePressed`, `bCanFire`, `bIsVulnerable`.
- Unused members (`PlayerRef` in `APlayerProjectile`, `WasPatrolPointReached` / `WasPatrolPointSet` in `AVirusEnemy`, `CurrentScale` field shadowing scale logic) were removed.

---

## Project Structure

```
Source/
└── VirusKiller/
    ├── VirusKiller.h / .cpp              # Module entry point
    ├── VirusKiller.Build.cs              # Dependencies (UMG, Slate)
    ├── VirusKillerGameModeBase.h / .cpp
    ├── PlayerPawn.h / .cpp               # Player cell — input, shooting, health
    ├── PlayerProjectile.h / .cpp         # Fired projectile — tick movement, overlap damage
    ├── VirusEnemy.h / .cpp               # Enemy cell — patrol, scale-health, split on death
    ├── HealthPickup.h / .cpp             # Drifting health orb
    ├── PlayerHUD.h / .cpp                # In-game HUD (BlueprintImplementableEvent for damage anim)
    └── GameOverHUD.h / .cpp              # Game over screen
Config/
VirusKiller.uproject
```

---

## Engine Version

Unreal Engine **4** (UE4)

---

## Related

- 📝 [Blog post — nbertoa.wordpress.com](https://nbertoa.wordpress.com/2023/05/11/unreal-4-c-virus-killer/)
- 📺 [Video demo — YouTube](https://www.youtube.com/watch?v=SdOsV93q1Ls)
- 📚 [Learning source — Udemy](https://www.udemy.com/course/jeffers-unreal-topdownshooter/)
