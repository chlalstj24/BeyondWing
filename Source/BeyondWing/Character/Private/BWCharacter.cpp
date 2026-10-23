#include "BeyondWing/Character/Public/BWCharacter.h"
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
	DeathEffect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Hit2.NS_Free_Magic_Hit2")));

	DeckComponent = CreateDefaultSubobject<UDeckComponent>(TEXT("DeckComponent"));
}

void ABWCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 플레이어는 직업별 수치를 쓴다 (적은 GridManager::SpawnEnemies에서 데이터 값으로 덮어씀)
	if (!bIsEnemy) ApplyClassProfile();

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
	}
}

void ABWCharacter::BeginTurnResources()
{
	++CombatTurnNumber;
	TurnAttackMaxBonus = 0;
	UBWDuelRules::ApplyTurnStartResources(ClassProfile, CombatTurnNumber, CurrentMP);
}

int32 ABWCharacter::Heal(int32 Amount)
{
	if (CurrentHP <= 0 || Amount <= 0) return 0;
	const int32 Before = CurrentHP;
	CurrentHP = FMath::Min(MaxHP, CurrentHP + Amount);
	const int32 Healed = CurrentHP - Before;

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
}

void ABWCharacter::ApplyStatus(EStatusEffect Type, int32 Duration, int32 Value)
{
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
	const int32 Bleed = UBWStatusRules::TickEndOfOwnTurn(Statuses);
	if (Bleed > 0 && CurrentHP > 0)
	{
		// 출혈은 고정 피해: 방어·회피·매직실드 없이 방어도 → HP
		const int32 HPBefore = CurrentHP;
		UBWDuelRules::ApplyDamageToBlockAndHP(Bleed, false, Block, CurrentHP);
		UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
		if (GI && !bIsEnemy) GI->PlayerCurrentHP = FMath::Max(0, GI->PlayerCurrentHP - (HPBefore - CurrentHP));
		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s bleeds %d (HP %d -> %d)"), bIsEnemy ? *EnemyDisplayName : TEXT("Player"), Bleed, HPBefore, CurrentHP);
		ShowPopup(FString::Printf(TEXT("출혈 -%d"), HPBefore - CurrentHP), FLinearColor(0.8f, 0.1f, 0.1f));
		if (CurrentHP <= 0) Die();
	}
	return Bleed;
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
	RolledSpeed = FMath::RandRange(Dice.Min, FMath::Max(Dice.Min, Dice.Max));
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
	if (!Action || Action->ActionType == EBWEnemyActionType::Heal || IsStunned()) return; // 회복은 이동하지 않고, 기절하면 아무것도 안 함

	AGridManager* GridManager = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
	ABWCharacter* Target = FindNearestOpponent();
	if (!GridManager || !Target) return;

	LockedMovePath = GridManager->FindRushPath(OwnGridCoordinate, Target->OwnGridCoordinate, RolledSpeed, true);
	const FIntPoint End = LockedMovePath.Num() > 0 ? LockedMovePath.Last() : OwnGridCoordinate;
	const FIntPoint Delta = Target->OwnGridCoordinate - End;
	if (FMath::Abs(Delta.X) + FMath::Abs(Delta.Y) == 1) LockedAttackTiles.Add(Target->OwnGridCoordinate);
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
	Plan.bIgnoreBlock = bIgnoreBlock;
	Plan.Defense.DamageAfterDefense = Plan.RawDamage;

	// 4단계: 방어 / 회피 / 매직실드 (플레이어만). MP는 복사본으로 계산하고 실제 소모는 반영할 때
	if (!bIsEnemy)
	{
		AGridManager* GridManager = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
		FRandomStream FallbackStream(FMath::Rand());
		FRandomStream& Stream = GridManager ? GridManager->GetCombatRandom() : FallbackStream;
		int32 MPCopy = CurrentMP;
		Plan.Defense = UBWDuelRules::ApplyDefense(Plan.RawDamage, Defense, MPCopy, Stream, ShieldMP);
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

	// 5단계: 방어도 → HP
	const int32 HPBefore = CurrentHP;
	UBWDuelRules::ApplyDamageToBlockAndHP(Result.DamageAfterDefense, Plan.bIgnoreBlock, Block, CurrentHP);
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
	ShowPopup(Popup, bIsEnemy ? FLinearColor(1.0f, 0.95f, 0.6f) : FLinearColor(1.0f, 0.25f, 0.2f));

	if (CurrentHP <= 0) Die();
	return Result;
}

void ABWCharacter::ShowPopup(const FString& Text, const FLinearColor& Color, float Scale)
{
	const float Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0f;
	PopupStack = Now - LastPopupTime < 0.35f ? PopupStack + 1 : 0;
	LastPopupTime = Now;
	ABWFloatingText::Spawn(GetWorld(), GetActorLocation() + FVector(0, 0, StatusWidgetHeight + 40.0f + 45.0f * PopupStack), Text, Color, Scale);
}

void ABWCharacter::Die()
{
	if (bIsDying) return;
	bIsDying = true;
	SetHitFlash(false);

	// [수정] 바로 없애지 않는다: 그리드에서 먼저 빼서 판정·이동에는 없는 말로 만들고, 쓰러지는 연출 뒤에 지운다
	AGridManager* GridManager = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
	if (GridManager)
	{
		GridManager->UnregisterCharacter(this);
		GridManager->CheckVictoryCondition();
	}
	SetActorEnableCollision(false);
	if (UNiagaraSystem* Burst = DeathEffect.LoadSynchronous())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Burst, GetActorLocation() + FVector(0, 0, 50));
	}
	DyingAge = 0.0f;
	DyingStartScale = GetActorScale3D();
	DyingStartLocation = GetActorLocation();
}

