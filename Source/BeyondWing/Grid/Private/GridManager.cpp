#include "GridManager.h"
#include "BeyondWing/Character/Public/BWCharacter.h"
#include "BeyondWing/Grid/Public/GridTile.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

AGridManager::AGridManager()
{
	PrimaryActorTick.bCanEverTick = false;
	Width = 8;
	Height = 8;
	TileSpacing = 120.0f;
	TotalSpawnDuration = 0.75f; 
	TileSpawnHeightOffset = 150.0f;
	bIsGridReady = false;
}

void AGridManager::GenerateGridInEditor()
{
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	for (AActor* Actor : AttachedActors)
	{
		if (Actor && Actor->IsValidLowLevel()) Actor->Destroy();
	}
	GridMap.Empty();
	TilePool.Empty();
	GenerateGrid();
}

void AGridManager::GenerateGrid()
{
	if (!TileClass || !GetWorld()) return;

	for (int32 x = 0; x < Width; ++x)
	{
		for (int32 y = 0; y < Height; ++y)
		{
			FVector FinalLocation = GetActorLocation() + FVector(x * TileSpacing, y * TileSpacing, 0.0f);
			FVector SpawnLocation = FinalLocation + FVector(0, 0, TileSpawnHeightOffset);
			
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;

			AGridTile* NewTile = GetWorld()->SpawnActor<AGridTile>(TileClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
			if (NewTile)
			{
				NewTile->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
				NewTile->GridCoordinate = FIntPoint(x, y);
				NewTile->SetActorHiddenInGame(true);
				NewTile->SetActorEnableCollision(false);
				NewTile->SetTileState(ETileState::None);

				GridMap.Add(FIntPoint(x, y), NewTile);
				TilePool.Add(NewTile);
			}
		}
	}
}

void AGridManager::StartDiagonalWave()
{
	if (TilePool.Num() == 0) return;
	bIsGridReady = false;
	SpawnWeightGroup(0);
}

void AGridManager::SpawnWeightGroup(int32 CurrentWeight)
{
	int32 MaxWeight = (Width - 1) + (Height - 1);
	if (CurrentWeight > MaxWeight)
	{
		FTimerHandle EnemyTimer;
		GetWorldTimerManager().SetTimer(EnemyTimer, this, &AGridManager::SpawnEnemies, 0.5f, false);
		return;
	}

	bool bAnySpawned = false;
	FVector AvgLoc = FVector::ZeroVector;
	int32 Count = 0;

	for (auto& Elem : GridMap)
	{
		if (Elem.Key.X + Elem.Key.Y == CurrentWeight)
		{
			AGridTile* Tile = Elem.Value;
			Tile->SetActorHiddenInGame(false);
			Tile->SetActorEnableCollision(true);
			
			FVector FinalLoc = GetActorLocation() + FVector(Elem.Key.X * TileSpacing, Elem.Key.Y * TileSpacing, 0.0f);
			Tile->StartLandingAnimation(FinalLoc, 0.4f);
			
			bAnySpawned = true;
			AvgLoc += Tile->GetActorLocation();
			Count++;
		}
	}

	if (bAnySpawned && TileSpawnSound)
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), TileSpawnSound, AvgLoc / (float)Count);

	float StepDelay = TotalSpawnDuration / (float)MaxWeight;
	FTimerHandle NextWaveTimer;
	FTimerDelegate Delegate;
	Delegate.BindUObject(this, &AGridManager::SpawnWeightGroup, CurrentWeight + 1);
	GetWorldTimerManager().SetTimer(NextWaveTimer, Delegate, StepDelay, false);
}

