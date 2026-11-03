#include "BeyondWing/Character/Public/BWCharacter.h"
#include "BeyondWing/Core/Public/BWAutoPlayer.h"
#include "BeyondWing/Core/Public/BWPlayerController.h"
#include "BeyondWing/Combat/Public/DeckComponent.h"
#include "Kismet/GameplayStatics.h"
#include "BeyondWing/Grid/Public/GridManager.h"
#include "BeyondWing/Grid/Public/GridTile.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "Particles/ParticleSystem.h"
#include "BeyondWing/UI/Public/BWFloatingText.h"
#include "Sound/SoundBase.h"
#include "Components/MeshComponent.h"
#include "Components/WidgetComponent.h"
#include "BeyondWing/UI/Public/BWUnitStatusWidget.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

ABWCharacter::ABWCharacter()
{
	PrimaryActorTick.bCanEverTick = true; // [수정] 부드러운 이동 연출을 위해 틱 활성화
	OwnGridCoordinate = FIntPoint(2, 4);
	CharacterClass = EClassType::PHA; 

	PHA = 5;
	INT = 5;
	FTH = 5;
	TEC = 5;
	ATK = 10;
	ASP = 1.0f;
	MaxHP = 100;
	CurrentHP = MaxHP;
	

	CurrentAttackBuff = 0;
	CurrentPatternIndex = 0;

	SmoothMoveSpeed = 15.0f; // 기본 이동 속도

	HitFlashMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/System/Material/M_HitFlash.M_HitFlash")));
	DeathEffect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/KTP_Effect/Particles/Fly/Expolison_03_01.Expolison_03_01")));

	DeckComponent = CreateDefaultSubobject<UDeckComponent>(TEXT("DeckComponent"));
}

void ABWCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 플레이어는 직업별 수치를 쓴다 (적은 GridManager::SpawnEnemies에서 데이터 값으로 덮어씀)
	if (!bIsEnemy) ApplyClassProfile();
	if (UBWAutoPlayer::IsActive()) return;

	// [추가] 머리 위 상태 표시 (HP 바, 방어도, 상태이상, 적의 이번 턴 행동)
	StatusWidgetComp = NewObject<UWidgetComponent>(this, TEXT("StatusWidget"));
	if (StatusWidgetComp)
	{
		StatusWidgetComp->SetupAttachment(GetRootComponent());
		StatusWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
		StatusWidgetComp->SetDrawAtDesiredSize(true);
		StatusWidgetComp->SetPivot(FVector2D(0.5f, 1.0f));
		StatusWidgetComp->SetWidgetClass(UBWUnitStatusWidget::StaticClass());
		StatusWidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		StatusWidgetComp->SetUsingAbsoluteScale(true);
		StatusWidgetComp->SetRelativeLocation(FVector(0, 0, StatusWidgetHeight));
		StatusWidgetComp->RegisterComponent();
		StatusWidgetComp->InitWidget();
		if (UBWUnitStatusWidget* Status = Cast<UBWUnitStatusWidget>(StatusWidgetComp->GetUserWidgetObject())) Status->SetOwnerCharacter(this);
	}
}

void ABWCharacter::ApplyClassProfile()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (GI) SpeedDice = GI->GetClassSpeedDice(CharacterClass);

	// 적에게는 방어 방식이 없고 방어도만 있으므로 플레이어만 직업 방어를 쓴다
	ClassProfile = GI ? GI->GetClassCombatProfile(CharacterClass) : UBWDuelRules::MakeClassProfile(CharacterClass);
	Defense = ClassProfile.Defense;
	MaxMP = ClassProfile.MaxMP;
	CurrentMP = FMath::Min(CurrentMP, MaxMP);
}

void ABWCharacter::ApplyRunState()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI || bIsEnemy) return;

	CharacterClass = GI->SelectedClass;
	PHA = GI->PlayerPHA;
	TEC = GI->PlayerTEC;
	INT = GI->PlayerINT;
	FTH = GI->PlayerFTH;
	ATK = GetEffectiveATK();
	MaxHP = FMath::Max(1, GI->PlayerMaxHP);
	// 판 중의 HP는 전투 사이에 이어진다 (0 이하로 남아 있으면 새로 시작한 것으로 보고 가득 채운다)
	CurrentHP = GI->PlayerCurrentHP > 0 ? FMath::Min(GI->PlayerCurrentHP, MaxHP) : MaxHP;
	GI->PlayerCurrentHP = CurrentHP;

	ApplyClassProfile();
	if (DeckComponent) DeckComponent->LoadDeckByCharacterClass();
}

void ABWCharacter::BeginCombatResources()
{
	CombatTurnNumber = 0;
	Life = FBWLifestyleState();
	ExplosionExtraMP = -1;
	CurrentMP = ClassProfile.bUsesMP ? FMath::Min(ClassProfile.StartMP, ClassProfile.MaxMP) : 0;
	Block += ClassProfile.StartBlock;
}

void ABWCharacter::ApplyAugments(const FBWAugmentTotals& Totals)
{
	AugmentTotals = Totals;

	// 속도를 바꾸는 것은 유물과 증강뿐
	SpeedDice.Max += Totals.SpeedMaxBonus;
	SpeedDice.Min = FMath::Min(SpeedDice.Min + Totals.SpeedMinBonus, SpeedDice.Max);

	Block += Totals.StartBlockBonus;
	if (ClassProfile.bUsesMP) ClassProfile.MPPerTurn += Totals.MPPerTurnBonus;
	DuelRerollsLeft = Totals.DuelRerollsPerBattle;

	if (DeckComponent)
	{
		DeckComponent->InitialHandSize += Totals.HandSizeBonus;
		DeckComponent->MaxEffectCardsPerTurn += Totals.EffectLimitBonus;
		DeckComponent->MaxAttackCardsPerTurn += Totals.AttackCardLimitBonus;
		DeckComponent->MaxDuelsPerTurn += Totals.DuelLimitBonus;
		DeckComponent->InitialHandSize += Totals.GetInt(EBWAugStat::BattleDraw);
		DeckComponent->MaxHandSize += Totals.GetInt(EBWAugStat::HandMax);
		DeckComponent->CardsDrawnPerTurn += Totals.GetInt(EBWAugStat::TurnDraw);
		if (Totals.Has(EBWAugStat::NoEffectCards)) DeckComponent->MaxEffectCardsPerTurn = 0;
	}

	// [추가] 개편안 증강
	AugTrack = FAugmentTrack();
	if (Totals.Has(EBWAugStat::SpeedMinIsOne)) SpeedDice.Min = 1;
	SpeedDice.Max = FMath::Max(1, SpeedDice.Max);
	SpeedDice.Min = FMath::Clamp(SpeedDice.Min, 1, SpeedDice.Max);
	if (Totals.Has(EBWAugStat::StartHeal)) Heal(Totals.GetInt(EBWAugStat::StartHeal));
	if (ClassProfile.bUsesMP)
	{
		ClassProfile.MaxMP += Totals.GetInt(EBWAugStat::MaxMP);
		MaxMP = ClassProfile.MaxMP;
		CurrentMP = FMath::Min(MaxMP, CurrentMP + Totals.GetInt(EBWAugStat::StartMP));
	}
	Defense.ShieldPerMP += Totals.Get(EBWAugStat::ShieldPerUp);
	if (Totals.Has(EBWAugStat::NoDefense))
	{
		Defense.FlatReduction = 0;
		Defense.EvadeChance = 0.0f;
		Defense.MagicShieldMaxMP = 0;
	}
	else
	{
		// 방어 숙련: 삶의 방식의 방어 방식에 맞게 오른다
		const float Up = Totals.Get(EBWAugStat::DefenseUp);
		// 스탯 방어 포인트: 3포인트마다 방어 +1(또는 가드 상한 +1), 회피는 포인트마다 +1%
		const UBWGameInstance* RunGI = Cast<UBWGameInstance>(GetGameInstance());
		const int32 Points = !bIsEnemy && RunGI ? RunGI->PlayerDefensePoints : 0;
		if (Defense.Type == EDefenseType::Evade) Defense.EvadeChance += 0.05f * Up + 0.01f * Points;
		else if (Defense.Type == EDefenseType::MagicShield) Defense.MagicShieldMaxMP += FMath::RoundToInt(Up) + Points / 3;
		else Defense.FlatReduction += FMath::RoundToInt(Up) + Points / 3;
	}
}