void ABWCharacter::PlayHitReaction(const FVector& FromLocation, bool bHeavy)
{
	if (bIsDying) return;
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

FBWDuelSide ABWCharacter::MakeAttackSide(const FBWCardData* BoostCard) const
{
	if (!BoostCard) return MakePlayerDuelSide(GetBasicAttackCard());

	// 카드로 강화: 숫자 범위만 기본 공격 + 카드 몫으로 바꾸고, 상태이상·밀기·증강·상태 보정은 카드 쪽 계산을 그대로 쓴다
	const FBWDiceRange Combined = UBWDuelRules::AddCardBoost(GetBasicAttackRange(), UBWDuelRules::ComputeCardSkillRange(*BoostCard, GetSkillStats()), GetCardPower());
	FBWCardData Merged = *BoostCard;
	Merged.SkillMin = Combined.Min;
	Merged.SkillMax = Combined.Max;
	// 계수는 이미 Combined에 들어 있으므로 0으로 두고 범위를 그대로 쓰게 한다
	Merged.ATKScale = Merged.PHAScale = Merged.TECScale = Merged.INTScale = Merged.FTHScale = Merged.HPScale = 0.0f;
	return MakePlayerDuelSide(Merged);
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

	ApplyStatusesToDuelSide(Side);
	return Side;
}

FBWDuelSide ABWCharacter::MakeEnemyDuelSide() const
{
	FBWDuelSide Side;
	Side.SkillRange = FBWDiceRange(1, 4);
	if (CombatPatterns.Num() > 0)
	{
		const int32 PatternIdx = CombatPatterns.IsValidIndex(CurrentPatternIndex) ? CurrentPatternIndex : 0;
		const TArray<FBWEnemyPattern>& Actions = CombatPatterns[PatternIdx].Actions;
		if (Actions.Num() > 0)
		{
			Side.SkillRange = Actions[0].SkillRange;
			// '강화 후 공격'으로 쌓인 값만큼 숫자 범위가 오른다
			Side.SkillRange.Min += CurrentAttackBuff;
			Side.SkillRange.Max += CurrentAttackBuff;
			Side.AppliedStatus = Actions[0].AppliedStatus;
			Side.StatusDuration = Actions[0].StatusDuration;
			Side.StatusValue = Actions[0].StatusValue;
			Side.bPushesTarget = Actions[0].bPushesTarget;
		}
	}
	ApplyStatusesToDuelSide(Side);
	return Side;
}

void ABWCharacter::ApplyStatusesToDuelSide(FBWDuelSide& Side) const
{
	if (HasStatus(EStatusEffect::Weak)) Side.SkillRange = UBWStatusRules::ApplyWeak(Side.SkillRange);
	if (HasStatus(EStatusEffect::Vulnerable)) Side.IncomingDamageMultiplier *= UBWStatusRules::VulnerableMultiplier;
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
	if (BoostCard) Reach = FMath::Max(Reach, BoostCard->Range);
	return Reach;
}

int32 ABWCharacter::GetMaxGuardMP() const
{
	if (Defense.Type != EDefenseType::MagicShield) return 0;
	return FMath::Clamp(CurrentMP, 0, FMath::Max(0, Defense.MagicShieldMaxMP));
}
