// SpaceGame — bridge simulator. Ship pawn (simulation core, C++ per DECISIONS D5).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Spaceship.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UShipMovementComponent;
class UPowerComponent;
class UWeaponComponent;
class UTorpedoLauncherComponent;
class UScienceComponent;
class UHealthComponent;
class UAudioComponent;
class USoundBase;

/**
 * ASpaceship — the player's ship. M2: visual hull + 3rd-person follow camera.
 * Movement / subsystems (power, weapons, health) are added in later milestones
 * as UActorComponents so state stays replication-ready (DECISIONS D5/D7).
 */
UCLASS()
class SPACEGAME_API ASpaceship : public APawn
{
	GENERATED_BODY()

public:
	ASpaceship();

	/** Impulse movement sim, for the Helm console / player controller to drive + read. */
	UFUNCTION(BlueprintPure, Category = "Ship")
	UShipMovementComponent* GetMovementComp() const { return MovementComp; }

	/** Engineering power model, for the Engineering console / controller to read + reallocate. */
	UFUNCTION(BlueprintPure, Category = "Ship")
	UPowerComponent* GetPowerComp() const { return PowerComp; }

	/** Forward beam weapon, for the Weapons console / controller to target + fire. */
	UFUNCTION(BlueprintPure, Category = "Ship")
	UWeaponComponent* GetWeaponComp() const { return WeaponComp; }

	/** Limited-ammo torpedo tube (shares the beam's target); Weapons console fires it. */
	UFUNCTION(BlueprintPure, Category = "Ship")
	UTorpedoLauncherComponent* GetTorpedoComp() const { return TorpedoComp; }

	/** Science sensors: scans enemy contacts to reveal their hull/shield. */
	UFUNCTION(BlueprintPure, Category = "Ship")
	UScienceComponent* GetScienceComp() const { return ScienceComp; }

	/** Hull + shield-power mitigation; enemy beams damage this, 0 hull = defeat. */
	UFUNCTION(BlueprintPure, Category = "Ship")
	UHealthComponent* GetHealthComp() const { return HealthComp; }

	/** Add camera-shake "trauma" (0..1, clamped); decays each tick. Drives the follow-cam shake. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Feel")
	void AddCameraTrauma(float Amount);

	// --- Docking (M19): dock at a friendly station to upgrade, repair, and restock ---

	/** True while docked to a station. */
	UFUNCTION(BlueprintPure, Category = "Ship|Dock")
	bool IsDocked() const { return bDocked; }

	/** Whether the ship may dock right now (a station in range + nearly stationary + not docked). */
	UFUNCTION(BlueprintPure, Category = "Ship|Dock")
	bool CanDock() const;

	/** Distance (uu) to the nearest station, or -1 if none. */
	UFUNCTION(BlueprintPure, Category = "Ship|Dock")
	float GetStationRange() const;

	/** Dock if CanDock(): freeze the ship, become combat-safe, repair hull + restock torpedoes.
	 *  Returns true on success. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Dock")
	bool Dock();

	/** Release from the station and return helm control. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Dock")
	void Undock();

	/** Re-apply the ship preset + owned drydock upgrades to the live ship and refill its pools.
	 *  Called after a drydock purchase so a bought upgrade takes effect immediately. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Dock")
	void RefreshLoadout();

	// --- Warp drive (M21): a charged FTL jump along the bow ---

	/** Warp charge 0..1 (builds over time, faster with engine power; 1 = ready). */
	UFUNCTION(BlueprintPure, Category = "Ship|Warp")
	float GetWarpCharge() const { return WarpCharge; }

	/** True when a warp can be triggered (fully charged, not docked). */
	UFUNCTION(BlueprintPure, Category = "Ship|Warp")
	bool IsWarpReady() const { return !bDocked && WarpCharge >= 1.f; }

	/** Jump WarpDistance along the bow if charged; spends the charge. Returns true if it jumped. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Warp")
	bool Warp();

	/** Lay in a course (M23.3): turn the bow toward Target, then warp toward it (clamped so it never
	 *  overshoots, leaving a short standoff). Spends the charge. Returns true if it jumped. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Warp")
	bool WarpToObjective(FVector Target);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Apply the campaign's chosen ship variant (from the game instance): mesh/scale + movement,
	 *  health and weapon stat trade-offs. Interceptor = fast/agile/light; Cruiser = slow/tanky.
	 *  Then layers on any owned drydock upgrades (M19.3). */
	void ApplyShipPreset();

	/** Add owned drydock upgrade magnitudes on top of the base variant stats (M19.3). */
	void ApplyUpgrades();

	/** Bound to HealthComp->OnDamaged: convert a hit into camera trauma. */
	UFUNCTION()
	void HandleDamaged(float EffectiveDamage, float HullRemaining);

	/** Unrotated root so camera/forward stay aligned with actor +X regardless of mesh orientation. */
	UPROPERTY(VisibleAnywhere, Category = "Ship")
	TObjectPtr<USceneComponent> ShipRoot;

	/** Main hull — imported Quaternius "Insurgent" fighter mesh (CC0). */
	UPROPERTY(VisibleAnywhere, Category = "Ship")
	TObjectPtr<UStaticMeshComponent> ShipMesh;

