#include "BWPlayerController.h"
#include "GridManager.h"
#include "DeckComponent.h"
#include "BWCharacter.h"
#include "BWHandWidget.h"
#include "BeyondWing/UI/Public/BWBattleHUDWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/CameraActor.h"
#include "Blueprint/WidgetTree.h"

ABWPlayerController::ABWPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bIsSkillTargetingMode = false; 
	bIsBasicAttackMode = false;
	CurrentSkillRange = 3;        
	SelectedCardIndex = -1; 
	LastHoveredTile = nullptr;
	HandWidget = nullptr;
	PlacementPreviewActor = nullptr;

	// 기본 공격 템플릿 초기화
	BasicAttackTemplate.CardName = TEXT("기본 공격");
	BasicAttackTemplate.SkillShape = ESkillShape::Single;
	BasicAttackTemplate.Range = 1;
	BasicAttackTemplate.APCost = 0;
	BasicAttackTemplate.ATKScale = 1.0f;
}

void ABWPlayerController::BeginPlay()
{
	Super::BeginPlay();
	bShowMouseCursor = true;
	ActivateTouchInterface(nullptr);
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	AActor* FoundCamera = UGameplayStatics::GetActorOfClass(GetWorld(), ACameraActor::StaticClass());
	if (FoundCamera) SetViewTargetWithBlend(FoundCamera);

	AActor* FoundGrid = UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass());
	if (FoundGrid) TargetGridManager = Cast<AGridManager>(FoundGrid);
}

void ABWPlayerController::SetupInputComponent() { Super::SetupInputComponent(); }

void ABWPlayerController::EnterMovementMode()
{
	if (PossessedBWCharacter && PossessedBWCharacter->CurrentMovePoints > 0)
	{
		CancelSelection();
		bIsMovementMode = true;
		if (TargetGridManager) TargetGridManager->DrawMovementPreview(PossessedBWCharacter->OwnGridCoordinate, PossessedBWCharacter->CurrentMovePoints);
	}
}

void ABWPlayerController::EnterBasicAttackMode()
{
	if (PossessedBWCharacter && PossessedBWCharacter->DeckComponent && PossessedBWCharacter->DeckComponent->bCanFreeAttack)
	{
		CancelSelection();
		bIsBasicAttackMode = true;
		
		// [수정] 기본 공격 템플릿의 색상 명시적 지정
		BasicAttackTemplate.DisplayColor = ETileState::AttackRange;

		if (TargetGridManager)
		{
			// 현재 마우스 아래 타일로 즉시 미리보기 갱신
			if (CurrentTileUnderCursor)
			{
				TargetGridManager->DrawSkillPreview(PossessedBWCharacter->OwnGridCoordinate, CurrentTileUnderCursor->GridCoordinate, BasicAttackTemplate);
			}
			else
			{
				// 마우스가 그리드 위에 없으면 일단 발밑 방향이라도 표시
				TargetGridManager->DrawSkillPreview(PossessedBWCharacter->OwnGridCoordinate, PossessedBWCharacter->OwnGridCoordinate, BasicAttackTemplate);
			}
		}
	}
}

void ABWPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	TraceGridUnderCursor();
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton)) OnLeftClick();
	if (WasInputKeyJustPressed(EKeys::RightMouseButton)) OnRightClick();
}

void ABWPlayerController::OnRightClick() 
{ 
	if (SelectedCardIndex != -1 || bIsMovementMode || bIsBasicAttackMode) CancelSelection(); 
}

void ABWPlayerController::CancelSelection()
{
	SelectedCardIndex = -1;
	bIsMovementMode = false;
	bIsBasicAttackMode = false;
	if (HandWidget) HandWidget->SelectedCardIndex = -1;
	if (TargetGridManager) TargetGridManager->ClearAllRanges();
}