void AGridManager::SpawnEnemies()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI)
	{
		UE_LOG(LogTemp, Error, TEXT("[BWCombat] GameInstance is NULL."));
		FinalizeGridColors();
		return;
	}

	if (GI->PendingEnemyRequests.Num() == 0)
	{
		FinalizeGridColors();
		return;
	}

	int32 SpawnedCount = 0;
	for (const FEnemySpawnRequest& Request : GI->PendingEnemyRequests)
	{
		if (!GI->EnemyDataTable) continue;

		static const FString ContextString(TEXT("Enemy Spawn Context"));
		FBWEnemyData* Data = GI->EnemyDataTable->FindRow<FBWEnemyData>(Request.EnemyID, ContextString);

		if (Data && Data->EnemyClass)
		{
			AGridTile* TargetTile = GetTileAtCoordinate(Request.Coordinate);
			if (!TargetTile) continue;

			FVector SpawnLoc = TargetTile->GetActorLocation() + FVector(0, 0, 20.0f);
			FActorSpawnParameters Params;
			ABWCharacter* NewEnemy = GetWorld()->SpawnActor<ABWCharacter>(Data->EnemyClass, SpawnLoc, FRotator(0, 180.0f, 0), Params);
			
			if (NewEnemy)
			{
				NewEnemy->OwnGridCoordinate = Request.Coordinate;
				NewEnemy->bIsEnemy = true; 
				NewEnemy->EnemyDisplayName = Data->EnemyDisplayName;
				DeployedCharacters.Add(NewEnemy);
				
				NewEnemy->BaseAttackPower = Data->BaseAttackPower;
				NewEnemy->MaxHP = Data->MaxHP;
				NewEnemy->CurrentHP = Data->MaxHP;
				NewEnemy->CombatPatterns = Data->CombatPatterns;
				NewEnemy->CurrentPatternIndex = 0;
				
				SpawnedCount++;
				if (CharacterPlacementSound)
					UGameplayStatics::PlaySoundAtLocation(GetWorld(), CharacterPlacementSound, SpawnLoc);
			}
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Successfully spawned %d enemies."), SpawnedCount);

	FTimerHandle ColorTimer;
	GetWorldTimerManager().SetTimer(ColorTimer, this, &AGridManager::FinalizeGridColors, 0.8f, false);
}

void AGridManager::CheckVictoryCondition()
{
	bool bAreAnyEnemiesAlive = false;
	for (ABWCharacter* Char : DeployedCharacters)
	{
		if (Char && !Char->IsActorBeingDestroyed() && Char->bIsEnemy && Char->CurrentHP > 0)
		{
			bAreAnyEnemiesAlive = true;
			break;
		}
	}

	if (!bAreAnyEnemiesAlive)
	{
		UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
		if (GI)
		{
			GI->GenerateCombatRewards(GI->CurrentEncounterType);
			UGameplayStatics::OpenLevel(GetWorld(), TEXT("MapLevel"));
		}
	}
}

void AGridManager::FinalizeGridColors()
{
	for (auto& Elem : GridMap)
	{
		if (Elem.Key.X >= 0 && Elem.Key.X <= 2)
			Elem.Value->DefaultZoneState = ETileState::AllyZone;
		else
			Elem.Value->DefaultZoneState = ETileState::None;

		Elem.Value->SetTileState(Elem.Value->DefaultZoneState);
	}
	bIsGridReady = true;

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), FoundActors);
	for (AActor* Actor : FoundActors)
	{
		ABWCharacter* Char = Cast<ABWCharacter>(Actor);
		if (Char && Char->bIsEnemy) 
		{
			Char->UpdateEnemyIntent();
			Char->VisualizeEnemyIntent();
		}
	}
}

void AGridManager::BeginPlay()
{
	Super::BeginPlay();
	GenerateGrid();
	StartDiagonalWave();
}

