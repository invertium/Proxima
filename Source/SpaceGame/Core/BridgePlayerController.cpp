// SpaceGame — bridge simulator. Player controller implementation.

#include "Core/BridgePlayerController.h"

#include "Core/BridgeHUDWidget.h"
#include "Core/ControlsOverlayWidget.h"
#include "Core/MissionSubsystem.h"
#include "Core/OutcomeMenuWidget.h"
#include "Core/PauseMenuWidget.h"
#include "Core/SpaceGameInstance.h"
#include "Core/SpaceGameMode.h"
#include "Components/ShipMovementComponent.h"
#include "Components/PowerComponent.h"
#include "Components/WeaponComponent.h"
#include "Components/HealthComponent.h"
#include "Components/ScienceComponent.h"
#include "Components/TorpedoLauncherComponent.h"
#include "Ships/Spaceship.h"
#include "Ships/EnemyShip.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"

void ABridgePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Bridge consoles are clickable (mouse/touch) as well as key-driven, so show the
	// cursor and use Game-and-UI input: UMG buttons receive clicks while the gameplay
	// key binds (Helm WASD, Engineering/Weapons arrows + Space) still reach the
	// controller. The console buttons are authored non-focusable so they never steal
	// keyboard focus from those binds.
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	// Fall back to the authored WBP if no class was assigned in defaults. Resolved
	// at play time (not CDO) so creating the WBP doesn't require an editor restart.
	if (!HUDWidgetClass)
	{
		HUDWidgetClass = LoadClass<UBridgeHUDWidget>(
			nullptr, TEXT("/Game/UI/Common/WBP_BridgeHUD.WBP_BridgeHUD_C"));
	}

	if (HUDWidgetClass)
	{
		HUDWidget = CreateWidget<UBridgeHUDWidget>(this, HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Bridge] HUDWidgetClass not set on %s — no HUD shell."), *GetName());
	}

	SetStation(CurrentStation);

	// Watch the placed hostiles: destroying them all is the win condition (M12).
	BindEnemyDeaths();
}

