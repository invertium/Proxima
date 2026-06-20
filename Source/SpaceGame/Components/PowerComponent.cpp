// SpaceGame — bridge simulator. Engineering power model implementation.

#include "Components/PowerComponent.h"

UPowerComponent::UPowerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// One entry per system, all nominal (100%).
	SystemPower.Init(1.0f, static_cast<int32>(EShipSystem::Count));
}

float UPowerComponent::GetSystemPower(EShipSystem System) const
{
	const int32 Index = static_cast<int32>(System);
	return SystemPower.IsValidIndex(Index) ? SystemPower[Index] : 1.0f;
}

void UPowerComponent::SetSystemPower(EShipSystem System, float Value)
{
	const int32 Index = static_cast<int32>(System);
	if (!SystemPower.IsValidIndex(Index))
	{
		return;
	}
	SystemPower[Index] = FMath::Clamp(Value, 0.0f, MaxPerSystem);
	UE_LOG(LogTemp, Log, TEXT("[Power] System %d -> %.0f%% (total %.0f%%)"),
		Index, SystemPower[Index] * 100.f, GetTotalPower() * 100.f);
}

void UPowerComponent::AdjustSystemPower(EShipSystem System, float Delta)
{
	SetSystemPower(System, GetSystemPower(System) + Delta);
}

float UPowerComponent::GetTotalPower() const
{
	float Total = 0.0f;
	for (const float P : SystemPower)
	{
		Total += P;
	}
	return Total;
}