void ABWCharacter::GainBlock(int32 Amount, const TCHAR* Label)
{
	if (Amount <= 0) return;
	Block += Amount;
	ShowPopup(FString::Printf(TEXT("%s 방어도 +%d"), Label, Amount), FLinearColor(0.45f, 0.75f, 1.0f));
}

void ABWCharacter::ApplyAugmentsToRange(FBWDiceRange& Range, const FBWCardData* BoostCard, const ABWCharacter* Target) const
{
	if (bIsEnemy) return;
	const FBWAugmentTotals& T = AugmentTotals;
	using S = EBWAugStat;
	int32 Min = T.GetInt(S::AttackMin);
	int32 Max = T.GetInt(S::AttackMax);

	Min += T.GetInt(S::BasicMin);
	Max += T.GetInt(S::BasicMax);
	if (T.Has(S::BasicMul))
	{
		const FBWDiceRange Basic = GetBasicAttackRange();
		Min += Basic.Min * T.GetInt(S::BasicMul);
		Max += Basic.Max * T.GetInt(S::BasicMul);
	}

	const bool bRangedCard = BoostCard && BoostCard->Range >= 2;
	if (bRangedCard) { Min += T.GetInt(S::RangedMin); Max += T.GetInt(S::RangedMax); }
	else Min += T.GetInt(S::MeleeMin);

	if (!AugTrack.bFirstDuelDone) { Min += T.GetInt(S::FirstDuelMinMax); Max += T.GetInt(S::FirstDuelMinMax); }
	if (BoostCard && !AugTrack.bFirstCardDone) Max += T.GetInt(S::FirstCardMax);
	if (!Life.bMovedThisTurn) { Min += T.GetInt(S::NoMoveMinMax); Max += T.GetInt(S::NoMoveMinMax); }
	const bool bTaigaMiracle = T.SetCounts.IsValidIndex((int32)EBWAugmentSet::Taiga) && T.SetCounts[(int32)EBWAugmentSet::Taiga] >= 4;
	Max += FMath::Min(AugTrack.MovedTiles * T.GetInt(S::MovedMaxPerTile), bTaigaMiracle ? 99 : 5);
	if (RemainingMove > 0 || AugTrack.bMoveLeftAtDuel) Min += T.GetInt(S::MoveLeftMin);
	if (AugTrack.EffectsThisTurn >= 3) Max += T.GetInt(S::EffectThreeMax);
	const int32 BlockBasic = (Block / 5) * T.GetInt(S::BlockToBasic); // 무게 중심
	Min += BlockBasic;
	Max += BlockBasic;
	if (DeckComponent && DeckComponent->Hand.Num() <= 3) Max += T.GetInt(S::LowHandMax);
	if (Statuses.ContainsByPredicate([](const FBWStatusInstance& St) { return UBWAugmentRules::IsDebuff(St.Type); }) == false) Min += T.GetInt(S::NoDebuffMin);
	if (CombatTurnNumber % 2 == 1) Max += T.GetInt(S::OddTurnMax);
	if (AugTrack.EffectsThisTurn == 0) Max += T.GetInt(S::NoEffectMax);
	if (CombatTurnNumber <= 3) { Min += T.GetInt(S::Early3MinMax); Max += T.GetInt(S::Early3MinMax); }
	else { Min -= T.GetInt(S::Late4MinMax); Max -= T.GetInt(S::Late4MinMax); }
	if (Block >= 10) Max += T.GetInt(S::Block10Max);
	Max += AugTrack.EffectsThisTurn * T.GetInt(S::EffectMaxPer);
	if (const UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance())) Min += FMath::Min(4, GI->CurrentGold / 100) * T.GetInt(S::GoldMinPer100);

	if (Target)
	{
		if (Target->CurrentHP >= Target->MaxHP) Max += T.GetInt(S::FullHPMax);
		if (Target->CurrentHP * 4 <= Target->MaxHP) { Min += T.GetInt(S::LowHPMinMax); Max += T.GetInt(S::LowHPMinMax); }
		if (MaxHP > 0 && Target->MaxHP > 0 && (float)CurrentHP / MaxHP < (float)Target->CurrentHP / Target->MaxHP) Min += T.GetInt(S::BehindMin);
		int32 Debuffs = 0;
		for (const FBWStatusInstance& St : Target->Statuses) Debuffs += UBWAugmentRules::IsDebuff(St.Type) ? 1 : 0;
		if (Debuffs > 0) Min += T.GetInt(S::StatusedMin);
		Max += Debuffs * T.GetInt(S::PerStatusMax);
	}

	Range.Min += Min;
	Range.Max += Max;
	Range.Max = FMath::FloorToInt(Range.Max * (1.0f + T.Get(S::AttackMaxPct)));
	if (T.Has(S::MinIsOne)) Range.Min = 1;
}

void ABWCharacter::NoteEffectCardUsed()
{
	++AugTrack.EffectsThisTurn;
	if (AugmentTotals.Has(EBWAugStat::TwoEffectDraw) && AugTrack.EffectsThisTurn == 2 && DeckComponent) DeckComponent->DrawCards(1);
}

void ABWCharacter::ApplyAugmentsAfterWin(ABWCharacter* Enemy, const FBWCardData& Card, bool bCardBoost, int32 Dealt)
{
	const FBWAugmentTotals& T = AugmentTotals;
	using S = EBWAugStat;
	if (T.Has(S::Lifesteal) && Dealt > 0) Heal(FMath::FloorToInt(Dealt * T.Get(S::Lifesteal)));
	if (!bCardBoost || Card.Range <= 1) GainBlock(T.GetInt(S::MeleeWinBlock), TEXT("베렌드"));
	if (!Enemy) return;

	if (Enemy->CurrentHP > 0 && !Enemy->IsActorBeingDestroyed())
	{
		if (Card.AppliedStatus == EStatusEffect::None)
		{
			if (T.Has(S::WinBleed)) Enemy->ApplyStatus(EStatusEffect::Bleed, 2, T.GetInt(S::WinBleed));
			if (T.Has(S::WinVuln)) Enemy->ApplyStatus(EStatusEffect::Vulnerable, T.GetInt(S::WinVuln), 0);
		}
		if (T.Has(S::WinSlow) && bCardBoost && Card.Range >= 2) Enemy->ApplyStatus(EStatusEffect::Slow, T.GetInt(S::WinSlow), 0);
		return;
	}

	// 적을 쓰러뜨렸다
	if (T.Has(S::KillHeal)) Heal(T.GetInt(S::KillHeal));
	RemainingMove += T.GetInt(S::KillMove);
	if (T.Has(S::KillDraw) && DeckComponent) DeckComponent->DrawCards(T.GetInt(S::KillDraw));
	if (T.Has(S::KillRefund) && !AugTrack.bKillRefundUsed && DeckComponent)
	{
		AugTrack.bKillRefundUsed = true;
		DeckComponent->AttackCardsUsedThisTurn = 0;
		DeckComponent->DuelsOpenedThisTurn = 0;
		ShowPopup(TEXT("처치 연쇄"), FLinearColor(1.0f, 0.85f, 0.4f));
	}
}

void ABWCharacter::ApplyAugmentsEndTurn()
{
	GainBlock(AugmentTotals.GetInt(EBWAugStat::TurnEndBlock), TEXT("증강"));
}