	UPROPERTY(VisibleAnywhere, Category = "Ship|Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Ship|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Impulse movement sim (throttle/turn). Public so test harness / Helm can drive it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship")
	TObjectPtr<UShipMovementComponent> MovementComp;

	/** Engineering power model (D11): per-system power that scales subsystem stats. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship")
	TObjectPtr<UPowerComponent> PowerComp;

	/** Forward beam weapon (recharge scaled by Weapons power). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship")
	TObjectPtr<UWeaponComponent> WeaponComp;

	/** Limited-ammo torpedo launcher (M17); shares the beam weapon's locked target. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship")
	TObjectPtr<UTorpedoLauncherComponent> TorpedoComp;

	/** Science sensors (M17): scans enemy contacts to reveal hull/shield. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship")
	TObjectPtr<UScienceComponent> ScienceComp;

	/** Hull + shield-power mitigation (D11); 0 hull triggers the defeat screen. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship")
	TObjectPtr<UHealthComponent> HealthComp;

	// --- Camera shake (trauma model, M14 game-feel) ---

	/** How fast accumulated trauma bleeds off (per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Feel")
	float TraumaDecayPerSec = 1.6f;

	/** Peak shake angles (deg) at full trauma; actual shake scales with trauma². */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Feel")
	float MaxShakePitch = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Feel")
	float MaxShakeYaw = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Feel")
	float MaxShakeRoll = 2.4f;

	/** Trauma a player hit adds per unit of effective damage taken. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Feel")
	float HitTraumaPerDamage = 0.04f;

	/** Hull-hit SFX (CC0), loaded in the constructor; played 2D when the player takes damage. */
	UPROPERTY(EditAnywhere, Category = "Ship|Feel")
	TObjectPtr<USoundBase> HitSound;

	// --- Ambient audio (M14 nice-to-haves) ---

	/** Looping engine-hum bed; volume + pitch ride throttle so the drive "spools up". */
	UPROPERTY(VisibleAnywhere, Category = "Ship|Feel")
	TObjectPtr<UAudioComponent> EngineAudio;

	/** Looping low-hull klaxon; started/stopped as hull crosses LowHullFraction. */
	UPROPERTY(VisibleAnywhere, Category = "Ship|Feel")
	TObjectPtr<UAudioComponent> AlarmAudio;

	/** Hull fraction (0..1) at/below which the low-hull alarm sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Feel")
	float LowHullFraction = 0.3f;

	/** Max speed (uu/s) at which docking is permitted — you must come to a near-stop to dock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Dock")
	float DockMaxSpeed = 250.f;

	/** Distance (uu) a warp jump covers along the bow. Sized for the open sector (M23) so a jump
	 *  meaningfully shortens the long hops between systems. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Warp")
	float WarpDistance = 18000.f;

	/** Warp charge gained per second at nominal (1.0x) engine power. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Warp")
	float WarpChargeRate = 0.16f;

	// --- Collision / ramming (M22) ---

	/** Hull-to-hull contact distance (uu) when neither ship has shields up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Collision")
	float CollisionRadius = 650.f;

	/** Extra contact distance added per ship that has its shields up — the shield bubble juts past the
	 *  hull, so a shielded ship has a bigger hitbox (and the impact lands on the shield, not the hull). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Collision")
	float ShieldRadiusBonus = 320.f;

	/** Ram damage at a standstill; scales toward 1.5x at full impulse. Applied to the enemy; the player
	 *  takes RamSelfFraction of it — a hard, mutual hit, not a free kill. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Collision")
	float RamDamage = 48.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Collision")
	float RamSelfFraction = 1.0f;

private:
	/** Per-tick ram detection: damage + knock apart on first contact (debounced via TouchingActors). */
	void HandleCollisions(float DeltaSeconds);

	/** Spawn a short burst of spark streaks at an impact point (orange for hull, cyan for a shield hit). */
	void SpawnImpactSparks(const FVector& Location, bool bShieldHit);
	/** Find the nearest AStation in the world (or null). */
	class AStation* NearestStation() const;

	/** True while docked to a station. */
	bool bDocked = false;

	/** Warp drive charge 0..1 (built in Tick, scaled by engine power). */
	float WarpCharge = 0.f;

	/** Enemies currently in contact this frame, so a single collision only damages once. */
	TSet<TWeakObjectPtr<AActor>> TouchingActors;

	/** Emissive material for the warp flash + shield-impact sparks (cyan), loaded in the constructor. */
	UPROPERTY()
	TObjectPtr<class UMaterialInterface> WarpFxMaterial;

	/** Emissive material for hull-impact sparks (orange), loaded in the constructor. */
	UPROPERTY()
	TObjectPtr<class UMaterialInterface> HullSparkMaterial;

	/** Current shake energy 0..1; squared to drive the offset, decayed each tick. */
	float CameraTrauma = 0.f;

	/** Tracks whether the alarm is currently sounding (edge-triggers play/stop). */
	bool bAlarmActive = false;

	/** Per-tick: ride engine-hum volume/pitch on throttle and toggle the low-hull alarm. */
	void UpdateAmbientAudio();
};
