// SpaceGame — bridge simulator. Player controller implementation.

#include "Core/BridgePlayerController.h"

#include "Core/BridgeHUDWidget.h"
#include "Components/ShipMovementComponent.h"
#include "Components/PowerComponent.h"
#include "Components/WeaponComponent.h"
#include "Ships/Spaceship.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"

void ABridgePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Keys reach the controller while the full-screen HUD overlay is shown.
	bShowMouseCursor = false;

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
	ThrottleLevel = FMath::Clamp(ThrottleLevel + ThrottleStep, 0.f, 1.f);
	ApplyThrottle();
}

void ABridgePlayerController::ThrottleDown()
{
	if (CurrentStation != EStation::Helm) { return; }
	ThrottleLevel = FMath::Clamp(ThrottleLevel - ThrottleStep, 0.f, 1.f);
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
