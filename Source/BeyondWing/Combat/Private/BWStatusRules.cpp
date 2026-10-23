#include "BWStatusRules.h"

void UBWStatusRules::ApplyStatus(TArray<FBWStatusInstance>& Statuses, EStatusEffect Type, int32 Duration, int32 Value)
{
	if (Type == EStatusEffect::None || Duration <= 0) return;

	if (FBWStatusInstance* Existing = Statuses.FindByPredicate([Type](const FBWStatusInstance& S) { return S.Type == Type; }))
	{
		Existing->RemainingTurns = Duration;
		Existing->Value = FMath::Max(Existing->Value, Value);
		return;
	}

	FBWStatusInstance NewStatus;
	NewStatus.Type = Type;
	NewStatus.RemainingTurns = Duration;
	NewStatus.Value = Value;
	Statuses.Add(NewStatus);
}

bool UBWStatusRules::HasStatus(const TArray<FBWStatusInstance>& Statuses, EStatusEffect Type)
{
	return Statuses.ContainsByPredicate([Type](const FBWStatusInstance& S) { return S.Type == Type && S.RemainingTurns > 0; });
}

int32 UBWStatusRules::GetStatusValue(const TArray<FBWStatusInstance>& Statuses, EStatusEffect Type)
{
	const FBWStatusInstance* Found = Statuses.FindByPredicate([Type](const FBWStatusInstance& S) { return S.Type == Type && S.RemainingTurns > 0; });
	return Found ? Found->Value : 0;
}

int32 UBWStatusRules::TickEndOfOwnTurn(TArray<FBWStatusInstance>& Statuses)
{
	const int32 BleedDamage = GetStatusValue(Statuses, EStatusEffect::Bleed);

	for (FBWStatusInstance& Status : Statuses) --Status.RemainingTurns;
	Statuses.RemoveAll([](const FBWStatusInstance& S) { return S.RemainingTurns <= 0; });
	return BleedDamage;
}

FBWDiceRange UBWStatusRules::ApplySlow(const FBWDiceRange& SpeedDice)
{
	return FBWDiceRange(SpeedDice.Min, FMath::Max(SpeedDice.Min, SpeedDice.Max - 1));
}

FBWDiceRange UBWStatusRules::ApplyWeak(const FBWDiceRange& SkillRange)
{
	const int32 Max = FMath::FloorToInt(SkillRange.Max * WeakMaxMultiplier);
	return FBWDiceRange(FMath::Min(SkillRange.Min, Max), Max);
}

FString UBWStatusRules::DescribeStatuses(const TArray<FBWStatusInstance>& Statuses)
{
	TArray<FString> Parts;
	for (const FBWStatusInstance& Status : Statuses)
	{
		const TCHAR* Name = TEXT("?");
		switch (Status.Type)
		{
		case EStatusEffect::Stun:       Name = TEXT("기절"); break;
		case EStatusEffect::Slow:       Name = TEXT("둔화"); break;
		case EStatusEffect::Weak:       Name = TEXT("약화"); break;
		case EStatusEffect::Vulnerable: Name = TEXT("취약"); break;
		case EStatusEffect::Bleed:      Name = TEXT("출혈"); break;
		case EStatusEffect::Confusion:  Name = TEXT("혼란"); break;
		case EStatusEffect::Focus:      Name = TEXT("집중"); break;
		default: continue;
		}
		Parts.Add(Status.Type == EStatusEffect::Bleed
			? FString::Printf(TEXT("%s(%d) %d"), Name, Status.Value, Status.RemainingTurns)
			: FString::Printf(TEXT("%s %d"), Name, Status.RemainingTurns));
	}
	return FString::Join(Parts, TEXT(", "));
}