void ABridgePlayerController::BindEnemyDeaths()
{
	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(this, AEnemyShip::StaticClass(), Enemies);
	for (AActor* A : Enemies)
	{
		if (const AEnemyShip* Enemy = Cast<AEnemyShip>(A))
		{
			if (UHealthComponent* Health = Enemy->GetHealthComp())
			{
				Health->OnDeath.AddUniqueDynamic(this, &ABridgePlayerController::HandleEnemyDeath);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Bridge] Tracking %d hostile(s) for win condition"), Enemies.Num());
}

void ABridgePlayerController::HandleEnemyDeath(AActor* DeadActor)
{
	// Bank this kill's salvage to the campaign wallet (and tally the encounter haul for the
	// victory screen). Done before the win check so even the last kill pays out.
	if (const AEnemyShip* Killed = Cast<AEnemyShip>(DeadActor))
	{
		const int32 Cr = Killed->GetRewardCredits();
		const int32 Xp = Killed->GetRewardXP();
		EarnedCredits += Cr;
		EarnedXP += Xp;
		if (USpaceGameInstance* GI = GetGameInstance<USpaceGameInstance>())
		{
			GI->AddReward(Cr, Xp);
		}
	}

	// Count hostiles still alive (the just-killed one reads hull 0 → not alive).
	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(this, AEnemyShip::StaticClass(), Enemies);
	int32 Alive = 0;
	for (AActor* A : Enemies)
	{
		const AEnemyShip* Enemy = Cast<AEnemyShip>(A);
		const UHealthComponent* Health = Enemy ? Enemy->GetHealthComp() : nullptr;
		if (Health && Health->IsAlive())
		{
			++Alive;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Bridge] Hostile destroyed — %d remaining"), Alive);

	// A nearby kill thumps the camera; the shake falls off with distance (M14 game-feel).
	if (ASpaceship* Ship = Cast<ASpaceship>(GetPawn()))
	{
		float Trauma = 0.5f;
		if (DeadActor)
		{
			const float Dist = FVector::Dist(DeadActor->GetActorLocation(), Ship->GetActorLocation());
			Trauma *= FMath::GetMappedRangeValueClamped(FVector2D(2000.f, 12000.f), FVector2D(1.f, 0.f), Dist);
		}
		Ship->AddCameraTrauma(Trauma);
	}

	// Clearing a fleet no longer ends the encounter here — the open-sector director (UMissionSubsystem)
	// owns clears: it advances/saves the campaign, fires the "next objective" comms beat with no reload,
	// and calls OnCampaignComplete() on the *final* clear. This handler only banks salvage + game-feel.
}

void ABridgePlayerController::OnCampaignComplete()
{
	// If the player died on the same beat (defeat overlay already up), don't also bank a win.
	if (Outcome) { return; }

	UE_LOG(LogTemp, Log, TEXT("[Bridge] VICTORY — campaign complete"));
	if (ASpaceGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpaceGameMode>() : nullptr)
	{
		GM->SetPhase(EGamePhase::Victory);
	}

	// Salvage standing for the epilogue (the director already advanced + saved the campaign).
	const USpaceGameInstance* GI = GetGameInstance<USpaceGameInstance>();
	const int32 Rank = GI ? GI->GetRank() : 1;
	const int32 Wallet = GI ? GI->GetCredits() : 0;
	const FString Haul = FString::Printf(
		TEXT("Salvage this run: +%d credits, +%d XP  —  %d credits banked, RANK %d."),
		EarnedCredits, EarnedXP, Wallet, Rank);

	OutcomeKind = EOutcomeKind::VictoryComplete;
	const FLinearColor Green(0.3f, 1.0f, 0.45f, 1.0f);
	const FString Epilogue = FString::Printf(TEXT(
		"The warlord's flagship is gone — and with it the Crimson Pact's grip on the Veil. "
		"For the first time in months the frontier is quiet, because of this crew. "
		"Stand down, Captain. You've earned the calm.\n\n%s"), *Haul);
	ShowOutcome(FText::FromString(TEXT("THE VEIL IS SECURE")), Green,
		FText::FromString(Epilogue),
		TEXT("MAIN MENU"), FString());
}

void ABridgePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Legacy key binds: three number keys, no Input Action assets needed (D9 primitive-first).
	InputComponent->BindKey(EKeys::One,   IE_Pressed, this, &ABridgePlayerController::SelectHelm);
	InputComponent->BindKey(EKeys::Two,   IE_Pressed, this, &ABridgePlayerController::SelectWeapons);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ABridgePlayerController::SelectEngineering);

	// Helm: W/S step throttle; A/D yaw the ship (held). Gated to the Helm station.
	InputComponent->BindKey(EKeys::W, IE_Pressed,  this, &ABridgePlayerController::ThrottleUp);
	InputComponent->BindKey(EKeys::S, IE_Pressed,  this, &ABridgePlayerController::ThrottleDown);
	InputComponent->BindKey(EKeys::A, IE_Pressed,  this, &ABridgePlayerController::TurnLeft);
	InputComponent->BindKey(EKeys::A, IE_Released, this, &ABridgePlayerController::TurnStop);
	InputComponent->BindKey(EKeys::D, IE_Pressed,  this, &ABridgePlayerController::TurnRight);
	InputComponent->BindKey(EKeys::D, IE_Released, this, &ABridgePlayerController::TurnStop);
	// Q/E strafe the ship sideways (held) — evasive side-slip that keeps the bow on target (#7).
	InputComponent->BindKey(EKeys::Q, IE_Pressed,  this, &ABridgePlayerController::StrafeLeft);
	InputComponent->BindKey(EKeys::Q, IE_Released, this, &ABridgePlayerController::StrafeStop);
	InputComponent->BindKey(EKeys::E, IE_Pressed,  this, &ABridgePlayerController::StrafeRight);
	InputComponent->BindKey(EKeys::E, IE_Released, this, &ABridgePlayerController::StrafeStop);

	// Engineering: arrows select a system (Left/Right) + reallocate its power (Up/Down).
	// Gated to the Engineering station, so they don't clash with Helm's WASD.
	InputComponent->BindKey(EKeys::Left,  IE_Pressed, this, &ABridgePlayerController::EngSelectPrev);
	InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &ABridgePlayerController::EngSelectNext);
	InputComponent->BindKey(EKeys::Up,    IE_Pressed, this, &ABridgePlayerController::EngPowerUp);
	InputComponent->BindKey(EKeys::Down,  IE_Pressed, this, &ABridgePlayerController::EngPowerDown);

	// Weapons: Right cycles target, Space fires the beam. Gated to the Weapons station
	// (Right is shared with Engineering — each handler early-outs off-station).
	InputComponent->BindKey(EKeys::Right,     IE_Pressed, this, &ABridgePlayerController::WeaponCycleTarget);
	InputComponent->BindKey(EKeys::SpaceBar,  IE_Pressed, this, &ABridgePlayerController::WeaponFire);

	// Solo-play hotkeys (R2): every verb the web consoles offer, on keys, so one player can
	// run the whole loop without a second screen. Helm/Weapons keys share the station gating.
	InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ABridgePlayerController::HelmDockToggle);
	InputComponent->BindKey(EKeys::B, IE_Pressed, this, &ABridgePlayerController::HelmRedAlertToggle);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ABridgePlayerController::HelmWarp);
	InputComponent->BindKey(EKeys::G, IE_Pressed, this, &ABridgePlayerController::HelmWarpToObjective);
	InputComponent->BindKey(EKeys::T, IE_Pressed, this, &ABridgePlayerController::WeaponFireTorpedo);
	InputComponent->BindKey(EKeys::C, IE_Pressed, this, &ABridgePlayerController::ScienceCycleTarget);
	InputComponent->BindKey(EKeys::X, IE_Pressed, this, &ABridgePlayerController::ScienceScan);

	// H toggles the controls reference card (works alongside any station).
	InputComponent->BindKey(EKeys::H, IE_Pressed, this, &ABridgePlayerController::ToggleControls);

	// Escape toggles the pause overlay (M18). bExecuteWhenPaused so it can also un-pause.
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ABridgePlayerController::TogglePause).bExecuteWhenPaused = true;
}