void AGridManager::DrawSkillPreview(FIntPoint OriginIdx, FIntPoint TargetIdx, FBWCardData CardData)
{
	ClearAllRanges();
	FIntPoint Dir = CalculateDirection(OriginIdx, TargetIdx);
	FIntPoint Perp(-Dir.Y, Dir.X);

	for (auto& Elem : GridMap)
	{
		FIntPoint TileCoord = Elem.Key;
		int32 Forward = (TileCoord.X - OriginIdx.X) * Dir.X + (TileCoord.Y - OriginIdx.Y) * Dir.Y;
		int32 Side = (TileCoord.X - OriginIdx.X) * Perp.X + (TileCoord.Y - OriginIdx.Y) * Perp.Y;
		int32 DistToTargetX = FMath::Abs(TileCoord.X - TargetIdx.X);
		int32 DistToTargetY = FMath::Abs(TileCoord.Y - TargetIdx.Y);
		int32 MaxDistToTarget = FMath::Max(DistToTargetX, DistToTargetY);
		int32 ManhattanToOrigin = FMath::Abs(TileCoord.X - OriginIdx.X) + FMath::Abs(TileCoord.Y - OriginIdx.Y);

		bool bHighlight = false;
		switch (CardData.SkillShape)
		{
		case ESkillShape::Single:         bHighlight = (TileCoord == (OriginIdx + Dir)); break;
		case ESkillShape::Manhattan:      bHighlight = (ManhattanToOrigin <= CardData.Range && ManhattanToOrigin > 0); break;
		case ESkillShape::Cross:          bHighlight = ((TileCoord.X == OriginIdx.X || TileCoord.Y == OriginIdx.Y) && ManhattanToOrigin <= CardData.Range && ManhattanToOrigin > 0); break;
		case ESkillShape::T_Shape:        bHighlight = ((Forward > 0 && Forward <= CardData.Range && Side == 0) || (Forward == CardData.Range && FMath::Abs(Side) == 1)); break;
		case ESkillShape::Line:           bHighlight = (Forward > 0 && Forward <= CardData.Range && Side == 0); break;
		case ESkillShape::HorizontalLine: bHighlight = (Forward == ((TargetIdx.X - OriginIdx.X)*Dir.X + (TargetIdx.Y - OriginIdx.Y)*Dir.Y) && FMath::Abs(Side) <= CardData.Range); break;
		case ESkillShape::VerticalLine:   bHighlight = (Side == ((TargetIdx.X - OriginIdx.X)*Perp.X + (TargetIdx.Y - OriginIdx.Y)*Perp.Y) && FMath::Abs(Forward - ((TargetIdx.X - OriginIdx.X)*Dir.X + (TargetIdx.Y - OriginIdx.Y)*Dir.Y)) <= CardData.Range); break;
		case ESkillShape::Square3x3:      bHighlight = (MaxDistToTarget <= 1); break;
		case ESkillShape::Square5x5:      bHighlight = (MaxDistToTarget <= 2); break;
		case ESkillShape::FrontSquare3x3: bHighlight = (FMath::Max(FMath::Abs(TileCoord.X - (OriginIdx.X + Dir.X*2)), FMath::Abs(TileCoord.Y - (OriginIdx.Y + Dir.Y*2))) <= 1); break;
		case ESkillShape::Ring:           bHighlight = (MaxDistToTarget == CardData.Range); break;
		case ESkillShape::CrossDiagonal:  bHighlight = (DistToTargetX == DistToTargetY && DistToTargetX <= CardData.Range && DistToTargetX > 0); break;
		case ESkillShape::Cone:           bHighlight = (Forward > 0 && Forward <= CardData.Range && FMath::Abs(Side) <= Forward); break;
		case ESkillShape::Arrowhead:      bHighlight = (Forward > 0 && Forward <= CardData.Range && FMath::Abs(Side) == Forward); break;
		case ESkillShape::Checkerboard:   bHighlight = ((TileCoord.X + TileCoord.Y) % 2 != (OriginIdx.X + OriginIdx.Y) % 2); break;
		case ESkillShape::Surround:       bHighlight = (FMath::Max(FMath::Abs(TileCoord.X - OriginIdx.X), FMath::Abs(TileCoord.Y - OriginIdx.Y)) == 1); break;
		case ESkillShape::FrontRow:       bHighlight = (Forward == 1); break;
		case ESkillShape::BackRow:        bHighlight = (Forward == -1); break;
		case ESkillShape::AllEnemies:     bHighlight = (TileCoord.X >= Width - 3); break;
		case ESkillShape::AllAllies:      bHighlight = (TileCoord.X <= 2); break;
		case ESkillShape::Random:         bHighlight = (ManhattanToOrigin <= CardData.Range && ManhattanToOrigin > 0); break; 
		case ESkillShape::Self:           bHighlight = (TileCoord == OriginIdx); break;
		}

		if (bHighlight)
		{
			ABWCharacter* TargetChar = GetCharacterAtCoordinate(TileCoord);
			if (TargetChar && TargetChar->bIsEnemy) Elem.Value->SetTileState(ETileState::AttackTarget);
			else Elem.Value->SetTileState(CardData.DisplayColor);
		}
	}
}