void ABWPlayerController::TraceGridUnderCursor()
{
	if (!TargetGridManager)
	{
		AActor* FoundGrid = UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass());
		if (FoundGrid) TargetGridManager = Cast<AGridManager>(FoundGrid);
		else return;
	}

	if (!TargetGridManager->IsGridReady())
	{
		DestroyPlacementPreview();
		CurrentTileUnderCursor = nullptr;
		return;
	}

	FHitResult HitResult;
	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(this);
	if (GetPawn()) TraceParams.AddIgnoredActor(GetPawn());
	if (PlacementPreviewActor) TraceParams.AddIgnoredActor(PlacementPreviewActor);

	if (GetHitResultUnderCursor(ECC_Visibility, false, HitResult))
	{
		AActor* HitActor = HitResult.GetActor();
		if (HitActor)
		{
			AGridTile* HitTile = Cast<AGridTile>(HitActor);
			if (HitTile)
			{
				CurrentTileUnderCursor = HitTile;
				
				// [추가] 마우스 아래에 캐릭터가 있는지 확인하여 포커스 업데이트
				ABWCharacter* CharOnTile = TargetGridManager->GetCharacterAtCoordinate(HitTile->GridCoordinate);
				if (CharOnTile != FocusedCharacter)
				{
					FocusedCharacter = CharOnTile;
					OnTargetCharacterHovered.Broadcast(FocusedCharacter);
				}

				if (CurrentTileUnderCursor != LastHoveredTile)
				{
					if (!bIsSkillTargetingMode && !bIsMovementMode && !bIsBasicAttackMode) TargetGridManager->DrawPlacementGuidelines();
					else TargetGridManager->ClearAllRanges();
					
					// [추가] 적 의도 타일 호버 시 툴팁 표시
					if (BattleHUDWidget)
					{
						bool bFoundIntent = false;
						
						// 1. 적 공격 의도 툴팁
						if (CurrentTileUnderCursor->CurrentState == ETileState::EnemyAttackRange)
						{
							TArray<AActor*> FoundActors;
							UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), FoundActors);
							for (AActor* Actor : FoundActors)
							{
								ABWCharacter* Enemy = Cast<ABWCharacter>(Actor);
								if (Enemy && Enemy->bIsEnemy && Enemy->CurrentHP > 0 && Enemy->LockedAttackTiles.Contains(CurrentTileUnderCursor->GridCoordinate))
								{
									int32 PIdx = Enemy->CurrentPatternIndex;
									if (PIdx < Enemy->CombatPatterns.Num())
									{
										for (const FBWEnemyPattern& Action : Enemy->CombatPatterns[PIdx].Actions)
										{
											if (Action.ActionType == EBWEnemyActionType::Attack)
											{
												BattleHUDWidget->ShowTooltip(Action.ActionName, Action.Description, Action.Value + Enemy->CurrentAttackBuff, Action.MoveDistance, Action.Range);
												bFoundIntent = true;
												break;
											}
										}
									}
									if (bFoundIntent) break;
								}
							}
						}
						// 2. 아군 배치 구역 툴팁
						else if (CurrentTileUnderCursor->CurrentState == ETileState::AllyZone)
						{
							if (BattleHUDWidget->AllyZone_Tooltip.bEnableTooltip)
							{
								BattleHUDWidget->ShowTooltip(BattleHUDWidget->AllyZone_Tooltip.Title, BattleHUDWidget->AllyZone_Tooltip.Description);
								bFoundIntent = true;
							}
						}
						// 3. 이동 가능 범위 툴팁
						else if (CurrentTileUnderCursor->CurrentState == ETileState::MoveRange)
						{
							if (BattleHUDWidget->MoveRange_Tooltip.bEnableTooltip)
							{
								int32 MovePts = PossessedBWCharacter ? PossessedBWCharacter->CurrentMovePoints : 0;
								BattleHUDWidget->ShowTooltip(BattleHUDWidget->MoveRange_Tooltip.Title, BattleHUDWidget->MoveRange_Tooltip.Description, MovePts);
								bFoundIntent = true;
							}
						}
						
						if (!bFoundIntent) BattleHUDWidget->HideTooltip();
					}

					if (bIsSkillTargetingMode && !bIsMovementMode && PossessedBWCharacter && PossessedBWCharacter->DeckComponent)
					{
						TArray<FBWCardData> HandCards = PossessedBWCharacter->DeckComponent->GetCurrentHand();
						if (SelectedCardIndex >= 0 && SelectedCardIndex < HandCards.Num())
						{
							TargetGridManager->DrawSkillPreview(PossessedBWCharacter->OwnGridCoordinate, CurrentTileUnderCursor->GridCoordinate, HandCards[SelectedCardIndex]);
						}
						// [복구] 스킬 선택 중에도 마우스 아래 타일은 노란색(Hovered)으로 표시
						CurrentTileUnderCursor->SetTileState(ETileState::Hovered);
					}
					else if (bIsBasicAttackMode && PossessedBWCharacter)
					{
						TargetGridManager->DrawSkillPreview(PossessedBWCharacter->OwnGridCoordinate, CurrentTileUnderCursor->GridCoordinate, BasicAttackTemplate);
						CurrentTileUnderCursor->SetTileState(ETileState::Hovered);
					}
					else if (bIsMovementMode && PossessedBWCharacter)
					{
						TargetGridManager->DrawMovementPreview(PossessedBWCharacter->OwnGridCoordinate, PossessedBWCharacter->CurrentMovePoints);
						if (CurrentTileUnderCursor->CurrentState == ETileState::MoveRange)
						{
							CurrentTileUnderCursor->SetTileState(ETileState::Hovered);
						}
					}
					
					UpdatePlacementPreview();
					LastHoveredTile = CurrentTileUnderCursor;
				}
				return;
			}
		}
	}
	
	if (LastHoveredTile)
	{
		if (!bIsSkillTargetingMode && !bIsMovementMode && !bIsBasicAttackMode) TargetGridManager->DrawPlacementGuidelines();
		else if (!bIsMovementMode && !bIsBasicAttackMode) TargetGridManager->ClearAllRanges();
		else if (bIsMovementMode && PossessedBWCharacter) TargetGridManager->DrawMovementPreview(PossessedBWCharacter->OwnGridCoordinate, 1);
		
		LastHoveredTile = nullptr;
		DestroyPlacementPreview();
	}
	CurrentTileUnderCursor = nullptr;
}