void ABWCharacter::BeginTurnResources()
{
	++CombatTurnNumber;
	TurnAttackMaxBonus = 0;
	CoverShield = 0;
	UBWDuelRules::ApplyTurnStartResources(ClassProfile, CombatTurnNumber, CurrentMP);

	// [추가] 증강: 턴 시작
	AugTrack.MovedTiles = 0;
	AugTrack.bExtraDuelUsed = false;
	AugTrack.EffectsThisTurn = 0;
	AugTrack.bTurnFirstDuelDone = false;
	AugTrack.bKillRefundUsed = false;
	if (AugmentTotals.Has(EBWAugStat::TurnHeal) && CombatTurnNumber > 1) Heal(AugmentTotals.GetInt(EBWAugStat::TurnHeal));
	if (AugmentTotals.Has(EBWAugStat::TurnHPLoss)) TakeFixedDamage(AugmentTotals.GetInt(EBWAugStat::TurnHPLoss), TEXT("대가"));
	if (CombatTurnNumber % 2 == 0) GainBlock(AugmentTotals.GetInt(EBWAugStat::EvenTurnBlock), TEXT("전투 리듬"));
	if (CombatTurnNumber == 1 && AugmentTotals.Has(EBWAugStat::FirstTurnFocus)) ApplyStatus(EStatusEffect::Focus, 1, 0);

	// 삶의 방식: 지난 적 턴 동안의 버프는 끝나고, 걸어 둔 것(영창·폭풍 질주·들림)은 이번 턴에 켜진다
	Life.Counter = 0;
	Life.EvadeBonus = 0.0f;
	Life.DefenseBonus = 0;
	Life.bGaleGuard = false;
	Life.bBloodRevenge = Life.bRageGuard && Life.bRageGuardHit;
	Life.bRageGuard = false;
	Life.bRageGuardHit = false;
	Life.TurnCritBonus = 0.0f;
	Life.bNoDuelThisTurn = false;
	Life.bMovedThisTurn = false;
	Life.bFiredMagicThisTurn = false;
	Life.bPossessed = Life.bPossessedNext;
	Life.bPossessedNext = false;
	Life.ChantReady = Life.ChantPending;
	Life.ChantPending = 0;
	Life.bStormReady = Life.bStormPending;
	Life.bStormPending = false;
	if (Life.GaleCooldown > 0) --Life.GaleCooldown;
	ExplosionExtraMP = -1;

	// 테네브리스: 턴 시작마다 어둠 1당 자해
	if (Life.bTenebris && Life.Dark > 0) TakeFixedDamage(Life.Dark * UBWLifestyleRules::TenebrisSelfDamagePerDark, TEXT("테네브리스"));
}

bool ABWCharacter::IsRaging() const
{
	return !bIsEnemy && CharacterClass == EClassType::PHA && UBWLifestyleRules::IsRaging(CurrentHP, MaxHP);
}

FString ABWCharacter::GetLifestyleStatusText() const
{
	return bIsEnemy ? FString() : UBWLifestyleRules::Describe(CharacterClass, Life, IsRaging());
}

void ABWCharacter::SetTenebris(bool bOn)
{
	if (Life.bTenebris == bOn) return;
	Life.bTenebris = bOn;
	if (DeckComponent)
	{
		for (TArray<FBWCardData>* Pile : { &DeckComponent->Deck, &DeckComponent->Hand, &DeckComponent->Discard })
		{
			for (FBWCardData& Card : *Pile)
			{
				if (Card.Special != EBWCardSpecial::HolyHeal) continue;
				Card = bOn ? UBWLifestyleRules::MakeTenebrisAttack(Card, FTH) : UBWLifestyleRules::RestoreFromTenebris(Card);
			}
		}
		DeckComponent->OnHandChanged.Broadcast(DeckComponent->Hand);
	}
	ShowPopup(bOn ? TEXT("테네브리스") : TEXT("빛으로 돌아옴"), bOn ? FLinearColor(0.55f, 0.25f, 0.85f) : FLinearColor(0.85f, 0.9f, 1.0f), 1.2f);
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Tenebris %s (dark %d)"), bOn ? TEXT("ON") : TEXT("OFF"), Life.Dark);
}

void ABWCharacter::AddLiberation(int32 Amount)
{
	if (Life.bLiberated || Amount <= 0) return;
	Life.Liberation += Amount;
	if (Life.Liberation < UBWLifestyleRules::LiberationNeeded - AugmentTotals.GetInt(EBWAugStat::LiberationDown)) return;
	Life.bLiberated = true;
	ClassProfile.MaxMP += UBWLifestyleRules::LiberatedMaxMPBonus;
	ClassProfile.MPPerTurn += 1;
	MaxMP = ClassProfile.MaxMP;
	ShowPopup(TEXT("해방"), FLinearColor(0.45f, 0.7f, 1.0f), 1.3f);
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Liberation! max MP %d, MP per turn %d"), MaxMP, ClassProfile.MPPerTurn);
}

int32 ABWCharacter::Heal(int32 Amount, bool bFromCard)
{
	// 광전사 광기: 회복 카드의 효과는 절반
	if (bFromCard) Amount = UBWLifestyleRules::ScaleCardHeal(Amount, IsRaging());
	if (!bIsEnemy)
	{
		if (AugmentTotals.Has(EBWAugStat::NoHeal) || (bFromCard && AugmentTotals.Has(EBWAugStat::NoCardHeal))) return 0;
		Amount = FMath::FloorToInt(Amount * (1.0f + AugmentTotals.Get(EBWAugStat::HealPct)));
	}
	if (CurrentHP <= 0 || Amount <= 0) return 0;
	const int32 Before = CurrentHP;
	CurrentHP = FMath::Min(MaxHP, CurrentHP + Amount);
	const int32 Healed = CurrentHP - Before;

	// [추가] 증강: 넘친 회복·회복한 만큼 방어도, 회복하면 이번 턴 공격 최댓값 +, 회복이 적 전체 피해로
	if (!bIsEnemy)
	{
		if (AugmentTotals.Has(EBWAugStat::OverhealBlock)) GainBlock(Amount - Healed, TEXT("넘친 회복"));
		if (AugmentTotals.Has(EBWAugStat::HealToBlock)) GainBlock(Healed, TEXT("회복"));
		TurnAttackMaxBonus += Healed > 0 ? AugmentTotals.GetInt(EBWAugStat::HealAttackMax) : 0;
		if (Healed > 0) if (ABWPlayerController* EventPC = Cast<ABWPlayerController>(GetController())) EventPC->OnSetHeal(Healed);
		if (AugmentTotals.Has(EBWAugStat::HealToAoE) && Healed > 0)
		{
			TArray<AActor*> Actors;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), Actors);
			for (AActor* Actor : Actors)
			{
				ABWCharacter* Other = Cast<ABWCharacter>(Actor);
				if (Other && Other->bIsEnemy && Other->CurrentHP > 0) Other->TakeFixedDamage(Healed, TEXT("빛의 의식"));
			}
		}
	}

	if (UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance()); GI && !bIsEnemy)
	{
		GI->PlayerCurrentHP = FMath::Min(GI->PlayerMaxHP, GI->PlayerCurrentHP + Healed);
	}
	ShowPopup(FString::Printf(TEXT("+%d"), Healed), FLinearColor(0.35f, 1.0f, 0.45f));
	return Healed;
}

void ABWCharacter::EndTurnResources()
{
	UBWDuelRules::ApplyTurnEndResources(ClassProfile, Block);

	// 마탄의 사수: 들린 채 마탄을 쏘지 않고 턴을 끝내면 악마가 대신 쏜다
	if (Life.bPossessed && !Life.bFiredMagicThisTurn && CurrentHP > 0)
	{
		TakeFixedDamage(FMath::Max(1, Life.MagicCount * UBWLifestyleRules::PossessedPenaltyPerShot), TEXT("악마의 탄환"));
	}
	// 어둠의 성자: 테네브리스의 어둠은 내 턴이 끝날 때마다 1씩 줄고, 0이면 빛으로 돌아온다
	if (Life.bTenebris)
	{
		Life.Dark = FMath::Max(0, Life.Dark - 1);
		if (Life.Dark == 0) SetTenebris(false);
	}
	Life.bBloodRevenge = false;
}

void ABWCharacter::ApplyStatus(EStatusEffect Type, int32 Duration, int32 Value)
{
	// [추가] 증강: 나에게 걸리는 나쁜 상태 저항
	if (!bIsEnemy && UBWAugmentRules::IsDebuff(Type))
	{
		const FBWAugmentTotals& T = AugmentTotals;
		const bool bStunGuard = Type == EStatusEffect::Stun && T.Has(EBWAugStat::StunGuard) && !AugTrack.bStunGuardUsed;
		if (bStunGuard) AugTrack.bStunGuardUsed = true;
		if (bStunGuard || T.Has(EBWAugStat::DebuffImmune) || (Type == EStatusEffect::Confusion && T.Has(EBWAugStat::NoConfuse)))
		{
			ShowPopup(TEXT("저항"), FLinearColor(0.7f, 0.9f, 1.0f));
			return;
		}
		Duration = FMath::Max(1, Duration - T.GetInt(EBWAugStat::DebuffShort));
		if (ABWPlayerController* EventPC = Cast<ABWPlayerController>(GetController())) EventPC->OnSetDebuffed(Type, Duration, Value);
	}
	UBWStatusRules::ApplyStatus(Statuses, Type, Duration, Value);
	TArray<FBWStatusInstance> Single;
	UBWStatusRules::ApplyStatus(Single, Type, Duration, Value);
	ShowPopup(UBWStatusRules::DescribeStatuses(Single), FLinearColor(0.7f, 0.5f, 1.0f));
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s gets %s (%d turns, value %d) -> [%s]"),
		bIsEnemy ? *EnemyDisplayName : TEXT("Player"), *UEnum::GetValueAsString(Type), Duration, Value, *UBWStatusRules::DescribeStatuses(Statuses));
}