void AGridManager::AddEnemyIntentPreview(FIntPoint OriginIdx, FIntPoint TargetIdx, ESkillShape Shape, int32 Range, bool bMoveBeforeAttack)
{
	FIntPoint ActualOrigin = OriginIdx;
	FIntPoint Dir = CalculateDirection(OriginIdx, TargetIdx);

	if (bMoveBeforeAttack)
	{
		FIntPoint NextCoord = OriginIdx + Dir;
		if (NextCoord.X >= 0 && NextCoord.X < Width && NextCoord.Y >= 0 && NextCoord.Y < Height)
		{
			if (GetCharacterAtCoordinate(NextCoord) == nullptr)
			{
				ActualOrigin = NextCoord;
				AGridTile* MoveTile = GetTileAtCoordinate(NextCoord);
				if (MoveTile) MoveTile->SetTileState(ETileState::MoveRange);
			}
		}
	}

	FIntPoint Perp(-Dir.Y, Dir.X);
	for (auto& Elem : GridMap)
	{
		FIntPoint TileCoord = Elem.Key;
		int32 Forward = (TileCoord.X - ActualOrigin.X) * Dir.X + (TileCoord.Y - ActualOrigin.Y) * Dir.Y;
		int32 Side = (TileCoord.X - ActualOrigin.X) * Perp.X + (TileCoord.Y - ActualOrigin.Y) * Perp.Y;
		int32 ManhattanToOrigin = FMath::Abs(TileCoord.X - ActualOrigin.X) + FMath::Abs(TileCoord.Y - ActualOrigin.Y);

		bool bHighlight = false;
		switch (Shape)
		{
		case ESkillShape::Single:         bHighlight = (TileCoord == (ActualOrigin + Dir)); break;
		case ESkillShape::Manhattan:      bHighlight = (ManhattanToOrigin <= Range && ManhattanToOrigin > 0); break;
		case ESkillShape::Cross:          bHighlight = ((TileCoord.X == ActualOrigin.X || TileCoord.Y == ActualOrigin.Y) && ManhattanToOrigin <= Range && ManhattanToOrigin > 0); break;
		case ESkillShape::T_Shape:        bHighlight = ((Forward > 0 && Forward <= Range && Side == 0) || (Forward == Range && FMath::Abs(Side) == 1)); break;
		case ESkillShape::Line:           bHighlight = (Forward > 0 && Forward <= Range && Side == 0); break;
		case ESkillShape::HorizontalLine: bHighlight = (Forward == ((TargetIdx.X - ActualOrigin.X)*Dir.X + (TargetIdx.Y - ActualOrigin.Y)*Dir.Y) && FMath::Abs(Side) <= Range); break;
		case ESkillShape::VerticalLine:   bHighlight = (Side == ((TargetIdx.X - ActualOrigin.X)*Perp.X + (TargetIdx.Y - ActualOrigin.Y)*Perp.Y) && FMath::Abs(Forward - ((TargetIdx.X - ActualOrigin.X)*Dir.X + (TargetIdx.Y - ActualOrigin.Y)*Dir.Y)) <= Range); break;
		case ESkillShape::Square3x3:      bHighlight = (FMath::Max(FMath::Abs(TileCoord.X - TargetIdx.X), FMath::Abs(TileCoord.Y - TargetIdx.Y)) <= 1); break;
		case ESkillShape::Square5x5:      bHighlight = (FMath::Max(FMath::Abs(TileCoord.X - TargetIdx.X), FMath::Abs(TileCoord.Y - TargetIdx.Y)) <= 2); break;
		case ESkillShape::Cone:           bHighlight = (Forward > 0 && Forward <= Range && FMath::Abs(Side) <= Forward); break;
		case ESkillShape::Arrowhead:      bHighlight = (Forward > 0 && Forward <= Range && FMath::Abs(Side) == Forward); break;
		case ESkillShape::Surround:       bHighlight = (FMath::Max(FMath::Abs(TileCoord.X - ActualOrigin.X), FMath::Abs(TileCoord.Y - ActualOrigin.Y)) == 1); break;
		case ESkillShape::FrontRow:       bHighlight = (Forward == 1); break;
		case ESkillShape::BackRow:        bHighlight = (Forward == -1); break;
		case ESkillShape::AllEnemies:     bHighlight = (TileCoord.X >= Width - 3); break;
		case ESkillShape::AllAllies:      bHighlight = (TileCoord.X <= 2); break;
		case ESkillShape::Random:         bHighlight = (ManhattanToOrigin <= Range && ManhattanToOrigin > 0); break;
		}

		if (bHighlight && Elem.Value->CurrentState != ETileState::MoveRange) 
		{
			Elem.Value->SetTileState(ETileState::EnemyAttackRange); 
		}
	}
}