void ABWPlayerController::UpdatePlacementPreview()
{
	if (!bIsSkillTargetingMode && !bIsMovementMode && !bIsBasicAttackMode && CurrentTileUnderCursor && TargetGridManager->IsValidAllyPlacementArea(CurrentTileUnderCursor->GridCoordinate))
	{
		TSubclassOf<AActor> PreviewClass = PlacementPreviewMap.Contains(CharacterClassToSpawn) ? PlacementPreviewMap[CharacterClassToSpawn] : nullptr;
		
		if (PreviewClass)
		{
			if (PlacementPreviewActor && PlacementPreviewActor->GetClass() != PreviewClass) DestroyPlacementPreview();

			if (!PlacementPreviewActor)
				PlacementPreviewActor = GetWorld()->SpawnActor<AActor>(PreviewClass, CurrentTileUnderCursor->GetActorLocation(), FRotator::ZeroRotator);
			
			if (PlacementPreviewActor)
			{
				PlacementPreviewActor->SetActorLocation(CurrentTileUnderCursor->GetActorLocation() + FVector(0, 0, 20));
				PlacementPreviewActor->SetActorHiddenInGame(false);
			}
		}
	}
	else DestroyPlacementPreview();
}

void ABWPlayerController::DestroyPlacementPreview()
{
	if (PlacementPreviewActor) { PlacementPreviewActor->Destroy(); PlacementPreviewActor = nullptr; }
}

