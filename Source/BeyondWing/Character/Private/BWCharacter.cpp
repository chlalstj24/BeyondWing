#include "BeyondWing/Character/Public/BWCharacter.h"
#include "BeyondWing/Combat/Public/DeckComponent.h"
#include "Kismet/GameplayStatics.h"
#include "BeyondWing/Grid/Public/GridManager.h"
#include "BeyondWing/Grid/Public/GridTile.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

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
	
	BaseMovePoints = 1;
	CurrentMovePoints = BaseMovePoints;

	BaseAttackPower = 10;
	CurrentAttackBuff = 0;
	CurrentPatternIndex = 0;

	SmoothMoveSpeed = 15.0f; // 기본 이동 속도

	DeckComponent = CreateDefaultSubobject<UDeckComponent>(TEXT("DeckComponent"));
}

void ABWCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentMovePoints = BaseMovePoints;
}

void ABWCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 부드러운 이동 연출 (Queue 방식)
	if (VisualMoveQueue.Num() > 0)
	{
		FVector CurrentLoc = GetActorLocation();
		FVector TargetLoc = VisualMoveQueue[0];

		FVector NewLoc = FMath::VInterpTo(CurrentLoc, TargetLoc, DeltaTime, SmoothMoveSpeed);
		SetActorLocation(NewLoc);

		if (FVector::Dist(NewLoc, TargetLoc) < 5.0f)
		{
			SetActorLocation(TargetLoc); // 위치 보정
			VisualMoveQueue.RemoveAt(0);
		}
	}
}

void ABWCharacter::AddMovementStep(FVector TargetWorldLoc)
{
	VisualMoveQueue.Add(TargetWorldLoc);
}

void ABWCharacter::ExecuteNextPattern()
{
	if (CombatPatterns.Num() == 0 || !bIsEnemy) return;
	if (CurrentPatternIndex >= CombatPatterns.Num()) CurrentPatternIndex = 0;

	FBWEnemyTurnPattern CurrentTurnPattern = CombatPatterns[CurrentPatternIndex];
	AGridManager* GridManager = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
	if (!GridManager) return;

	UE_LOG(LogTemp, Error, TEXT("[AI_TRACE] === EXECUTE START: %s (TurnIdx: %d) ==="), *GetName(), CurrentPatternIndex);

	// 1. [이동 실행 - 스르륵 한 칸씩]
	if (LockedMovePath.Num() > 0)
	{
		for (int32 i = 0; i < LockedMovePath.Num(); ++i)
		{
			FIntPoint StepCoord = LockedMovePath[i];
			ABWCharacter* BlockingChar = GridManager->GetCharacterAtCoordinate(StepCoord);
			
			if (BlockingChar == nullptr)
			{
				OwnGridCoordinate = StepCoord;
				AGridTile* NextTile = GridManager->GetTileAtCoordinate(StepCoord);
				if (NextTile)
				{
					// 즉시 이동 대신 큐에 추가하여 틱에서 이동하도록 함
					AddMovementStep(NextTile->GetActorLocation() + FVector(0, 0, 20));
				}
			}
			else if (!BlockingChar->bIsEnemy)
			{
				BlockingChar->ReceiveDamage(BaseAttackPower);
				break; 
			}
			else break; 
		}
	}

	// 2. 액션 실행 (적 이펙트는 타겟 위치에서 터지도록 GM에게 위임)
	for (int32 i = 0; i < CurrentTurnPattern.Actions.Num(); ++i)
	{
		const FBWEnemyPattern& Action = CurrentTurnPattern.Actions[i];
		
		// 적 액션 연출 (임시 스킬 데이터 생성하여 GM 호출)
		if (Action.ActionType == EBWEnemyActionType::Attack && LockedAttackTiles.Num() > 0)
		{
			FBWCardData TempSkill;
			TempSkill.SkillShape = Action.SkillShape;
			TempSkill.Range = Action.Range;
			TempSkill.SkillEffect = Action.ActionEffect;
			TempSkill.CardSound = Action.ActionSound;
			TempSkill.BaseDamage = (float)Action.Value;

			GridManager->ExecuteSkill(OwnGridCoordinate, LockedAttackTiles[0], TempSkill);
		}
		else if (Action.ActionType == EBWEnemyActionType::Heal)
		{
			CurrentHP = FMath::Min(MaxHP, (int32)(CurrentHP + Action.Value));
		}
		else if (Action.ActionType == EBWEnemyActionType::BuffAttack)
		{
			CurrentAttackBuff += Action.Value;
		}
	}

	CurrentPatternIndex++;
	LockedAttackTiles.Empty();
	LockedMovePath.Empty();
	bIntentLocked = false;
}