void AGridManager::DrawMovementPreview(FIntPoint OriginIdx, int32 MoveRange)
{
	ClearAllRanges();
	for (auto& Elem : GridMap)
	{
		FIntPoint TileCoord = Elem.Key;
		int32 Distance = FMath::Abs(OriginIdx.X - TileCoord.X) + FMath::Abs(OriginIdx.Y - TileCoord.Y);
		// [수정] 이동 범위 표시 시 내 발밑(Distance == 0)도 포함하여 파란색으로 표시
		if (Distance >= 0 && Distance <= MoveRange)
		{
			if (GetCharacterAtCoordinate(TileCoord) == nullptr || TileCoord == OriginIdx) Elem.Value->SetTileState(ETileState::MoveRange);
		}
	}
}

ABWCharacter* AGridManager::GetCharacterAtCoordinate(FIntPoint Coord)
{
	DeployedCharacters.RemoveAll([](ABWCharacter* C) { return C == nullptr || !C->IsValidLowLevel() || C->IsActorBeingDestroyed(); });
	for (ABWCharacter* Char : DeployedCharacters) if (Char->OwnGridCoordinate == Coord) return Char;
	return nullptr;
}

void AGridManager::DrawPlacementGuidelines()
{
	for (auto& Elem : GridMap)
	{
		if (Elem.Key.X >= 0 && Elem.Key.X <= 2) Elem.Value->SetTileState(ETileState::AllyZone);
		else Elem.Value->SetTileState(ETileState::None);
	}
	RefreshEnemyIntents();
}

bool AGridManager::IsValidAllyPlacementArea(FIntPoint Coord)
{
	if (Coord.X >= 0 && Coord.X <= 2 && Coord.Y >= 0 && Coord.Y < Height)
	{
		for (ABWCharacter* Char : DeployedCharacters) if (Char && Char->OwnGridCoordinate == Coord) return false;
		return true;
	}
	return false;
}

bool AGridManager::TryPlaceCharacter(FIntPoint TargetCoord, TSubclassOf<ABWCharacter> CharClass)
{
	if (!bIsGridReady || !CharClass || !IsValidAllyPlacementArea(TargetCoord)) return false;
	AGridTile* TargetTile = GetTileAtCoordinate(TargetCoord);
	if (!TargetTile) return false;

	FVector SpawnLoc = TargetTile->GetActorLocation() + FVector(0, 0, 20);
	ABWCharacter* NewChar = GetWorld()->SpawnActor<ABWCharacter>(CharClass, SpawnLoc, FRotator::ZeroRotator);
	if (NewChar)
	{
		NewChar->OwnGridCoordinate = TargetCoord;
		DeployedCharacters.Add(NewChar);
		for (auto& Elem : GridMap)
		{
			Elem.Value->DefaultZoneState = ETileState::None;
			Elem.Value->SetTileState(ETileState::None);
		}
		if (CharacterPlacementSound) UGameplayStatics::PlaySoundAtLocation(GetWorld(), CharacterPlacementSound, SpawnLoc);
		return true;
	}
	return false;
}