void ABWPlayerController::SelectCard(int32 Index)
{
	if (bIsMovementMode || bIsBasicAttackMode) CancelSelection();

	if (PossessedBWCharacter && PossessedBWCharacter->DeckComponent)
	{
		TArray<FBWCardData> HandCards = PossessedBWCharacter->DeckComponent->GetCurrentHand();
		if (HandCards.IsValidIndex(Index))
		{
			if (HandCards[Index].APCost > PossessedBWCharacter->DeckComponent->CurrentAP) return;
		}
	}
	SelectedCardIndex = Index;
	if (HandWidget) HandWidget->SelectedCardIndex = Index;
}

void ABWPlayerController::OnLeftClick()
{
	TraceGridUnderCursor();
	if (TargetGridManager && !TargetGridManager->IsGridReady()) return;

	if (!CurrentTileUnderCursor || !TargetGridManager) return;

	if (bIsMovementMode && PossessedBWCharacter && PossessedBWCharacter->DeckComponent)
	{
		if (CurrentTileUnderCursor->CurrentState == ETileState::MoveRange || CurrentTileUnderCursor->CurrentState == ETileState::Hovered)
		{
			if (TargetGridManager->GetCharacterAtCoordinate(CurrentTileUnderCursor->GridCoordinate) == nullptr)
			{
				int32 Dist = FMath::Abs(PossessedBWCharacter->OwnGridCoordinate.X - CurrentTileUnderCursor->GridCoordinate.X) + 
							 FMath::Abs(PossessedBWCharacter->OwnGridCoordinate.Y - CurrentTileUnderCursor->GridCoordinate.Y);

				if (Dist <= PossessedBWCharacter->CurrentMovePoints)
				{
					PossessedBWCharacter->CurrentMovePoints -= Dist;
					PossessedBWCharacter->OwnGridCoordinate = CurrentTileUnderCursor->GridCoordinate;
					PossessedBWCharacter->SetActorLocation(CurrentTileUnderCursor->GetActorLocation() + FVector(0, 0, 20));
					
					PossessedBWCharacter->DeckComponent->OnMovePointsChanged.Broadcast(PossessedBWCharacter->CurrentMovePoints);
					CancelSelection();
				}
			}
		}
		return;
	}

	if (bIsBasicAttackMode && PossessedBWCharacter && PossessedBWCharacter->DeckComponent)
	{
		if (PossessedBWCharacter->DeckComponent->bCanFreeAttack)
		{
			// 거리 체크 (기본 사거리 1)
			int32 Dist = FMath::Abs(PossessedBWCharacter->OwnGridCoordinate.X - CurrentTileUnderCursor->GridCoordinate.X) + 
						 FMath::Abs(PossessedBWCharacter->OwnGridCoordinate.Y - CurrentTileUnderCursor->GridCoordinate.Y);
			
			if (Dist <= BasicAttackTemplate.Range)
			{
				TargetGridManager->ExecuteSkill(PossessedBWCharacter->OwnGridCoordinate, CurrentTileUnderCursor->GridCoordinate, BasicAttackTemplate);
				PossessedBWCharacter->DeckComponent->bCanFreeAttack = false; // 공격권 소모
				CancelSelection();
			}
		}
		return;
	}

	if (!bIsSkillTargetingMode) 
	{
		if (CharacterClassToSpawn)
		{
			if (TargetGridManager->TryPlaceCharacter(CurrentTileUnderCursor->GridCoordinate, CharacterClassToSpawn))
			{
				DestroyPlacementPreview();
				TArray<AActor*> FoundActors;
				UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), FoundActors);
				if (FoundActors.Num() > 0)
				{
					PossessedBWCharacter = Cast<ABWCharacter>(FoundActors.Last());
					bIsSkillTargetingMode = true; 
					Possess(PossessedBWCharacter);
					if (HandWidget) { HandWidget->LinkToPlayerDeck(PossessedBWCharacter); HandWidget->RefreshHand(PossessedBWCharacter->DeckComponent->GetCurrentHand()); }
					if (BattleHUDWidget) BattleHUDWidget->InitializeBattleHUD(PossessedBWCharacter);
					if (PossessedBWCharacter->DeckComponent) PossessedBWCharacter->DeckComponent->StartTurn();
					
					// 적 의도 표시
					for (AActor* Actor : FoundActors)
					{
						ABWCharacter* Char = Cast<ABWCharacter>(Actor);
						if (Char && Char != PossessedBWCharacter) Char->VisualizeEnemyIntent();
					}

					OnCombatStarted.Broadcast();
				}
			}
		}
	}
	else if (SelectedCardIndex != -1 && PossessedBWCharacter && PossessedBWCharacter->DeckComponent)
	{
		FBWCardData UsedCardData;
		int32 IndexToUse = SelectedCardIndex;
		if (HandWidget) { HandWidget->PlayDiscardAnimation(IndexToUse); HandWidget->SelectedCardIndex = -1; }
		SelectedCardIndex = -1;
		if (PossessedBWCharacter->DeckComponent->UseCard(IndexToUse, UsedCardData))
		{
			TargetGridManager->ExecuteSkill(PossessedBWCharacter->OwnGridCoordinate, CurrentTileUnderCursor->GridCoordinate, UsedCardData);
			if (HandWidget) HandWidget->RefreshHand(PossessedBWCharacter->DeckComponent->GetCurrentHand());
		}
		else SelectCard(IndexToUse);
	}
}

