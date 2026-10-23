#include "GridManager.h"
#include "BeyondWing/Character/Public/BWCharacter.h"
#include "BeyondWing/Grid/Public/GridTile.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "Engine/Engine.h"

AGridManager::AGridManager()
{
	PrimaryActorTick.bCanEverTick = false;
	Width = 8;
	Height = 8; // [수정] 8×8 (사용자 결정). 아군 구역은 왼쪽 AllyZoneColumns(4)열
	TileSpacing = 120.0f;
	TotalSpawnDuration = 0.75f; 
	TileSpawnHeightOffset = 150.0f;
	bIsGridReady = false;
}

FVector AGridManager::GetTileWorldLocation(FIntPoint Coord) const
{
	// [수정] 전투 기획서 2장: 가로 8 × 세로 5, 왼쪽 4열이 아군. 카메라가 월드 +X를 보고 있으므로
	// 칸의 X(아군→적)를 월드 +Y(화면 오른쪽)에, Y를 월드 X(화면 깊이)에 놓는다
	if (bColumnsAlongWorldY) return GetActorLocation() + FVector(Coord.Y * TileSpacing, Coord.X * TileSpacing, 0.0f);
	return GetActorLocation() + FVector(Coord.X * TileSpacing, Coord.Y * TileSpacing, 0.0f);
}

bool AGridManager::WorldToGrid(FVector WorldLocation, FIntPoint& OutCoord) const
{
	if (TileSpacing <= 0.0f) return false;
	const FVector Local = WorldLocation - GetActorLocation();
	// 칸 중심이 GetTileWorldLocation이므로 반 칸 범위까지 같은 칸으로 본다
	const int32 AlongX = FMath::RoundToInt((bColumnsAlongWorldY ? Local.Y : Local.X) / TileSpacing);
	const int32 AlongY = FMath::RoundToInt((bColumnsAlongWorldY ? Local.X : Local.Y) / TileSpacing);
	OutCoord = FIntPoint(AlongX, AlongY);
	return AlongX >= 0 && AlongX < Width && AlongY >= 0 && AlongY < Height;
}

FRotator AGridManager::GetFacingRotation(FIntPoint GridDir) const
{
	const FVector WorldDir = GetTileWorldLocation(GridDir) - GetTileWorldLocation(FIntPoint::ZeroValue);
	return FRotator(0.0f, WorldDir.Rotation().Yaw, 0.0f);
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
			FVector FinalLocation = GetTileWorldLocation(FIntPoint(x, y));
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
			
			FVector FinalLoc = GetTileWorldLocation(Elem.Key);
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
			ABWCharacter* NewEnemy = GetWorld()->SpawnActor<ABWCharacter>(Data->EnemyClass, SpawnLoc, GetFacingRotation(FIntPoint(-1, 0)), Params); // 적은 아군 쪽(왼쪽)을 본다
			
			if (NewEnemy)
			{
				NewEnemy->OwnGridCoordinate = Request.Coordinate;
				NewEnemy->bIsEnemy = true; 
				NewEnemy->EnemyDisplayName = Data->EnemyDisplayName;
				DeployedCharacters.Add(NewEnemy);
				
				NewEnemy->EnemyOrder = SpawnedCount;
				NewEnemy->MaxHP = Data->MaxHP;
				NewEnemy->CurrentHP = Data->MaxHP;
				NewEnemy->CombatPatterns = Data->CombatPatterns;
				NewEnemy->CurrentPatternIndex = 0;
				NewEnemy->SpeedDice = Data->SpeedDice;
				NewEnemy->Block = Data->StartingBlock;
				
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
	if (bBattleOver) return;

	bool bAreAnyEnemiesAlive = false;
	bool bIsPlayerAlive = false;
	bool bHasPlayer = false;
	for (ABWCharacter* Char : DeployedCharacters)
	{
		if (!Char || Char->IsActorBeingDestroyed()) continue;
		if (Char->bIsEnemy) bAreAnyEnemiesAlive |= Char->CurrentHP > 0;
		else { bHasPlayer = true; bIsPlayerAlive |= Char->CurrentHP > 0; }
	}

	// [추가] 1장 패배: 아군 말이 쓰러지면 패배 (HP 0이면 사망)
	if (bHasPlayer && !bIsPlayerAlive)
	{
		bBattleOver = true;
		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Defeat: the player fell."));
#if !UE_BUILD_SHIPPING
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("패배: 아군 말이 쓰러졌습니다."));
#endif
		// [추가] 판이 끝났으니 이어하기 저장을 지운다
		if (UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance())) GI->DeleteSavedRun();
		FTimerHandle DefeatTimer;
		GetWorldTimerManager().SetTimer(DefeatTimer, [this]() { UGameplayStatics::OpenLevel(GetWorld(), TEXT("TitleLevel")); }, 2.0f, false);
		return;
	}

	if (!bAreAnyEnemiesAlive)
	{
		bBattleOver = true;
		UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
		if (GI)
		{
			GI->GenerateCombatRewards(GI->CurrentEncounterType);
			GI->SaveRun(); // [추가] 이긴 상태(HP·보상·지도 위치)로 이어하기 저장
			UGameplayStatics::OpenLevel(GetWorld(), TEXT("MapLevel"));
		}
	}
}