void AGridManager::ExecuteSkill(FIntPoint OriginIdx, FIntPoint TargetIdx, FBWCardData CardData) 
{ 
	TArray<FIntPoint> HitTiles = GetAffectedCoordinates(OriginIdx, TargetIdx, CardData.SkillShape, CardData.Range);
	bool bCritical = false;
	ABWCharacter* Attacker = GetCharacterAtCoordinate(OriginIdx);
	if (!Attacker) return;

	int32 Damage = Attacker->CalculateFinalCardDamage(CardData, bCritical);
	
	// [추가] 시각 및 효과음 연출
	FVector AttackerLoc = GetTileAtCoordinate(OriginIdx)->GetActorLocation() + FVector(0, 0, 40);
	
	// [수정] 이펙트가 터질 타겟 위치 (공격 방향의 첫 번째 그리드)
	AGridTile* TargetTile = GetTileAtCoordinate(TargetIdx);
	FVector SpawnLoc = TargetTile ? TargetTile->GetActorLocation() + FVector(0, 0, 40) : AttackerLoc;
	
	FRotator ShootRot = (SpawnLoc - AttackerLoc).Rotation();

	// 공격 속도(ASP) 만큼 이펙트 반복 연출 (PHA 20당 1회 추가)
	int32 RepeatCount = Attacker->GetEffectiveASP();
	
	for (int32 i = 0; i < RepeatCount; ++i)
	{
		FTimerHandle EffectTimer;
		float Delay = i * 0.25f; // 연타 간격
		
		GetWorldTimerManager().SetTimer(EffectTimer, [this, CardData, SpawnLoc, ShootRot]() {
			if (CardData.SkillEffect.LoadSynchronous())
			{
				// [수정] 본인 몸이 아니라, 공격 방향의 타겟 그리드에서 이펙트 생성
				UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), CardData.SkillEffect.Get(), SpawnLoc, ShootRot);
			}
			if (CardData.CardSound.LoadSynchronous())
			{
				UGameplayStatics::PlaySoundAtLocation(GetWorld(), CardData.CardSound.Get(), SpawnLoc);
			}
		}, Delay + 0.01f, false);
	}

	for (FIntPoint HitCoord : HitTiles)
	{
		ABWCharacter* TargetChar = GetCharacterAtCoordinate(HitCoord);
		if (TargetChar && TargetChar->bIsEnemy != Attacker->bIsEnemy) 
		{
			TargetChar->ReceiveDamage(Damage);
		}
	}
}

void AGridManager::ClearAllRanges() 
{ 
	for (auto& Elem : GridMap) if (Elem.Value) Elem.Value->SetTileState(Elem.Value->DefaultZoneState); 
	RefreshEnemyIntents();
}

void AGridManager::RefreshEnemyIntents()
{
	if (!bIsGridReady) return;
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), FoundActors);
	for (AActor* Actor : FoundActors)
	{
		ABWCharacter* Char = Cast<ABWCharacter>(Actor);
		if (Char && Char->bIsEnemy && Char->CurrentHP > 0) Char->VisualizeEnemyIntent();
	}
}