void ABWPlayerController::RequestEndTurn()
{
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Player Turn Ended. Gathering Enemies for sequential execution..."));

	if (TargetGridManager)
	{
		TargetGridManager->ClearAllRanges();
	}

	EnemyTurnQueue.Empty();

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), FoundActors);

	for (AActor* Actor : FoundActors)
	{
		ABWCharacter* Char = Cast<ABWCharacter>(Actor);
		if (Char && Char->bIsEnemy && Char->CurrentHP > 0)
		{
			EnemyTurnQueue.Add(Char);
		}
	}

	// 순차 실행 시작
	ProcessNextEnemyTurn();
}

void ABWPlayerController::ProcessNextEnemyTurn()
{
	if (EnemyTurnQueue.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] All enemies acted. Delaying 0.5s before Player Turn..."));
		
		// [수정] 모든 적 행동 종료 후 0.5초 대기 후 플레이어 턴 시작
		FTimerHandle TurnResetTimer;
		GetWorldTimerManager().SetTimer(TurnResetTimer, [this]() {
			if (PossessedBWCharacter && PossessedBWCharacter->DeckComponent)
			{
				PossessedBWCharacter->DeckComponent->StartTurn();
			}

			TArray<AActor*> FoundActors;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), FoundActors);
			for (AActor* Actor : FoundActors)
			{
				ABWCharacter* Char = Cast<ABWCharacter>(Actor);
				if (Char && Char->bIsEnemy && Char->CurrentHP > 0)
				{
					Char->UpdateEnemyIntent();
					Char->VisualizeEnemyIntent();
				}
			}
		}, 0.5f, false);

		return;
	}

	ABWCharacter* NextEnemy = EnemyTurnQueue[0];
	EnemyTurnQueue.RemoveAt(0);

	if (NextEnemy && NextEnemy->IsValidLowLevel() && NextEnemy->CurrentHP > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[BWCombat] Enemy Acting: %s"), *NextEnemy->GetName());
		NextEnemy->ExecuteNextPattern();

		// 다음 적이 행동하기 전 대기 시간 (연출용)
		FTimerHandle NextEnemyTimer;
		GetWorldTimerManager().SetTimer(NextEnemyTimer, this, &ABWPlayerController::ProcessNextEnemyTurn, 1.2f, false);
	}
	else
	{
		// 유효하지 않은 적이면 즉시 다음 단계
		ProcessNextEnemyTurn();
	}
}

AGridTile* ABWPlayerController::GetTileUnderCursor() { return CurrentTileUnderCursor; }