UShipMovementComponent* ABridgePlayerController::GetShipMovement() const
{
	const ASpaceship* Ship = Cast<ASpaceship>(GetPawn());
	return Ship ? Ship->GetMovementComp() : nullptr;
}

void ABridgePlayerController::ApplyThrottle()
{
	if (UShipMovementComponent* Move = GetShipMovement())
	{
		Move->SetThrottle(ThrottleLevel);
	}
}

void ABridgePlayerController::ThrottleUp()
{
	if (CurrentStation != EStation::Helm) { return; }
	// Lower bound allows reverse (issue #4): S can back the ship past a full stop into reverse
	// thrust, W brings it forward again through zero.
	const float MinT = GetShipMovement() ? GetShipMovement()->GetReverseThrottleMin() : 0.f;
	ThrottleLevel = FMath::Clamp(ThrottleLevel + ThrottleStep, MinT, 1.f);
	ApplyThrottle();
}

void ABridgePlayerController::ThrottleDown()
{
	if (CurrentStation != EStation::Helm) { return; }
	const float MinT = GetShipMovement() ? GetShipMovement()->GetReverseThrottleMin() : 0.f;
	ThrottleLevel = FMath::Clamp(ThrottleLevel - ThrottleStep, MinT, 1.f);
	ApplyThrottle();
}

void ABridgePlayerController::TurnLeft()
{
	if (CurrentStation != EStation::Helm) { return; }
	if (UShipMovementComponent* Move = GetShipMovement()) { Move->SetTurn(-1.f); }
}

void ABridgePlayerController::TurnRight()
{
	if (CurrentStation != EStation::Helm) { return; }
	if (UShipMovementComponent* Move = GetShipMovement()) { Move->SetTurn(1.f); }
}

void ABridgePlayerController::StrafeLeft()
{
	if (CurrentStation != EStation::Helm) { return; }
	if (UShipMovementComponent* Move = GetShipMovement()) { Move->SetStrafe(-1.f); }
}

void ABridgePlayerController::StrafeRight()
{
	if (CurrentStation != EStation::Helm) { return; }
	if (UShipMovementComponent* Move = GetShipMovement()) { Move->SetStrafe(1.f); }
}