TArray<FIntPoint> AGridManager::GetAffectedCoordinates(FIntPoint OriginIdx, FIntPoint TargetIdx, ESkillShape Shape, int32 Range)
{
	TArray<FIntPoint> Result;
	FIntPoint Dir = CalculateDirection(OriginIdx, TargetIdx);
	FIntPoint Perp(-Dir.Y, Dir.X);

	for (auto& Elem : GridMap)
	{
		FIntPoint TileCoord = Elem.Key;
		int32 Forward = (TileCoord.X - OriginIdx.X) * Dir.X + (TileCoord.Y - OriginIdx.Y) * Dir.Y;
		int32 Side = (TileCoord.X - OriginIdx.X) * Perp.X + (TileCoord.Y - OriginIdx.Y) * Perp.Y;
		int32 DistToTargetX = FMath::Abs(TileCoord.X - TargetIdx.X);
		int32 DistToTargetY = FMath::Abs(TileCoord.Y - TargetIdx.Y);
		int32 ManhattanToOrigin = FMath::Abs(TileCoord.X - OriginIdx.X) + FMath::Abs(TileCoord.Y - OriginIdx.Y);

		bool bHighlight = false;
		switch (Shape)
		{
		case ESkillShape::Single:         bHighlight = (TileCoord == (OriginIdx + Dir * Range)); break;
		case ESkillShape::Manhattan:      bHighlight = (ManhattanToOrigin <= Range && ManhattanToOrigin > 0); break;
		case ESkillShape::Cross:          bHighlight = ((TileCoord.X == OriginIdx.X || TileCoord.Y == OriginIdx.Y) && ManhattanToOrigin <= Range && ManhattanToOrigin > 0); break;
		case ESkillShape::T_Shape:        bHighlight = ((Forward > 0 && Forward <= Range && Side == 0) || (Forward == Range && FMath::Abs(Side) == 1)); break;
		case ESkillShape::Line:           bHighlight = (Forward > 0 && Forward <= Range && Side == 0); break;
		case ESkillShape::HorizontalLine: bHighlight = (Forward == ((TargetIdx.X - OriginIdx.X)*Dir.X + (TargetIdx.Y - OriginIdx.Y)*Dir.Y) && FMath::Abs(Side) <= Range); break;
		case ESkillShape::VerticalLine:   bHighlight = (Side == ((TargetIdx.X - OriginIdx.X)*Perp.X + (TargetIdx.Y - OriginIdx.Y)*Perp.Y) && FMath::Abs(Forward - ((TargetIdx.X - OriginIdx.X)*Dir.X + (TargetIdx.Y - OriginIdx.Y)*Dir.Y)) <= Range); break;
		case ESkillShape::Square3x3:      bHighlight = (FMath::Max(DistToTargetX, DistToTargetY) <= 1); break;
		case ESkillShape::Square5x5:      bHighlight = (FMath::Max(DistToTargetX, DistToTargetY) <= 2); break;
		case ESkillShape::FrontSquare3x3: bHighlight = (FMath::Max(FMath::Abs(TileCoord.X - (OriginIdx.X + Dir.X*2)), FMath::Abs(TileCoord.Y - (OriginIdx.Y + Dir.Y*2))) <= 1); break;
		case ESkillShape::Ring:           bHighlight = (FMath::Max(DistToTargetX, DistToTargetY) == Range); break;
		case ESkillShape::CrossDiagonal:  bHighlight = (DistToTargetX == DistToTargetY && DistToTargetX <= Range && DistToTargetX > 0); break;
		case ESkillShape::Cone:           bHighlight = (Forward > 0 && Forward <= Range && FMath::Abs(Side) <= Forward); break;
		case ESkillShape::Arrowhead:      bHighlight = (Forward > 0 && Forward <= Range && FMath::Abs(Side) == Forward); break;
		case ESkillShape::Checkerboard:   bHighlight = ((TileCoord.X + TileCoord.Y) % 2 != (OriginIdx.X + OriginIdx.Y) % 2); break;
		case ESkillShape::Surround:       bHighlight = (FMath::Max(FMath::Abs(TileCoord.X - OriginIdx.X), FMath::Abs(TileCoord.Y - OriginIdx.Y)) == 1); break;
		case ESkillShape::FrontRow:       bHighlight = (Forward == 1); break;
		case ESkillShape::BackRow:        bHighlight = (Forward == -1); break;
		case ESkillShape::AllEnemies:     bHighlight = (TileCoord.X >= Width - 3); break;
		case ESkillShape::AllAllies:      bHighlight = (TileCoord.X <= 2); break;
		case ESkillShape::Random:         bHighlight = (ManhattanToOrigin <= Range && ManhattanToOrigin > 0); break;
		case ESkillShape::Self:           bHighlight = (TileCoord == OriginIdx); break;
		}
		if (bHighlight) Result.Add(TileCoord);
	}
	return Result;
}

AGridTile* AGridManager::GetTileAtCoordinate(FIntPoint Coord) { return GridMap.Contains(Coord) ? GridMap[Coord] : nullptr; }
FIntPoint AGridManager::CalculateDirection(FIntPoint Origin, FIntPoint Target) 
{
	if (Origin == Target) return FIntPoint(1, 0); 
	int32 DeltaX = Target.X - Origin.X;
	int32 DeltaY = Target.Y - Origin.Y;
	if (FMath::Abs(DeltaX) >= FMath::Abs(DeltaY)) return FIntPoint(FMath::Sign(DeltaX), 0);
	return FIntPoint(0, FMath::Sign(DeltaY));
}
void AGridManager::OnConstruction(const FTransform& Transform) { Super::OnConstruction(Transform); }