void AGridManager::FinalizeGridColors()
{
	// [추가] 이번 전투의 장애물 벽
	if (UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance())) WallCoords = GI->PendingWallTiles;

	for (auto& Elem : GridMap)
	{
		if (WallCoords.Contains(Elem.Key))
			Elem.Value->DefaultZoneState = ETileState::Wall;
		else if (IsAllyZoneColumn(Elem.Key.X))
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

	// [추가] 에디터에서 GenerateGridInEditor로 미리 깔아 둔 타일은 크기가 옛 값일 수 있으므로 런타임 그리드와 겹치지 않게 제거
	// (레벨에 저장되면 부착은 풀리고 Owner만 남는다)
	TArray<AActor*> ExistingTiles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGridTile::StaticClass(), ExistingTiles);
	for (AActor* Actor : ExistingTiles)
	{
		if (Actor && (Actor->GetOwner() == this || Actor->GetAttachParentActor() == this)) Actor->Destroy();
	}
	// GridMap·TilePool도 레벨에 함께 저장되어 옛 타일을 가리키므로 비우고 새로 만든다
	GridMap.Empty();
	TilePool.Empty();

	CombatRandom.Initialize(FMath::Rand());
	UE_LOG(LogTemp, Log, TEXT("[BWCombat] Combat random seed: %d"), CombatRandom.GetInitialSeed());

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
		case ESkillShape::AllEnemies:     bHighlight = (!IsAllyZoneColumn(TileCoord.X)); break;
		case ESkillShape::AllAllies:      bHighlight = (IsAllyZoneColumn(TileCoord.X)); break;
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
		case ESkillShape::AllEnemies:     bHighlight = (!IsAllyZoneColumn(TileCoord.X)); break;
		case ESkillShape::AllAllies:      bHighlight = (IsAllyZoneColumn(TileCoord.X)); break;
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

	// [수정] 맨해튼 거리 대신 실제 경로 기준 (다른 말은 못 지나가고, 접촉 칸에서 멈춤). 내 발밑도 표시
	TMap<FIntPoint, int32> Distance;
	TMap<FIntPoint, FIntPoint> Parent;
	BuildMoveField(OriginIdx, MoveRange, false, Distance, Parent);

	for (const TPair<FIntPoint, int32>& Reach : Distance)
	{
		if (AGridTile* Tile = GetTileAtCoordinate(Reach.Key)) Tile->SetTileState(ETileState::MoveRange);
	}
}

void AGridManager::SetWallTiles(const TArray<FIntPoint>& Coords)
{
	WallCoords = Coords;
	for (auto& Elem : GridMap)
	{
		const bool bWall = WallCoords.Contains(Elem.Key);
		if (bWall) Elem.Value->DefaultZoneState = ETileState::Wall;
		else if (Elem.Value->DefaultZoneState == ETileState::Wall) Elem.Value->DefaultZoneState = ETileState::None;
		Elem.Value->SetTileState(Elem.Value->DefaultZoneState);
	}
}

bool AGridManager::IsWallOrOutside(FIntPoint Coord) const
{
	return !GridMap.Contains(Coord) || WallCoords.Contains(Coord);
}