bool ABWCharacter::HasStatus(EStatusEffect Type) const
{
	return UBWStatusRules::HasStatus(Statuses, Type);
}

int32 ABWCharacter::EndOwnTurnStatuses()
{
	int32 Bleed = UBWStatusRules::TickEndOfOwnTurn(Statuses);
	if (!bIsEnemy) Bleed = FMath::Max(0, Bleed - AugmentTotals.GetInt(EBWAugStat::BleedTakenDown));
	if (Bleed > 0 && CurrentHP > 0) TakeFixedDamage(Bleed, TEXT("출혈"));
	return Bleed;
}

int32 ABWCharacter::TakeFixedDamage(int32 Amount, const FString& Label)
{
	if (Amount <= 0 || CurrentHP <= 0) return 0;
	// [추가] 증강: 자해 감소·방어도·회복으로 뒤집기
	if (!bIsEnemy && Label == TEXT("자해"))
	{
		Amount = FMath::Max(1, Amount - AugmentTotals.GetInt(EBWAugStat::SelfDamageDown));
		GainBlock(AugmentTotals.GetInt(EBWAugStat::SelfDamageBlock), TEXT("레디언트"));
		if (ABWPlayerController* EventPC = Cast<ABWPlayerController>(GetController())) EventPC->OnSetSelfDamage(Amount);
		if (AugmentTotals.Has(EBWAugStat::SelfDamageHeals)) { Heal(Amount); return 0; }
	}
	// 고정 피해: 방어·회피·매직실드 없이 방어도 → HP
	const int32 HPBefore = CurrentHP;
	UBWDuelRules::ApplyDamageToBlockAndHP(Amount, false, Block, CurrentHP);
	const int32 Lost = HPBefore - CurrentHP;
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (GI && !bIsEnemy) GI->PlayerCurrentHP = FMath::Max(0, GI->PlayerCurrentHP - Lost);
	if (!bIsEnemy) Life.Wounds += Amount; // 광전사 상처: 자해·출혈 등 고정 피해의 누계
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s takes %d %s damage (HP %d -> %d)"), bIsEnemy ? *EnemyDisplayName : TEXT("Player"), Amount, *Label, HPBefore, CurrentHP);
	ShowPopup(FString::Printf(TEXT("%s -%d"), *Label, Lost), FLinearColor(0.8f, 0.1f, 0.1f));
	if (Lost > 0) PlayHitReaction(GetActorLocation() + GetActorForwardVector() * 50.0f, false);
	if (CurrentHP <= 0) Die();
	else
	{
		if (Lost > 0) BreakChant();
		CheckEnemyPhase();
	}
	return Lost;
}

void ABWCharacter::ApplyTerrainToDuelSide(FBWDuelSide& Side) const
{
	const AGridManager* Grid = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
	if (Grid && Grid->GetTerrainAt(OwnGridCoordinate) == EBWTerrainType::Resonance) Side.SkillRange.Max += UBWTerrainRules::ResonanceMaxBonus;
}

void ABWCharacter::SkipTurnForStun()
{
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s is stunned and skips its turn"), *EnemyDisplayName);
	AdvancePattern();
}

int32 ABWCharacter::RollSpeed()
{
	// 둔화: 최댓값 -1
	const FBWDiceRange Dice = HasStatus(EStatusEffect::Slow) ? UBWStatusRules::ApplySlow(SpeedDice) : SpeedDice;
	RolledSpeed = FMath::RandRange(Dice.Min, FMath::Max(Dice.Min, Dice.Max)) + RushSpeed;
	RushSpeed = 0;
	RemainingMove = RolledSpeed;
	return RolledSpeed;
}

void ABWCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateReactions(DeltaTime);

	// 부드러운 이동 연출 (Queue 방식)
	if (VisualMoveQueue.Num() > 0)
	{
		FVector CurrentLoc = GetActorLocation();
		FVector TargetLoc = VisualMoveQueue[0];

		// [추가] 걸어가는 방향을 바라본다 (찌르고 돌아오는 단계는 방향을 유지)
		const FVector Flat = FVector(TargetLoc.X - CurrentLoc.X, TargetLoc.Y - CurrentLoc.Y, 0.0f);
		if (VisualMoveFacing.IsValidIndex(0) && VisualMoveFacing[0] && Flat.SizeSquared() > 25.0f)
		{
			SetActorRotation(FRotator(0.0f, Flat.Rotation().Yaw, 0.0f));
		}

		FVector NewLoc = FMath::VInterpTo(CurrentLoc, TargetLoc, DeltaTime, SmoothMoveSpeed);
		SetActorLocation(NewLoc);

		if (FVector::Dist(NewLoc, TargetLoc) < 5.0f)
		{
			SetActorLocation(TargetLoc); // 위치 보정
			VisualMoveQueue.RemoveAt(0);
			if (VisualMoveFacing.Num() > 0) VisualMoveFacing.RemoveAt(0);
		}
	}
}

void ABWCharacter::AddMovementStep(FVector TargetWorldLoc, bool bTurnToFace)
{
	if (UBWAutoPlayer::IsActive()) return; // 측정 봇: 위치는 칸 좌표로만 쓴다
	VisualMoveQueue.Add(TargetWorldLoc);
	VisualMoveFacing.Add(bTurnToFace);
}

void ABWCharacter::FaceTowards(const FVector& WorldLocation)
{
	const FVector Flat = FVector(WorldLocation.X - GetActorLocation().X, WorldLocation.Y - GetActorLocation().Y, 0.0f);
	if (Flat.SizeSquared() > 1.0f) SetActorRotation(FRotator(0.0f, Flat.Rotation().Yaw, 0.0f));
}


const FBWEnemyPattern* ABWCharacter::GetCurrentAction() const
{
	if (CombatPatterns.Num() == 0) return nullptr;
	const int32 Idx = CombatPatterns.IsValidIndex(CurrentPatternIndex) ? CurrentPatternIndex : 0;
	return CombatPatterns[Idx].Actions.Num() > 0 ? &CombatPatterns[Idx].Actions[0] : nullptr;
}

void ABWCharacter::AdvancePattern()
{
	if (CombatPatterns.Num() > 0) CurrentPatternIndex = (CurrentPatternIndex + 1) % CombatPatterns.Num();
	LockedAttackTiles.Empty();
	LockedMovePath.Empty();
	bIntentLocked = false;
}

ABWCharacter* ABWCharacter::FindNearestOpponent() const
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), FoundActors);
	ABWCharacter* Best = nullptr;
	int32 BestDist = MAX_int32;
	for (AActor* Actor : FoundActors)
	{
		ABWCharacter* Char = Cast<ABWCharacter>(Actor);
		if (!Char || Char->bIsEnemy == bIsEnemy || Char->CurrentHP <= 0 || Char->IsActorBeingDestroyed()) continue;
		const int32 Dist = FMath::Abs(Char->OwnGridCoordinate.X - OwnGridCoordinate.X) + FMath::Abs(Char->OwnGridCoordinate.Y - OwnGridCoordinate.Y);
		if (Dist < BestDist) { BestDist = Dist; Best = Char; }
	}
	return Best;
}