void ABridgePlayerController::StrafeStop()
{
	if (UShipMovementComponent* Move = GetShipMovement()) { Move->SetStrafe(0.f); }
}

void ABridgePlayerController::TurnStop()
{
	if (UShipMovementComponent* Move = GetShipMovement()) { Move->SetTurn(0.f); }
}

UPowerComponent* ABridgePlayerController::GetShipPower() const
{
	const ASpaceship* Ship = Cast<ASpaceship>(GetPawn());
	return Ship ? Ship->GetPowerComp() : nullptr;
}

void ABridgePlayerController::EngSelectPrev()
{
	if (CurrentStation != EStation::Engineering) { return; }
	const int32 Count = static_cast<int32>(EShipSystem::Count);
	const int32 Idx = (static_cast<int32>(SelectedSystem) + Count - 1) % Count;
	SelectedSystem = static_cast<EShipSystem>(Idx);
}

void ABridgePlayerController::EngSelectNext()
{
	if (CurrentStation != EStation::Engineering) { return; }
	const int32 Count = static_cast<int32>(EShipSystem::Count);
	const int32 Idx = (static_cast<int32>(SelectedSystem) + 1) % Count;
	SelectedSystem = static_cast<EShipSystem>(Idx);
}

void ABridgePlayerController::EngPowerUp()
{
	if (CurrentStation != EStation::Engineering) { return; }
	if (UPowerComponent* Power = GetShipPower()) { Power->AdjustSystemPower(SelectedSystem, PowerStep); }
}

void ABridgePlayerController::EngAdjustSystem(EShipSystem System, bool bIncrease)
{
	// Mouse/touch entry point for the Engineering console's per-row +/- buttons:
	// select the clicked row (so its highlight follows) and step its power. Shares
	// PowerStep with the arrow keys so both input paths behave identically.
	SelectedSystem = System;
	if (UPowerComponent* Power = GetShipPower())
	{
		Power->AdjustSystemPower(System, bIncrease ? PowerStep : -PowerStep);
	}
}

void ABridgePlayerController::EngPowerDown()
{
	if (CurrentStation != EStation::Engineering) { return; }
	if (UPowerComponent* Power = GetShipPower()) { Power->AdjustSystemPower(SelectedSystem, -PowerStep); }
}

UWeaponComponent* ABridgePlayerController::GetShipWeapon() const
{
	const ASpaceship* Ship = Cast<ASpaceship>(GetPawn());
	return Ship ? Ship->GetWeaponComp() : nullptr;
}

void ABridgePlayerController::WeaponCycleTarget()
{
	if (CurrentStation != EStation::Weapons) { return; }
	if (UWeaponComponent* Weapon = GetShipWeapon()) { Weapon->CycleTarget(); }
}

void ABridgePlayerController::WeaponFire()
{
	if (CurrentStation != EStation::Weapons) { return; }
	if (UWeaponComponent* Weapon = GetShipWeapon()) { Weapon->FireBeam(); }
}

ASpaceship* ABridgePlayerController::GetShip() const
{
	return Cast<ASpaceship>(GetPawn());
}

// --- Solo-play hotkeys (R2) ---

void ABridgePlayerController::HelmRedAlertToggle()
{
	if (CurrentStation != EStation::Helm) { return; }
	if (ASpaceship* Ship = GetShip())
	{
		Ship->ToggleRedAlert();
	}
}

void ABridgePlayerController::HelmDockToggle()
{
	if (CurrentStation != EStation::Helm) { return; }
	ASpaceship* Ship = GetShip();
	if (!Ship) { return; }
	if (Ship->IsDocked())
	{
		Ship->Undock();
	}
	else if (!Ship->Dock())
	{
		UE_LOG(LogTemp, Log, TEXT("[Bridge] Dock refused — no friendly station in range"));
	}
}

void ABridgePlayerController::HelmWarp()
{
	if (CurrentStation != EStation::Helm) { return; }
	if (ASpaceship* Ship = GetShip())
	{
		if (!Ship->Warp())
		{
			UE_LOG(LogTemp, Log, TEXT("[Bridge] Warp refused — %s"),
				Ship->IsDocked() ? TEXT("docked") : TEXT("still charging"));
		}
	}
}