bool AGridManager::TryPushCharacter(ABWCharacter* Target, FIntPoint Dir, bool& bOutHitWall)
{
	bOutHitWall = false;
	if (!Target) return false;

	const FIntPoint Dest = Target->OwnGridCoordinate + Dir;
	if (IsWallOrOutside(Dest))
	{
		bOutHitWall = true;
		return false;
	}
	if (GetCharacterAtCoordinate(Dest)) return false; // 다른 말에 막히면 밀리지 않는다 (말은 벽이 아님)

	Target->OwnGridCoordinate = Dest;
	if (AGridTile* DestTile = GetTileAtCoordinate(Dest)) Target->AddMovementStep(DestTile->GetActorLocation() + FVector(0, 0, 20));
	return true;
}

bool AGridManager::IsTileWalkable(FIntPoint Coord)
{
	AGridTile* Tile = GetTileAtCoordinate(Coord);
	if (!Tile || Tile->DefaultZoneState == ETileState::Wall) return false;
	return GetCharacterAtCoordinate(Coord) == nullptr;
}

ABWCharacter* AGridManager::FindContactOpponent(FIntPoint Coord, bool bMoverIsEnemy)
{
	static const FIntPoint Dirs[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	for (const FIntPoint& Dir : Dirs)
	{
		ABWCharacter* Other = GetCharacterAtCoordinate(Coord + Dir);
		if (Other && Other->bIsEnemy != bMoverIsEnemy && Other->CurrentHP > 0) return Other;
	}
	return nullptr;
}

void AGridManager::BuildMoveField(FIntPoint Origin, int32 MaxSteps, bool bMoverIsEnemy, TMap<FIntPoint, int32>& OutDistance, TMap<FIntPoint, FIntPoint>& OutParent)
{
	static const FIntPoint Dirs[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };

	OutDistance.Reset();
	OutParent.Reset();
	OutDistance.Add(Origin, 0);

	// 너비 우선 탐색: 속도 1당 1칸
	TArray<FIntPoint> Queue;
	Queue.Add(Origin);
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const FIntPoint Current = Queue[Head];
		const int32 Dist = OutDistance[Current];
		if (Dist >= MaxSteps) continue;

		// 이동하다 상대와 접촉하면 그 칸에서 멈춘다 (출발 칸은 이미 접촉 중이어도 떠날 수 있음)
		if (Current != Origin && FindContactOpponent(Current, bMoverIsEnemy)) continue;

		for (const FIntPoint& Dir : Dirs)
		{
			const FIntPoint Next = Current + Dir;
			if (OutDistance.Contains(Next) || !IsTileWalkable(Next)) continue;
			OutDistance.Add(Next, Dist + 1);
			OutParent.Add(Next, Current);
			Queue.Add(Next);
		}
	}
}

