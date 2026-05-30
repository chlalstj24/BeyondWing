#include "BWPlayerController.h"
#include "GridManager.h"
#include "DeckComponent.h"
#include "BWCharacter.h"
#include "BWHandWidget.h"
#include "BeyondWing/UI/Public/BWBattleHUDWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

ABWPlayerController::ABWPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bIsSkillTargetingMode = false; 
	CurrentSkillRange = 3;        
	SelectedCardIndex = -1; 
	LastHoveredTile = nullptr;
	HandWidget = nullptr;
}

void ABWPlayerController::BeginPlay()
{
	Super::BeginPlay();
	bShowMouseCursor = true;

	AActor* FoundGrid = UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass());
	if (FoundGrid) TargetGridManager = Cast<AGridManager>(FoundGrid);
}

void ABWPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	TraceGridUnderCursor();

	if (WasInputKeyJustPressed(EKeys::LeftMouseButton)) OnLeftClick();
	if (WasInputKeyJustPressed(EKeys::RightMouseButton)) OnRightClick();
	
	if (bIsSkillTargetingMode && PossessedBWCharacter && PossessedBWCharacter->DeckComponent)
	{
		if (WasInputKeyJustPressed(EKeys::R))
		{
			PossessedBWCharacter->DeckComponent->StartTurn();
			TargetGridManager->ClearAllRanges();
		}
	}
}

void ABWPlayerController::OnRightClick()
{
	if (SelectedCardIndex != -1) CancelSelection();
}

void ABWPlayerController::CancelSelection()
{
	SelectedCardIndex = -1;
	if (HandWidget) HandWidget->SelectedCardIndex = -1;
	if (TargetGridManager) TargetGridManager->ClearAllRanges();
	UE_LOG(LogTemp, Warning, TEXT("[BWPlayerController] Selection Cancelled."));
}

void ABWPlayerController::TraceGridUnderCursor()
{
	if (!TargetGridManager) return;
	FHitResult HitResult;
	if (GetHitResultUnderCursor(ECC_Visibility, false, HitResult) && HitResult.GetActor())
	{
		AGridTile* HitTile = Cast<AGridTile>(HitResult.GetActor());
		if (HitTile)
		{
			CurrentTileUnderCursor = HitTile;
			if (CurrentTileUnderCursor != LastHoveredTile)
			{
				if (!bIsSkillTargetingMode) TargetGridManager->DrawPlacementGuidelines();
				else TargetGridManager->ClearAllRanges();

				if (bIsSkillTargetingMode && PossessedBWCharacter && PossessedBWCharacter->DeckComponent)
				{
					TArray<FCardData> HandCards = PossessedBWCharacter->DeckComponent->GetCurrentHand();
					if (SelectedCardIndex >= 0 && SelectedCardIndex < HandCards.Num())
					{
						TargetGridManager->DrawSkillPreview(PossessedBWCharacter->OwnGridCoordinate, CurrentTileUnderCursor->GridCoordinate, HandCards[SelectedCardIndex]);
						CurrentTileUnderCursor->SetTileState(ETileState::Hovered);
					}
				}
				LastHoveredTile = CurrentTileUnderCursor;
			}
			return;
		}
	}
	if (LastHoveredTile)
	{
		if (!bIsSkillTargetingMode) TargetGridManager->DrawPlacementGuidelines();
		else TargetGridManager->ClearAllRanges();
		LastHoveredTile = nullptr;
	}
	CurrentTileUnderCursor = nullptr;
}

void ABWPlayerController::SelectCard(int32 Index)
{
	if (PossessedBWCharacter && PossessedBWCharacter->DeckComponent)
	{
		TArray<FCardData> HandCards = PossessedBWCharacter->DeckComponent->GetCurrentHand();
		if (HandCards.IsValidIndex(Index))
		{
			int32 Cost = HandCards[Index].APCost;
			int32 CurrentAP = PossessedBWCharacter->DeckComponent->CurrentAP;

			if (Cost > CurrentAP)
			{
				if (!InsufficientAPSound.IsNull())
				{
					USoundBase* ErrorSound = InsufficientAPSound.LoadSynchronous();
					if (ErrorSound) UGameplayStatics::PlaySound2D(GetWorld(), ErrorSound);
				}
				return;
			}
		}
	}

	SelectedCardIndex = Index;
	if (HandWidget) HandWidget->SelectedCardIndex = Index;
	UE_LOG(LogTemp, Warning, TEXT("[BWPlayerController] Card %d Selected."), Index);
}