void ABWCharacter::UpdateEnemyIntent()
{
	// [수정] 12장·4-3: 공격 행동이면 이번 턴 굴린 속도로 나를 향해 최단 경로로 돌진하는 길을 미리 보여 준다
	LockedAttackTiles.Empty();
	LockedMovePath.Empty();
	bIntentLocked = false;
	if (!bIsEnemy || CurrentHP <= 0) return;

	const FBWEnemyPattern* Action = GetCurrentAction();
	bIntentLocked = true;
	if (!Action || Action->ActionType == EBWEnemyActionType::Heal || Action->ActionType == EBWEnemyActionType::Block
		|| Action->ActionType == EBWEnemyActionType::Summon || IsStunned()) return; // 제자리 행동, 기절하면 아무것도 안 함

	AGridManager* GridManager = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
	ABWCharacter* Target = FindNearestOpponent();
	if (!GridManager || !Target) return;

	LockedMovePath = GridManager->FindEnemyMovePath(this, Target->OwnGridCoordinate);
	const FIntPoint End = LockedMovePath.Num() > 0 ? LockedMovePath.Last() : OwnGridCoordinate;
	const FIntPoint Delta = Target->OwnGridCoordinate - End;
	if (FMath::Abs(Delta.X) + FMath::Abs(Delta.Y) <= FMath::Max(1, ActionRange)) LockedAttackTiles.Add(Target->OwnGridCoordinate);
}

void ABWCharacter::VisualizeEnemyIntent()
{
	if (!bIntentLocked) return;
	AGridManager* GridManager = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
	if (!GridManager) return;

	for (FIntPoint AT : LockedAttackTiles)
	{
		AGridTile* AttackTile = GridManager->GetTileAtCoordinate(AT);
		if (AttackTile) AttackTile->SetTileState(ETileState::EnemyAttackRange);
	}

	for (FIntPoint PathTile : LockedMovePath)
	{
		AGridTile* T = GridManager->GetTileAtCoordinate(PathTile);
		if (T) T->SetTileState(ETileState::EnemyMovePath); // [수정] 내 이동 범위(파랑)와 구분
	}
}

void ABWCharacter::ReceiveDamage(int32 DamageAmount)
{
	TakeHit(DamageAmount, false);
}

FBWDefenseResult ABWCharacter::TakeHit(int32 Damage, bool bIgnoreBlock)
{
	return ApplyPlannedHit(PlanHit(Damage, bIgnoreBlock));
}

FBWPlannedHit ABWCharacter::PlanHit(int32 Damage, bool bIgnoreBlock, int32 ShieldMP)
{
	FBWPlannedHit Plan;
	Plan.RawDamage = FMath::Max(0, Damage);
	// 삶의 방식: 질풍노도(받는 피해 절반), 격노(-90%)
	if (!bIsEnemy && Life.bGaleGuard) Plan.RawDamage = FMath::FloorToInt(Plan.RawDamage * UBWLifestyleRules::GaleGuardMultiplier);
	if (!bIsEnemy && Life.bRageGuard) Plan.RawDamage = FMath::FloorToInt(Plan.RawDamage * UBWLifestyleRules::RageGuardMultiplier);
	if (!bIsEnemy) Plan.RawDamage = FMath::Max(0, Plan.RawDamage - AugmentTotals.GetInt(EBWAugStat::FlatTaken));
	Plan.bIgnoreBlock = bIgnoreBlock;
	Plan.Defense.DamageAfterDefense = Plan.RawDamage;

	// 4단계: 방어 / 회피 / 매직실드 (플레이어만). MP는 복사본으로 계산하고 실제 소모는 반영할 때
	if (!bIsEnemy)
	{
		AGridManager* GridManager = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
		FRandomStream FallbackStream(FMath::Rand());
		FRandomStream& Stream = GridManager ? GridManager->GetCombatRandom() : FallbackStream;
		int32 MPCopy = CurrentMP;
		// 이번 적 턴 동안의 방어 +, 회피 + (엄폐·불굴 등)
		FBWDefenseStats TurnDefense = Defense;
		TurnDefense.FlatReduction += Life.DefenseBonus;
		TurnDefense.EvadeChance += Life.EvadeBonus;
		Plan.Defense = UBWDuelRules::ApplyDefense(Plan.RawDamage, TurnDefense, MPCopy, Stream, ShieldMP);
	}

	// 사격 방벽 보호막이 방어도보다 먼저 막는다
	if (!bIgnoreBlock && CoverShield > 0)
	{
		Plan.CoverAbsorbed = FMath::Min(CoverShield, Plan.Defense.DamageAfterDefense);
		Plan.Defense.DamageAfterDefense -= Plan.CoverAbsorbed;
	}

	// 5단계 미리 보기: 방어도 → HP
	int32 BlockCopy = Block;
	int32 HPCopy = FMath::Max(0, CurrentHP);
	UBWDuelRules::ApplyDamageToBlockAndHP(Plan.Defense.DamageAfterDefense, bIgnoreBlock, BlockCopy, HPCopy);
	Plan.BlockAbsorbed = Block - BlockCopy;
	Plan.HPLoss = FMath::Max(0, CurrentHP) - HPCopy;
	return Plan;
}

FBWDefenseResult ABWCharacter::ApplyPlannedHit(const FBWPlannedHit& Plan)
{
	const FBWDefenseResult Result = Plan.Defense;
	if (CurrentHP <= 0) return Result;

	CurrentMP = FMath::Max(0, CurrentMP - Result.MPSpent);
	CoverShield = FMath::Max(0, CoverShield - Plan.CoverAbsorbed);
	if (!bIsEnemy && Life.bRageGuard && Plan.RawDamage > 0) Life.bRageGuardHit = true;

	// 5단계: 방어도 → HP
	const int32 HPBefore = CurrentHP;
	const int32 BlockBefore = Block;
	UBWDuelRules::ApplyDamageToBlockAndHP(Result.DamageAfterDefense, Plan.bIgnoreBlock, Block, CurrentHP);
	if (!bIsEnemy && AugmentTotals.Has(EBWAugStat::BlockNoLoss)) Block = BlockBefore; // 세트 우르스카 4개
	// [추가] 증강: 처음 HP 30% 아래로 떨어지면 회복
	if (!bIsEnemy && AugmentTotals.Has(EBWAugStat::Last30Heal) && !AugTrack.bLast30Used && CurrentHP > 0 && CurrentHP * 10 < MaxHP * 3)
	{
		AugTrack.bLast30Used = true;
		CurrentHP = FMath::Min(MaxHP, CurrentHP + AugmentTotals.GetInt(EBWAugStat::Last30Heal));
		ShowPopup(TEXT("마지막 숨"), FLinearColor(0.35f, 1.0f, 0.45f));
	}
	const int32 HPLost = HPBefore - CurrentHP;

	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (GI && !bIsEnemy)
	{
		GI->PlayerCurrentHP = FMath::Max(0, GI->PlayerCurrentHP - HPLost);
		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Player hit %d -> after defense %d (evaded %d, MP spent %d), block %d, HP lost %d. GI HP: %d"),
			Plan.RawDamage, Result.DamageAfterDefense, Result.bEvaded ? 1 : 0, Result.MPSpent, Block, HPLost, GI->PlayerCurrentHP);
	}

	// [추가] 피해 숫자 연출 (회피, 매직실드·방어도로 막은 양도 함께)
	FString Popup = Result.bEvaded ? TEXT("회피!") : FString::Printf(TEXT("-%d"), HPLost);
	if (!Result.bEvaded && Result.DamageAfterDefense > HPLost) Popup += FString::Printf(TEXT("  (방어도 %d)"), Result.DamageAfterDefense - HPLost);
	if (Result.MPSpent > 0) Popup += FString::Printf(TEXT("  (실드 %d · MP -%d)"), Result.ShieldBlocked, Result.MPSpent);
	if (Plan.CoverAbsorbed > 0) Popup += FString::Printf(TEXT("  (방벽 %d)"), Plan.CoverAbsorbed);
	ShowPopup(Popup, bIsEnemy ? FLinearColor(1.0f, 0.95f, 0.6f) : FLinearColor(1.0f, 0.25f, 0.2f));

	if (CurrentHP <= 0) Die();
	else
	{
		if (Result.DamageAfterDefense > 0) BreakChant();
		CheckEnemyPhase();
	}
	return Result;
}

void ABWCharacter::BreakChant()
{
	if (!bChanting) return;
	bChanting = false;
	AdvancePattern(); // 영창으로 준비하던 다음 카드가 사라진다
	ShowPopup(TEXT("영창이 깨졌다"), FLinearColor(0.6f, 0.6f, 0.9f));
}