void ABridgePlayerController::HelmWarpToObjective()
{
	if (CurrentStation != EStation::Helm) { return; }
	ASpaceship* Ship = GetShip();
	if (!Ship) { return; }
	// Same course the Helm console's LAY IN COURSE button plots (/api/warp?mode=objective):
	// the director's active objective, falling back to a bow jump if it's unavailable.
	FVector Target = Ship->GetActorLocation() + Ship->GetActorForwardVector() * 1.f;
	if (const UWorld* World = GetWorld())
	{
		if (const UMissionSubsystem* MS = World->GetSubsystem<UMissionSubsystem>())
		{
			Target = MS->GetActiveObjectiveLocation();
		}
	}
	if (!Ship->WarpToObjective(Target))
	{
		UE_LOG(LogTemp, Log, TEXT("[Bridge] Lay-in-course refused — %s"),
			Ship->IsDocked() ? TEXT("docked") : TEXT("warp still charging"));
	}
}

void ABridgePlayerController::WeaponFireTorpedo()
{
	if (CurrentStation != EStation::Weapons) { return; }
	const ASpaceship* Ship = GetShip();
	if (UTorpedoLauncherComponent* Torpedo = Ship ? Ship->GetTorpedoComp() : nullptr)
	{
		Torpedo->Fire();
	}
}

void ABridgePlayerController::ScienceCycleTarget()
{
	// Science has no dedicated station key — any seat can work the scanner.
	const ASpaceship* Ship = GetShip();
	if (UScienceComponent* Science = Ship ? Ship->GetScienceComp() : nullptr)
	{
		Science->CycleTarget();
	}
}

void ABridgePlayerController::ScienceScan()
{
	const ASpaceship* Ship = GetShip();
	if (UScienceComponent* Science = Ship ? Ship->GetScienceComp() : nullptr)
	{
		Science->BeginScan();
	}
}

void ABridgePlayerController::SetStation(EStation NewStation)
{
	// Leaving Helm: stop active steering so the ship doesn't keep yawing unattended
	// (throttle persists — the ship coasts at its set impulse).
	if (CurrentStation == EStation::Helm && NewStation != EStation::Helm)
	{
		if (UShipMovementComponent* Move = GetShipMovement()) { Move->SetTurn(0.f); }
	}

	CurrentStation = NewStation;

	const TCHAR* Name =
		NewStation == EStation::Weapons     ? TEXT("Weapons") :
		NewStation == EStation::Engineering ? TEXT("Engineering") :
		                                      TEXT("Helm");
	UE_LOG(LogTemp, Log, TEXT("[Bridge] Active station -> %s"), Name);

	if (HUDWidget)
	{
		HUDWidget->SetActiveStation(NewStation);
	}
}

void ABridgePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Listen for the player ship's destruction so we can raise the defeat screen.
	if (ASpaceship* Ship = Cast<ASpaceship>(InPawn))
	{
		if (UHealthComponent* Health = Ship->GetHealthComp())
		{
			Health->OnDeath.AddUniqueDynamic(this, &ABridgePlayerController::HandlePlayerDeath);
		}
	}
}

void ABridgePlayerController::HandlePlayerDeath(AActor* DeadActor)
{
	UE_LOG(LogTemp, Log, TEXT("[Bridge] PLAYER DEFEATED — ship destroyed"));

	// Phase flips right away (hostiles stand down off it; the web API reports Defeat), but the
	// overlay waits a short beat so the crew watches the ship go up instead of an instant menu (M24).
	if (ASpaceGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpaceGameMode>() : nullptr)
	{
		GM->SetPhase(EGamePhase::Defeat);
	}
	GetWorldTimerManager().SetTimer(DefeatBeatTimer, this,
		&ABridgePlayerController::ShowDefeatOutcome, DefeatBeatSeconds, false);
}

void ABridgePlayerController::ShowDefeatOutcome()
{
	OutcomeKind = EOutcomeKind::Defeat;
	ShowOutcome(
		FText::FromString(TEXT("DEFEAT")), FLinearColor(1.0f, 0.25f, 0.2f, 1.0f),
		FText::FromString(TEXT("Your ship was destroyed.")),
		TEXT("RETRY"), TEXT("MAIN MENU"));
}