void ABWPlayerController::OnLeftClick()
{
	if (SelectedCardIndex != -1 && CurrentTileUnderCursor == nullptr)
	{
		CancelSelection();
		return;
	}

	if (!CurrentTileUnderCursor || !TargetGridManager) return;

	if (!bIsSkillTargetingMode) 
	{
		if (CharacterClassToSpawn && TargetGridManager->TryPlaceCharacter(CurrentTileUnderCursor->GridCoordinate, CharacterClassToSpawn))
		{
			TArray<AActor*> FoundActors;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), FoundActors);
			if (FoundActors.Num() > 0)
			{
				PossessedBWCharacter = Cast<ABWCharacter>(FoundActors.Last());
				bIsSkillTargetingMode = true; 
				TargetGridManager->ClearAllRanges();
				if (PossessedBWCharacter->DeckComponent)
				{
					// [핵심] 모든 HUD를 찾아 캐릭터 정보를 강제로 주입
					TArray<UUserWidget*> AllWidgets;
					UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), AllWidgets, UUserWidget::StaticClass());
					for (UUserWidget* Widget : AllWidgets)
					{
						if (UBWHandWidget* HandW = Cast<UBWHandWidget>(Widget))
						{
							SetHandWidget(HandW);
							HandW->LinkToPlayerDeck(PossessedBWCharacter);
						}
						else if (UWidget* FoundHand = Widget->GetWidgetFromName(TEXT("WBP_Hand")))
						{
							if (UBWHandWidget* HandW2 = Cast<UBWHandWidget>(FoundHand))
							{
								SetHandWidget(HandW2);
								HandW2->LinkToPlayerDeck(PossessedBWCharacter);
							}
						}

						if (UBWBattleHUDWidget* BattleW = Cast<UBWBattleHUDWidget>(Widget))
						{
							BattleW->InitializeBattleHUD(PossessedBWCharacter);
						}
						else if (UWidget* FoundBattle = Widget->GetWidgetFromName(TEXT("WBP_BattleHUD")))
						{
							if (UBWBattleHUDWidget* BattleW2 = Cast<UBWBattleHUDWidget>(FoundBattle))
							{
								BattleW2->InitializeBattleHUD(PossessedBWCharacter);
							}
						}
					}

					PossessedBWCharacter->DeckComponent->StartTurn();
					OnCombatStarted.Broadcast();
				}
			}
		}
	}
	else if (SelectedCardIndex != -1 && PossessedBWCharacter && PossessedBWCharacter->DeckComponent)
	{
		FCardData UsedCardData;
		int32 IndexToUse = SelectedCardIndex;
		
		if (HandWidget) 
		{
			HandWidget->PlayDiscardAnimation(IndexToUse);
			HandWidget->SelectedCardIndex = -1;
		}
		SelectedCardIndex = -1;

		if (PossessedBWCharacter->DeckComponent->UseCard(IndexToUse, UsedCardData))
		{
			if (!UsedCardData.CardSound.IsNull())
			{
				USoundBase* SoundToPlay = UsedCardData.CardSound.LoadSynchronous();
				if (SoundToPlay) UGameplayStatics::PlaySound2D(GetWorld(), SoundToPlay);
			}
			TargetGridManager->ExecuteSkill(PossessedBWCharacter->OwnGridCoordinate, CurrentTileUnderCursor->GridCoordinate, UsedCardData);
			
			if (HandWidget) HandWidget->RefreshHand(PossessedBWCharacter->DeckComponent->GetCurrentHand());
		}
		else
		{
			SelectCard(IndexToUse);
		}
	}
}

AGridTile* ABWPlayerController::GetTileUnderCursor() { return CurrentTileUnderCursor; }
void ABWPlayerController::SetupInputComponent() { Super::SetupInputComponent(); }