void ABWCharacter::Revive(FIntPoint Coord)
{
	bRevivePending = false;
	OwnGridCoordinate = Coord;
	CurrentHP = FMath::Max(1, FMath::FloorToInt(MaxHP * ReviveFraction));
	SetActorHiddenInGame(false);
	ShowPopup(TEXT("다시 일어났다"), FLinearColor(1.0f, 0.35f, 0.3f), 1.2f);
}

void ABWCharacter::EndEnemyTurnRules()
{
	if (!bIsEnemy || CurrentHP <= 0) return;
	if (TurnSelfBuff > 0) CurrentAttackBuff = FMath::Min(MaxAttackBuff, CurrentAttackBuff + TurnSelfBuff);
	if (EnrageTurn > 0 && ++EnemyTurnsTaken >= EnrageTurn) CurrentAttackBuff += EnrageBuff;
	if (TurnSelfDamage > 0) TakeFixedDamage(TurnSelfDamage, TEXT("수명"));
	if (bLastWish && CurrentHP > 0 && --LastWishLeft <= 0) TakeFixedDamage(CurrentHP + Block, TEXT("대가"));
}

void ABWCharacter::CheckEnemyPhase()
{
	if (bIsEnemy && !bLastWish && LastWishFraction > 0.0f && CurrentHP <= FMath::FloorToInt(MaxHP * LastWishFraction))
	{
		bLastWish = true;
		LastWishLeft = LastWishTurns;
		ShowPopup(TEXT("마지막 기원"), FLinearColor(0.8f, 0.2f, 0.3f), 1.3f);
	}
	if (bIsEnemy && !bPhase3Done && Phase3HPFraction > 0.0f && CurrentHP <= FMath::FloorToInt(MaxHP * Phase3HPFraction))
	{
		bPhase3Done = true;
		bPhase2Done = true;
		if (Phase3Patterns.Num() > 0)
		{
			CombatPatterns = Phase3Patterns;
			CurrentPatternIndex = 0;
		}
		Block += Phase3Block;
		ShowPopup(TEXT("장부를 펼친다"), FLinearColor(1.0f, 0.35f, 0.3f), 1.3f);
		return;
	}
	if (!bIsEnemy || bPhase2Done || Phase2HPFraction <= 0.0f || CurrentHP > FMath::FloorToInt(MaxHP * Phase2HPFraction)) return;
	bPhase2Done = true;
	if (Phase2Patterns.Num() > 0)
	{
		CombatPatterns = Phase2Patterns;
		CurrentPatternIndex = 0;
	}
	Block += Phase2Block;
	ShowPopup(TEXT("형세가 바뀌었다"), FLinearColor(1.0f, 0.35f, 0.3f), 1.3f);
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s enters phase 2"), *EnemyDisplayName);
}

void ABWCharacter::ShowPopup(const FString& Text, const FLinearColor& Color, float Scale)
{
	if (UBWAutoPlayer::IsActive()) return;
	const float Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0f;
	PopupStack = Now - LastPopupTime < 0.35f ? PopupStack + 1 : 0;
	LastPopupTime = Now;
	ABWFloatingText::Spawn(GetWorld(), GetActorLocation() + FVector(0, 0, StatusWidgetHeight + 40.0f + 45.0f * PopupStack), Text, Color, Scale);
}

void ABWCharacter::Die()
{
	if (bIsDying) return;
	// [추가] 미회수: 쓰러졌다가 다음 적 턴에 일어난다
	if (bIsEnemy && ReviveFraction > 0.0f && !bRevived)
	{
		bRevived = true;
		bRevivePending = true;
		bChanting = false;
		Statuses.Empty();
		Block = 0;
		SetActorHiddenInGame(true);
		ShowPopup(TEXT("쓰러졌다..."), FLinearColor(0.7f, 0.7f, 0.75f));
		if (AGridManager* Grid = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()))) Grid->CheckVictoryCondition();
		return;
	}
	// [추가] 훔친 골드를 돌려받는다
	if (bIsEnemy && StolenGold > 0)
	{
		if (UBWGameInstance* GoldGI = Cast<UBWGameInstance>(GetGameInstance())) GoldGI->CurrentGold += StolenGold;
		ShowPopup(FString::Printf(TEXT("골드 %d 회수"), StolenGold), FLinearColor(1.0f, 0.85f, 0.4f));
		StolenGold = 0;
	}
	bIsDying = true;
	SetHitFlash(false);

	// [수정] 바로 없애지 않는다: 그리드에서 먼저 빼서 판정·이동에는 없는 말로 만들고, 쓰러지는 연출 뒤에 지운다
	AGridManager* GridManager = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
	if (GridManager)
	{
		// [수정] 승패를 먼저 본다. 명단에서 먼저 빼면 플레이어가 없는 것으로 보여 패배가 나지 않았다
		GridManager->CheckVictoryCondition();
		GridManager->UnregisterCharacter(this);
	}
	SetActorEnableCollision(false);
	if (UBWAutoPlayer::IsActive()) return;
	if (UNiagaraSystem* Burst = DeathEffect.LoadSynchronous())
	{
		ABWPlayerController::PlayShortNiagara(GetWorld(), Burst, GetActorLocation() + FVector(0, 0, 50));
	}
	DyingAge = 0.0f;
	DyingStartScale = GetActorScale3D();
	DyingStartLocation = GetActorLocation();
}

void ABWCharacter::PlayHitReaction(const FVector& FromLocation, bool bHeavy)
{
	if (bIsDying || UBWAutoPlayer::IsActive()) return;
	FVector Dir = GetActorLocation() - FromLocation;
	Dir.Z = 0.0f;
	ReactDir = Dir.GetSafeNormal();
	ReactAge = 0.0f;
	bReactHeavy = bHeavy;
	bReactIsDodge = false;
	SetHitFlash(true);
}

void ABWCharacter::PlayDodge(const FVector& FromLocation)
{
	if (bIsDying) return;
	FVector Dir = GetActorLocation() - FromLocation;
	Dir.Z = 0.0f;
	// 공격이 오는 방향의 옆으로 비킨다
	ReactDir = FVector::CrossProduct(Dir.GetSafeNormal(), FVector::UpVector);
	ReactAge = 0.0f;
	bReactHeavy = false;
	bReactIsDodge = true;
}

void ABWCharacter::SetMeshOffset(const FVector& WorldOffset, float ScaleZ)
{
	if (MeshBaseTransforms.Num() == 0)
	{
		TArray<UMeshComponent*> Meshes;
		GetComponents<UMeshComponent>(Meshes);
		for (UMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetAttachParent() == GetRootComponent() || Mesh == GetRootComponent()) MeshBaseTransforms.Emplace(Mesh, Mesh->GetRelativeTransform());
		}
	}
	for (const TPair<TWeakObjectPtr<USceneComponent>, FTransform>& Entry : MeshBaseTransforms)
	{
		USceneComponent* Comp = Entry.Key.Get();
		if (!Comp || Comp == GetRootComponent()) continue;
		const FVector LocalOffset = GetActorTransform().InverseTransformVectorNoScale(WorldOffset);
		Comp->SetRelativeLocation(Entry.Value.GetLocation() + LocalOffset);
		FVector Scale = Entry.Value.GetScale3D();
		Scale.Z *= ScaleZ;
		Scale.X *= 1.0f + (1.0f - ScaleZ) * 0.5f;
		Scale.Y *= 1.0f + (1.0f - ScaleZ) * 0.5f;
		Comp->SetRelativeScale3D(Scale);
	}
}

void ABWCharacter::SetHitFlash(bool bOn)
{
	if (bFlashOn == bOn) return;
	bFlashOn = bOn;
	UMaterialInterface* Flash = bOn ? HitFlashMaterial.LoadSynchronous() : nullptr;
	if (bOn && !Flash) return;
	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* Mesh : Meshes) Mesh->SetOverlayMaterial(Flash);
}