void ABridgePlayerController::ShowOutcome(const FText& Title, FLinearColor Color, const FText& Subtitle,
	const FString& PrimaryLabel, const FString& SecondaryLabel)
{
	if (Outcome) { return; } // already shown — encounter is over

	Outcome = CreateWidget<UOutcomeMenuWidget>(this, UOutcomeMenuWidget::StaticClass());
	if (Outcome)
	{
		Outcome->Setup(Title, Color, Subtitle, PrimaryLabel, SecondaryLabel);
		Outcome->AddToViewport(100); // above the bridge HUD
	}

	// Freeze the encounter and hand input to the UI.
	bShowMouseCursor = true;
	SetInputMode(FInputModeUIOnly());
	UGameplayStatics::SetGamePaused(this, true);
}

// --- Pause overlay (ESC, M18) ---

void ABridgePlayerController::TogglePause()
{
	// Don't let the pause overlay fight the end-of-encounter screen.
	if (Outcome) { return; }
	if (bPaused) { HidePauseMenu(); } else { ShowPauseMenu(); }
}

void ABridgePlayerController::ShowPauseMenu()
{
	if (!PauseMenu)
	{
		PauseMenu = CreateWidget<UPauseMenuWidget>(this, UPauseMenuWidget::StaticClass());
	}
	if (PauseMenu && !PauseMenu->IsInViewport())
	{
		PauseMenu->AddToViewport(120); // above the bridge HUD
	}
	bPaused = true;
	bShowMouseCursor = true;
	SetInputMode(FInputModeUIOnly());
	UGameplayStatics::SetGamePaused(this, true);
}

void ABridgePlayerController::HidePauseMenu()
{
	if (PauseMenu)
	{
		PauseMenu->RemoveFromParent();
		PauseMenu = nullptr; // rebuilt fresh next time (clears any toast)
	}
	bPaused = false;
	UGameplayStatics::SetGamePaused(this, false);
	// Restore the bridge's game-and-UI input (consoles stay clickable + key binds live).
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

// --- Controls reference overlay (H, R2) ---

void ABridgePlayerController::ToggleControls()
{
	if (ControlsOverlay)
	{
		ControlsOverlay->RemoveFromParent();
		ControlsOverlay = nullptr;
		return;
	}
	ControlsOverlay = CreateWidget<UControlsOverlayWidget>(this, UControlsOverlayWidget::StaticClass());
	if (ControlsOverlay)
	{
		ControlsOverlay->AddToViewport(110); // above the HUD, below the pause overlay (120)
	}
}

void ABridgePlayerController::PauseResume()
{
	HidePauseMenu();
}

void ABridgePlayerController::PauseSave()
{
	if (USpaceGameInstance* GI = GetGameInstance<USpaceGameInstance>())
	{
		if (GI->SaveCampaign() && PauseMenu)
		{
			PauseMenu->ShowSavedToast();
		}
	}
}

void ABridgePlayerController::PauseRestart()
{
	UGameplayStatics::SetGamePaused(this, false);
	if (ASpaceGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpaceGameMode>() : nullptr)
	{
		GM->RestartEncounter();
	}
}

void ABridgePlayerController::PauseMainMenu()
{
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, FName(TEXT("MainMenu")));
}

void ABridgePlayerController::PauseQuit()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

// --- Outcome overlay actions (M18 campaign flow) ---

void ABridgePlayerController::OutcomePrimary()
{
	UGameplayStatics::SetGamePaused(this, false);
	switch (OutcomeKind)
	{
	case EOutcomeKind::VictoryNext:
		// MissionIndex was already advanced + saved; reloading the encounter builds the next one.
		UGameplayStatics::OpenLevel(this, FName(TEXT("VSlice_Arena")));
		break;
	case EOutcomeKind::Defeat:
		if (ASpaceGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpaceGameMode>() : nullptr)
		{
			GM->RestartEncounter();
		}
		break;
	case EOutcomeKind::VictoryComplete:
	default:
		UGameplayStatics::OpenLevel(this, FName(TEXT("MainMenu")));
		break;
	}
}

void ABridgePlayerController::OutcomeSecondary()
{
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, FName(TEXT("MainMenu")));
}