void ABWCharacter::UpdateEnemyIntent()
{
	if (CombatPatterns.Num() == 0 || !bIsEnemy) return;

	AGridManager* GridManager = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
	if (!GridManager) return;

	int32 PreviewIndex = CurrentPatternIndex;
	if (PreviewIndex >= CombatPatterns.Num()) PreviewIndex = 0;

	FBWEnemyTurnPattern IntentPattern = CombatPatterns[PreviewIndex];
	LockedAttackTiles.Empty();
	LockedMovePath.Empty();

	UE_LOG(LogTemp, Error, TEXT("[AI_INTENT] UPDATING Intent for %s (Index: %d)"), *GetName(), PreviewIndex);

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), FoundActors);
	ABWCharacter* TargetPlayer = nullptr;
	float MinDist = 9999.0f;

	for (AActor* Actor : FoundActors)
	{
		ABWCharacter* Char = Cast<ABWCharacter>(Actor);
		if (Char && !Char->bIsEnemy && Char->CurrentHP > 0)
		{
			float Dist = FMath::Abs(Char->OwnGridCoordinate.X - OwnGridCoordinate.X) + FMath::Abs(Char->OwnGridCoordinate.Y - OwnGridCoordinate.Y);
			if (Dist < MinDist) { MinDist = Dist; TargetPlayer = Char; }
		}
	}

	FIntPoint TargetCoord = TargetPlayer ? TargetPlayer->OwnGridCoordinate : FIntPoint(0, OwnGridCoordinate.Y);
	FIntPoint SimulatedPos = OwnGridCoordinate;

	for (int32 i = 0; i < IntentPattern.Actions.Num(); ++i)
	{
		const FBWEnemyPattern& Action = IntentPattern.Actions[i];
		
		// [검증 로그]
		UE_LOG(LogTemp, Error, TEXT("[AI_INTENT] Action %d Raw: ActionType=%d, Val=%d, Dist=%d"), 
			i, (int32)Action.ActionType, Action.Value, Action.MoveDistance);
		
		if (Action.MoveDistance > 0)
		{
			FIntPoint Dir = GridManager->CalculateDirection(SimulatedPos, TargetCoord);
			for (int32 step = 0; step < Action.MoveDistance; ++step)
			{
				FIntPoint NextCoord = SimulatedPos + Dir;
				if (NextCoord.X >= 0 && NextCoord.X < GridManager->Width && NextCoord.Y >= 0 && NextCoord.Y < GridManager->Height)
				{
					ABWCharacter* BlockingChar = GridManager->GetCharacterAtCoordinate(NextCoord);
					if (BlockingChar == nullptr)
					{
						LockedMovePath.AddUnique(NextCoord);
						SimulatedPos = NextCoord;
						UE_LOG(LogTemp, Error, TEXT("[AI_INTENT] -> Path Added: %s"), *NextCoord.ToString());
					}
					else if (!BlockingChar->bIsEnemy)
					{
						LockedMovePath.AddUnique(NextCoord);
						LockedAttackTiles.AddUnique(NextCoord);
						UE_LOG(LogTemp, Error, TEXT("[AI_INTENT] -> Collision Preview: %s"), *NextCoord.ToString());
						break;
					}
					else break;
				}
			}
		}

		if (Action.ActionType == EBWEnemyActionType::Attack) // Attack
		{
			TArray<FIntPoint> Tiles = GridManager->GetAffectedCoordinates(SimulatedPos, TargetCoord, Action.SkillShape, Action.Range);
			for (FIntPoint T : Tiles) LockedAttackTiles.AddUnique(T);
			UE_LOG(LogTemp, Error, TEXT("[AI_INTENT] -> Attack Tiles added: %d"), Tiles.Num());
		}
	}
	bIntentLocked = true;
	UE_LOG(LogTemp, Error, TEXT("[AI_INTENT] INTENT FINALIZED. MovePath size: %d"), LockedMovePath.Num());
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
		if (T) T->SetTileState(ETileState::MoveRange);
	}
}

void ABWCharacter::ReceiveDamage(int32 DamageAmount)
{
	if (CurrentHP <= 0) return;
	CurrentHP = FMath::Max(0, CurrentHP - DamageAmount);
	
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (GI && !bIsEnemy)
	{
		GI->PlayerCurrentHP = FMath::Max(0, GI->PlayerCurrentHP - DamageAmount);
		UE_LOG(LogTemp, Error, TEXT("[AI_TRACE] Player Took %d. GI HP: %d"), DamageAmount, GI->PlayerCurrentHP);
	}

	if (CurrentHP <= 0) Die();
}

void ABWCharacter::Die()
{
	AGridManager* GridManager = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
	if (GridManager) GridManager->CheckVictoryCondition();
	Destroy();
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