void ABWCharacter::UpdateReactions(float DeltaTime)
{
	if (bIsDying)
	{
		// 쓰러짐: 납작하게 주저앉으며 작아지고 살짝 가라앉은 뒤 사라진다
		DyingAge += DeltaTime;
		const float Alpha = DeathTime > 0.0f ? FMath::Clamp(DyingAge / DeathTime, 0.0f, 1.0f) : 1.0f;
		const float Ease = Alpha * Alpha;
		SetActorScale3D(DyingStartScale * FMath::Lerp(1.0f, 0.05f, Ease));
		SetActorLocation(DyingStartLocation - FVector(0, 0, 30.0f * Ease));
		if (Alpha >= 1.0f) Destroy();
		return;
	}
	if (ReactAge < 0.0f) return;

	ReactAge += DeltaTime;
	if (bFlashOn && ReactAge >= HitFlashTime) SetHitFlash(false);

	const float Duration = bReactIsDodge ? HitReactTime * 1.3f : HitReactTime;
	const float Alpha = FMath::Clamp(ReactAge / Duration, 0.0f, 1.0f);
	if (Alpha >= 1.0f)
	{
		ReactAge = -1.0f;
		SetMeshOffset(FVector::ZeroVector, 1.0f);
		return;
	}
	// 빠르게 밀렸다가 천천히 돌아온다 (sin 곡선의 앞부분을 짧게)
	const float Curve = Alpha < 0.25f ? Alpha / 0.25f : 1.0f - (Alpha - 0.25f) / 0.75f;
	const float Distance = bReactIsDodge ? 45.0f : (bReactHeavy ? 28.0f : 16.0f);
	const float Squash = bReactIsDodge ? 1.0f : 1.0f - (bReactHeavy ? 0.18f : 0.1f) * Curve;
	SetMeshOffset(ReactDir * Distance * Curve, Squash);
}

FBWSkillStats ABWCharacter::GetSkillStats() const
{
	FBWSkillStats Stats;
	Stats.PHA = PHA;
	Stats.TEC = TEC;
	Stats.INT = INT;
	Stats.FTH = FTH;
	Stats.MaxHP = MaxHP;
	return Stats;
}

FBWCardData ABWCharacter::GetBasicAttackCard() const
{
	const UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	return GI ? GI->GetClassBasicStrike(CharacterClass) : UBWDuelRules::MakeBasicStrike(CharacterClass);
}

float ABWCharacter::GetCardPower() const
{
	const UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	return GI ? GI->GetClassCardPower(CharacterClass) : UBWDuelRules::MakeClassCardPower(CharacterClass);
}

FBWDiceRange ABWCharacter::GetBasicAttackRange() const
{
	return UBWDuelRules::ComputeCardSkillRange(GetBasicAttackCard(), GetSkillStats());
}

FBWDiceRange ABWCharacter::GetCardBoostRange(const FBWCardData& Card) const
{
	return UBWDuelRules::ScaleCardBoost(UBWDuelRules::ComputeCardSkillRange(Card, GetSkillStats()), GetCardPower());
}

FBWDuelSide ABWCharacter::MakeAttackSide(const FBWCardData* BoostCard, const ABWCharacter* Target) const
{
	// 숫자 범위 = 기본 공격 (+ 공격카드 몫), 그 위에 삶의 방식 보너스. 상태이상·밀기·증강·상태 보정은 카드 쪽 계산을 그대로 쓴다
	FBWDiceRange Basic = GetBasicAttackRange();
	FBWDiceRange Boost(0, 0);
	const bool bSpell = BoostCard && BoostCard->MPCost > 0;
	if (BoostCard)
	{
		Boost = GetCardBoostRange(*BoostCard);
		// 매직 캐스터 해방: 마법 카드 +40%
		if (bSpell && Life.bLiberated)
		{
			Boost = FBWDiceRange(FMath::FloorToInt(Boost.Min * UBWLifestyleRules::LiberatedSpellMultiplier), FMath::FloorToInt(Boost.Max * UBWLifestyleRules::LiberatedSpellMultiplier));
		}
		if (BoostCard->Special == EBWCardSpecial::Storm && Life.bStormReady) { Boost.Min *= 2; Boost.Max *= 2; }
	}
	// 철혈의 분쇄자 폭풍 질주: 이번 턴 첫 합의 기본 공격 두 배
	if (Life.bStormReady) { Basic.Min *= 2; Basic.Max *= 2; }
	FBWDiceRange Range(Basic.Min + Boost.Min, Basic.Max + Boost.Max);

	if (BoostCard)
	{
		FBWConditionContext Context;
		Context.bRaging = IsRaging();
		Context.Wounds = Life.Wounds;
		Context.LostHP = FMath::Max(0, MaxHP - CurrentHP);
		Context.bTargetLow = Target && Target->MaxHP > 0 && Target->CurrentHP * 10 <= Target->MaxHP * 3;
		Context.bMoved = Life.bMovedThisTurn;
		Context.Dark = Life.Dark;
		Context.Block = Block;
		Context.bLiberated = Life.bLiberated;
		Context.bTenebris = Life.bTenebris;
		Range = UBWLifestyleRules::ApplyCondition(Range, BoostCard->Condition, BoostCard->ConditionValue, Context);

		if (BoostCard->Special == EBWCardSpecial::WillOfIron)
		{
			const FBWDiceRange Will = UBWLifestyleRules::WillBonus(Life.Wounds);
			Range.Min += Will.Min;
			Range.Max += Will.Max;
		}
		if (BoostCard->Special == EBWCardSpecial::MagicBullet)
		{
			Range.Max += UBWLifestyleRules::MagicMaxBonus(Life.MagicCount, BoostCard->Condition == EBWCardCondition::MagicDouble);
		}
		if (bSpell && Life.ChantReady > 0) Range.Max += Life.ChantReady;
		if (BoostCard->Special == EBWCardSpecial::Explosion)
		{
			const int32 Extra = ExplosionExtraMP >= 0 ? ExplosionExtraMP : UBWLifestyleRules::ExplosionExtraMP(CurrentMP - BoostCard->MPCost);
			Range = UBWLifestyleRules::ApplyExplosion(Range, Extra);
		}
	}
	// 광전사 광기 (피의 복수면 3배), 어둠의 성자 테네브리스 (어둠 1당 최댓값 +2)
	if (IsRaging())
	{
		const FBWDiceRange Rage = UBWLifestyleRules::RageBonus(PHA, Life.bBloodRevenge);
		Range.Min += Rage.Min;
		Range.Max += Rage.Max;
	}
	if (Life.bTenebris) Range.Max += Life.Dark * UBWLifestyleRules::TenebrisMaxPerDark;
	ApplyAugmentsToRange(Range, BoostCard, Target);
	Range.Max = FMath::Max(0, Range.Max);
	Range.Min = FMath::Clamp(Range.Min, 0, Range.Max);

	FBWCardData Merged = BoostCard ? *BoostCard : GetBasicAttackCard();
	Merged.CardKind = ECardKind::Attack;
	Merged.SkillMin = Range.Min;
	Merged.SkillMax = Range.Max;
	// 계수는 이미 Range에 들어 있으므로 0으로 두고 범위를 그대로 쓰게 한다
	Merged.ATKScale = Merged.PHAScale = Merged.TECScale = Merged.INTScale = Merged.FTHScale = Merged.HPScale = 0.0f;
	FBWDuelSide Side = MakePlayerDuelSide(Merged);

	Side.CritChance = FMath::Min(1.0f, Side.CritChance + ClassProfile.CritChanceBonus + Life.TurnCritBonus + (BoostCard ? BoostCard->CritChanceBonus : 0.0f));
	if (BoostCard) Side.CritMultiplier += BoostCard->CritMultiplierBonus;
	// 마탄: 3번째부터 피해 +10%, 일곱 번째는 반드시 치명타에 세 배
	if (BoostCard && BoostCard->Special == EBWCardSpecial::MagicBullet)
	{
		const int32 Shot = Life.MagicCount + 1;
		Side.DamageMultiplier *= UBWLifestyleRules::MagicDamageMultiplier(Shot);
		if (Shot >= UBWLifestyleRules::SeventhShot) Side.CritChance = 1.0f;
	}

	// [추가] 증강: 피해 배율·치명타·굴림 규칙
	const FBWAugmentTotals& T = AugmentTotals;
	using S = EBWAugStat;
	const bool bRangedCard = BoostCard && BoostCard->Range >= 2;
	float DamagePct = T.Get(S::DamagePct) + (MaxHP > 0 ? FMath::FloorToInt(10.0f * (MaxHP - CurrentHP) / MaxHP) * T.Get(S::WinPerLostHP) : 0.0f);
	if (bRangedCard) DamagePct += T.Get(S::RangedDamagePct);
	if (CombatTurnNumber <= 4) DamagePct += T.Get(S::EarlyDamagePct);
	int32 EnemiesAlive = 0;
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), Actors);
	for (AActor* Actor : Actors)
	{
		const ABWCharacter* Other = Cast<ABWCharacter>(Actor);
		EnemiesAlive += Other && Other->bIsEnemy && Other->CurrentHP > 0 ? 1 : 0;
	}
	if (EnemiesAlive == 1) DamagePct += T.Get(S::LastEnemyPct);
	Side.DamageMultiplier *= (1.0f + DamagePct) * T.Get(S::DamageMult);
	Side.IncomingDamageMultiplier *= 1.0f + T.Get(S::TakenPct) + T.Get(S::LoseTakenPct) + (EnemiesAlive >= 3 ? T.Get(S::CrowdTakenPct) : 0.0f);
	Side.CritMultiplier += T.Get(S::CritMul);
	if (bRangedCard && BoostCard->Range >= 3) Side.CritChance += T.Get(S::RangedCrit);
	if (T.Has(S::FirstCritX2) && !AugTrack.bFirstCritDone) Side.CritMultiplier *= 2.0f;
	if (T.Has(S::AlwaysCrit) || (T.Has(S::FirstAttackCrit) && !AugTrack.bFirstAttackCritDone)) Side.CritChance = 1.0f;
	Side.CritChance = FMath::Min(1.0f, Side.CritChance);
	Side.bWinsTies = T.Has(S::TieWin);
	Side.bMaxRoll = T.Has(S::MaxRoll);
	Side.bNeverLose = bRangedCard && T.Has(S::RangedNeverLose);
	Side.bIgnoreGuard = BoostCard && BoostCard->bIgnoreTargetBlock;
	if (T.Has(S::TurnFirstFocus) && !AugTrack.bTurnFirstDuelDone) Side.bFocused = true;
	return Side;
}