TArray<FIntPoint> AGridManager::FindRushPath(FIntPoint Origin, FIntPoint Target, int32 MaxSteps, bool bMoverIsEnemy)
{
	TArray<FIntPoint> Path;
	const FIntPoint Delta = Target - Origin;
	if (MaxSteps <= 0 || FMath::Abs(Delta.X) + FMath::Abs(Delta.Y) <= 1) return Path;

	// 거리 제한 없이 펼쳐서 '가장 가까운 접촉 칸'을 고른 뒤 속도만큼 자른다
	TMap<FIntPoint, int32> Distance;
	TMap<FIntPoint, FIntPoint> Parent;
	BuildMoveField(Origin, Width * Height, bMoverIsEnemy, Distance, Parent);

	bool bFound = false;
	FIntPoint Goal = Origin;
	int32 BestDist = MAX_int32;
	static const FIntPoint Dirs[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	for (const FIntPoint& Dir : Dirs)
	{
		const int32* D = Distance.Find(Target + Dir);
		if (D && *D < BestDist) { BestDist = *D; Goal = Target + Dir; bFound = true; }
	}

	// 접촉 칸이 모두 막혔으면 Target에 가장 가까워지는 칸 (같으면 덜 걷는 칸)
	if (!bFound)
	{
		int32 BestManhattan = FMath::Abs(Delta.X) + FMath::Abs(Delta.Y);
		for (const TPair<FIntPoint, int32>& Reach : Distance)
		{
			const int32 Manhattan = FMath::Abs(Target.X - Reach.Key.X) + FMath::Abs(Target.Y - Reach.Key.Y);
			if (Manhattan < BestManhattan || (Manhattan == BestManhattan && bFound && Reach.Value < BestDist))
			{
				BestManhattan = Manhattan; BestDist = Reach.Value; Goal = Reach.Key; bFound = true;
			}
		}
		if (!bFound) return Path;
	}

	for (FIntPoint Step = Goal; Step != Origin; Step = Parent[Step]) Path.Insert(Step, 0);
	if (Path.Num() > MaxSteps) Path.SetNum(MaxSteps);
	return Path;
}

TArray<FIntPoint> AGridManager::FindMovePath(FIntPoint Origin, FIntPoint Target, int32 MaxSteps, bool bMoverIsEnemy)
{
	TArray<FIntPoint> Path;
	TMap<FIntPoint, int32> Distance;
	TMap<FIntPoint, FIntPoint> Parent;
	BuildMoveField(Origin, MaxSteps, bMoverIsEnemy, Distance, Parent);
	if (Target == Origin || !Distance.Contains(Target)) return Path;

	for (FIntPoint Step = Target; Step != Origin; Step = Parent[Step])
	{
		Path.Insert(Step, 0);
	}
	return Path;
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
		if (Elem.Value->DefaultZoneState == ETileState::Wall) Elem.Value->SetTileState(ETileState::Wall);
		else if (IsAllyZoneColumn(Elem.Key.X)) Elem.Value->SetTileState(ETileState::AllyZone);
		else Elem.Value->SetTileState(ETileState::None);
	}
	RefreshEnemyIntents();
}

bool AGridManager::IsValidAllyPlacementArea(FIntPoint Coord)
{
	if (IsAllyZoneColumn(Coord.X) && Coord.Y >= 0 && Coord.Y < Height && !WallCoords.Contains(Coord))
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
	ABWCharacter* NewChar = GetWorld()->SpawnActor<ABWCharacter>(CharClass, SpawnLoc, GetFacingRotation(FIntPoint(1, 0))); // 아군은 적 쪽(오른쪽)을 본다
	if (NewChar)
	{
		NewChar->OwnGridCoordinate = TargetCoord;
		DeployedCharacters.Add(NewChar);
		for (auto& Elem : GridMap)
		{
			// 배치가 끝나면 아군 구역 표시는 지우되 벽은 남긴다
			Elem.Value->DefaultZoneState = WallCoords.Contains(Elem.Key) ? ETileState::Wall : ETileState::None;
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
		if (Char && Char->bIsEnemy && Char->CurrentHP > 0)
		{
			// 내가 움직이면 적의 돌진 경로도 바뀌므로 지금 위치 기준으로 다시 계산해서 보여 준다
			Char->UpdateEnemyIntent();
			Char->VisualizeEnemyIntent();
		}
	}

	// [추가] 말이 서 있는 칸을 빛나게 한다 (적은 빨강, 나는 청록). 공격 대상·마우스 강조는 덮지 않는다
	for (AActor* Actor : FoundActors)
	{
		ABWCharacter* Char = Cast<ABWCharacter>(Actor);
		if (!Char || Char->CurrentHP <= 0 || Char->IsActorBeingDestroyed()) continue;
		AGridTile* Tile = GetTileAtCoordinate(Char->OwnGridCoordinate);
		// 적이 노리는 내 칸(보라)은 그대로 둬서 적 의도 툴팁이 뜨게 한다
		if (!Tile || Tile->CurrentState == ETileState::AttackTarget || Tile->CurrentState == ETileState::Hovered ||
			Tile->CurrentState == ETileState::MoveRange || Tile->CurrentState == ETileState::EnemyAttackRange) continue;
		Tile->SetTileState(Char->bIsEnemy ? ETileState::EnemyUnit : ETileState::PlayerUnit);
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
		case ESkillShape::AllEnemies:     bHighlight = (!IsAllyZoneColumn(TileCoord.X)); break;
		case ESkillShape::AllAllies:      bHighlight = (IsAllyZoneColumn(TileCoord.X)); break;
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