FBWDuelSide ABWCharacter::MakePlayerDuelSide(const FBWCardData& Card) const
{
	FBWDuelSide Side;
	Side.SkillRange = UBWDuelRules::ComputeCardSkillRange(Card, GetSkillStats());
	UBWDuelRules::ComputeCritFromTEC(TEC, Side.CritChance, Side.CritMultiplier);
	Side.AppliedStatus = Card.AppliedStatus;
	Side.StatusDuration = Card.StatusDuration;
	Side.StatusValue = Card.StatusValue;
	Side.bPushesTarget = Card.bPushesTarget;

	// 증강: 급소(치명타 +), 중량타(공격카드 최댓값 +30%), 재굴림
	Side.CritChance = FMath::Min(1.0f, Side.CritChance + AugmentTotals.CritChanceBonus);
	if (Card.CardKind == ECardKind::Attack)
	{
		Side.SkillRange = UBWDuelRules::ScaleRangeMax(Side.SkillRange, AugmentTotals.AttackMaxMultiplier);
		Side.SkillRange.Max += TurnAttackMaxBonus; // 효과카드 '기세'
	}
	Side.LosingRerolls = DuelRerollsLeft > 0 ? 1 : 0;

	ApplyTerrainToDuelSide(Side);
	ApplyStatusesToDuelSide(Side);
	return Side;
}

FBWDuelSide ABWCharacter::MakeEnemyDuelSide(bool bCounter) const
{
	FBWDuelSide Side;
	Side.SkillRange = FBWDiceRange(1, 4);
	if (CombatPatterns.Num() > 0)
	{
		const int32 PatternIdx = CombatPatterns.IsValidIndex(CurrentPatternIndex) ? CurrentPatternIndex : 0;
		const TArray<FBWEnemyPattern>& Actions = CombatPatterns[PatternIdx].Actions;
		if (Actions.Num() > 0)
		{
			const FBWEnemyPattern& Action = Actions[0];
			const bool bHasCounter = bCounter && Action.CounterRange.Max > 0;
			Side.SkillRange = bHasCounter ? Action.CounterRange : Action.SkillRange;
			// '강화 후 공격'으로 쌓인 값만큼 숫자 범위가 오른다 (방어 카드 수치는 오르지 않는다)
			Side.bGuard = bHasCounter && Action.CounterKind == EBWEnemyCounterKind::Guard;
			if (!Side.bGuard)
			{
				Side.SkillRange.Min += CurrentAttackBuff;
				Side.SkillRange.Max += CurrentAttackBuff;
			}
			Side.AppliedStatus = bHasCounter ? Action.CounterStatus : Action.AppliedStatus;
			Side.StatusDuration = bHasCounter ? Action.CounterStatusDuration : Action.StatusDuration;
			Side.StatusValue = bHasCounter ? Action.CounterStatusValue : Action.StatusValue;
			Side.bPushesTarget = !bCounter && Action.bPushesTarget;
			Side.GuardRiposte = bHasCounter ? Action.CounterRiposte : 0;
		}
	}
	ApplyTerrainToDuelSide(Side);
	ApplyStatusesToDuelSide(Side);
	if (bLastWish) Side.IncomingDamageMultiplier *= 0.5f;

	// [추가] 증강의 대가·세트: 적 범위 + / 적 전체 혼란
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (const ABWCharacter* Player = PC ? Cast<ABWCharacter>(PC->GetPawn()) : nullptr)
	{
		const FBWAugmentTotals& T = Player->AugmentTotals;
		Side.SkillRange.Max += T.GetInt(EBWAugStat::EnemyMax);
		Side.SkillRange.Max = FMath::FloorToInt(Side.SkillRange.Max * (1.0f + T.Get(EBWAugStat::EnemyMaxPct)));
		if (T.Has(EBWAugStat::EnemiesConfused)) Side.bConfused = true;
	}
	return Side;
}

void ABWCharacter::ApplyStatusesToDuelSide(FBWDuelSide& Side) const
{
	if (HasStatus(EStatusEffect::Weak)) Side.SkillRange = UBWStatusRules::ApplyWeak(Side.SkillRange);
	if (HasStatus(EStatusEffect::Vulnerable)) Side.IncomingDamageMultiplier *= !bIsEnemy && AugmentTotals.Has(EBWAugStat::VulnSoft) ? 1.1f : UBWStatusRules::VulnerableMultiplier;
	Side.bConfused = HasStatus(EStatusEffect::Confusion);
	Side.bFocused = HasStatus(EStatusEffect::Focus);
}

int32 ABWCharacter::CalculateFinalCardDamage(const FBWCardData& CardInfo, bool& OutIsCritical)
{
	float FinalDamage = CardInfo.BaseDamage;
	
	// [수정] 멀티 스탯 스케일링 합산 (유저 공식 반영)
	FinalDamage += (float)PHA * CardInfo.PHAScale;
	FinalDamage += (float)INT * CardInfo.INTScale;
	FinalDamage += (float)FTH * CardInfo.FTHScale;
	FinalDamage += (float)TEC * CardInfo.TECScale;
	FinalDamage += (float)MaxHP * CardInfo.HPScale;
	
	// ATK는 PHA + TEC로 자동 계산되어 반영됩니다.
	FinalDamage += (float)GetEffectiveATK() * CardInfo.ATKScale;

	float CritChance = FMath::Clamp((float)TEC / 2.0f, 0.0f, 100.0f);
	float CritMultiplier = 1.5f + ((float)TEC / 66.0f);
	float RandomRoll = FMath::FRandRange(0.0f, 100.0f);
	
	if (RandomRoll <= CritChance)
	{
		OutIsCritical = true;
		FinalDamage *= CritMultiplier;
	}
	else
	{
		OutIsCritical = false;
	}

	return FMath::Max(0, FMath::RoundToInt(FinalDamage));
}
int32 ABWCharacter::GetAttackReach(const FBWCardData* BoostCard) const
{
	int32 Reach = FMath::Max(1, GetBasicAttackCard().Range);
	if (BoostCard) Reach = FMath::Max(Reach, BoostCard->Range + (BoostCard->Range >= 2 ? AugmentTotals.GetInt(EBWAugStat::RangedReachUp) : 0));
	return Reach + AugmentTotals.GetInt(EBWAugStat::ReachUp);
}

int32 ABWCharacter::GetMaxGuardMP() const
{
	if (Defense.Type != EDefenseType::MagicShield) return 0;
	return FMath::Clamp(CurrentMP, 0, FMath::Max(0, Defense.MagicShieldMaxMP));
}
