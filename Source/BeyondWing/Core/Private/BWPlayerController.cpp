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
#include "Particles/ParticleSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/PostProcessVolume.h"

namespace
{
	// 카드·적 행동 이펙트: Niagara가 지정돼 있으면 그것을, 아니면 예전 파티클을 쓴다
	void BWSpawnFX(UWorld* World, const TSoftObjectPtr<UNiagaraSystem>& Niagara, const TSoftObjectPtr<UParticleSystem>& Legacy, const FVector& Location)
	{
		if (!World) return;
		if (UNiagaraSystem* System = Niagara.LoadSynchronous())
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Location);
		}
		else if (UParticleSystem* Particle = Legacy.LoadSynchronous())
		{
			UGameplayStatics::SpawnEmitterAtLocation(World, Particle, Location);
		}
	}
}
#include "Sound/SoundBase.h"
#include "BWDuelRules.h"
#include "BeyondWing/UI/Public/BWAugmentSelectWidget.h"
#include "BeyondWing/UI/Public/BWGuardChoiceWidget.h"
#include "BeyondWing/UI/Public/BWGeleonChoiceWidget.h"
#include "BeyondWing/UI/Public/BWTurnBannerWidget.h"
#include "BeyondWing/Combat/Public/BWAugmentRules.h"

ABWPlayerController::ABWPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bAutoManageActiveCameraTarget = false; // 말을 조종해도 전투 카메라를 그대로 쓴다
	bIsSkillTargetingMode = false;
	bIsDuelTargetingMode = false;
	CurrentSkillRange = 3;        
	SelectedCardIndex = -1; 
	LastHoveredTile = nullptr;
	HandWidget = nullptr;
	PlacementPreviewActor = nullptr;
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
	BattleCamera = FoundCamera;
	if (FoundCamera) SetViewTargetWithBlend(FoundCamera);

	// [수정] 게임 모드가 기본으로 만든 빈 폰(구체 메시)이 판 가운데에 보이지 않게 숨긴다
	if (APawn* StartPawn = GetPawn(); StartPawn && !Cast<ABWCharacter>(StartPawn))
	{
		StartPawn->SetActorHiddenInGame(true);
		StartPawn->SetActorEnableCollision(false);
	}


	AActor* FoundGrid = UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass());
	if (FoundGrid) TargetGridManager = Cast<AGridManager>(FoundGrid);
}

void ABWPlayerController::SetupInputComponent() { Super::SetupInputComponent(); }

void ABWPlayerController::EnterMovementMode()
{
	if (bActionInProgress || bEnemyPhase) return;
	if (PossessedBWCharacter && PossessedBWCharacter->RemainingMove > 0)
	{
		CancelSelection();
		bIsMovementMode = true;
		if (TargetGridManager) TargetGridManager->DrawMovementPreview(PossessedBWCharacter->OwnGridCoordinate, PossessedBWCharacter->RemainingMove);
		Prompt(FString::Printf(TEXT("파란 칸으로 이동 (남은 %d칸). 적을 누르면 맞닿는 칸까지 다가갑니다"), PossessedBWCharacter->RemainingMove), 0.0f);
	}
	else if (PossessedBWCharacter)
	{
		Prompt(TEXT("이번 턴 이동 칸을 다 썼습니다"));
	}
}

void ABWPlayerController::EnterBasicAttackMode()
{
	if (bActionInProgress || bEnemyPhase) return;
	if (!CanUseBasicStrike())
	{
		if (PossessedBWCharacter && PossessedBWCharacter->IsStunned()) PlayCardUnusableFeedback(TEXT("기절 상태라 합을 걸 수 없습니다."));
		else if (PossessedBWCharacter && PossessedBWCharacter->DeckComponent && !PossessedBWCharacter->DeckComponent->CanOpenDuel()) PlayCardUnusableFeedback(TEXT("이번 턴에 이미 합을 열었습니다."));
		else PlayCardUnusableFeedback(FString::Printf(TEXT("기본 공격 사거리(%d칸) 안에 적이 없습니다. 먼저 적에게 다가가세요"), GetReachForHandIndex(INDEX_NONE)));
		return;
	}

	// [수정] 가까운 적을 자동으로 고르지 않는다: 사거리 안의 적을 강조하고 플레이어가 누른다 (우클릭 취소)
	EnterDuelTargeting(INDEX_NONE);
	Prompt(TEXT("기본 공격을 할 적을 누르세요 (금색 칸 · 우클릭 취소)"), 0.0f);
}

void ABWPlayerController::EnterDuelTargeting(int32 HandIndex)
{
	CancelSelection();
	bIsDuelTargetingMode = true;
	DuelCardIndex = HandIndex;
	SelectedCardIndex = HandIndex;
	if (HandWidget) HandWidget->SelectedCardIndex = HandIndex;
	RefreshDuelTargetHighlight();
}

void ABWPlayerController::RefreshDuelTargetHighlight()
{
	if (!TargetGridManager) return;
	TargetGridManager->ClearAllRanges();
	// [수정] 이 공격의 사거리 칸을 깔고, 그 안의 적을 대상으로 강조
	const int32 Reach = GetReachForHandIndex(DuelCardIndex);
	if (PossessedBWCharacter && Reach >= 1)
	{
		const FIntPoint Me = PossessedBWCharacter->OwnGridCoordinate;
		for (int32 dx = -Reach; dx <= Reach; ++dx)
		{
			for (int32 dy = -Reach; dy <= Reach; ++dy)
			{
				const int32 Dist = FMath::Abs(dx) + FMath::Abs(dy);
				if (Dist == 0 || Dist > Reach) continue;
				AGridTile* Tile = TargetGridManager->GetTileAtCoordinate(Me + FIntPoint(dx, dy));
				if (Tile && !TargetGridManager->GetCharacterAtCoordinate(Tile->GridCoordinate) && Tile->CurrentState != ETileState::Wall) Tile->SetTileState(ETileState::AttackRange);
			}
		}
	}
	for (ABWCharacter* Enemy : GetEnemiesInReach(Reach))
	{
		if (AGridTile* Tile = TargetGridManager->GetTileAtCoordinate(Enemy->OwnGridCoordinate)) Tile->SetTileState(ETileState::AttackTarget);
	}
}

TArray<ABWCharacter*> ABWPlayerController::GetEnemiesInReach(int32 Reach) const
{
	TArray<ABWCharacter*> Result;
	if (!PossessedBWCharacter || !TargetGridManager) return Result;
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), Found);
	for (AActor* Actor : Found)
	{
		ABWCharacter* Other = Cast<ABWCharacter>(Actor);
		if (IsEnemyInReach(Other, Reach)) Result.Add(Other);
	}
	// 가까운 적부터
	const FIntPoint Me = PossessedBWCharacter->OwnGridCoordinate;
	Result.Sort([Me](const ABWCharacter& A, const ABWCharacter& B)
	{
		const FIntPoint DA = A.OwnGridCoordinate - Me, DB = B.OwnGridCoordinate - Me;
		return FMath::Abs(DA.X) + FMath::Abs(DA.Y) < FMath::Abs(DB.X) + FMath::Abs(DB.Y);
	});
	return Result;
}

bool ABWPlayerController::IsEnemyInReach(const ABWCharacter* Enemy, int32 Reach) const
{
	if (!Enemy || !PossessedBWCharacter || !Enemy->bIsEnemy || Enemy->CurrentHP <= 0 || Enemy->IsActorBeingDestroyed()) return false;
	const FIntPoint Delta = Enemy->OwnGridCoordinate - PossessedBWCharacter->OwnGridCoordinate;
	const int32 Dist = FMath::Abs(Delta.X) + FMath::Abs(Delta.Y);
	return Dist >= 1 && Dist <= FMath::Max(1, Reach);
}

int32 ABWPlayerController::GetReachForHandIndex(int32 HandIndex) const
{
	if (!PossessedBWCharacter) return 1;
	const UDeckComponent* Deck = PossessedBWCharacter->DeckComponent;
	if (HandIndex != INDEX_NONE && Deck && Deck->Hand.IsValidIndex(HandIndex) && Deck->Hand[HandIndex].CardKind == ECardKind::Attack)
	{
		return PossessedBWCharacter->GetAttackReach(&Deck->Hand[HandIndex]);
	}
	return PossessedBWCharacter->GetAttackReach(nullptr);
}

void ABWPlayerController::UpdateReachPreview()
{
	// 아무 모드도 아닐 때 공격카드를 집거나 올리면 그 카드의 사거리 칸을 보여 준다 (사거리 2 이상만)
	int32 Index = -1;
	if (bShowReachPreview && HandWidget && PossessedBWCharacter && !bActionInProgress && !bEnemyPhase && !bIsMovementMode && !bIsDuelTargetingMode)
	{
		const int32 Focus = HandWidget->GetFocusedCardIndex();
		const UDeckComponent* Deck = PossessedBWCharacter->DeckComponent;
		if (Deck && Deck->Hand.IsValidIndex(Focus) && Deck->Hand[Focus].CardKind == ECardKind::Attack && GetReachForHandIndex(Focus) > 1) Index = Focus;
	}
	if (Index == ReachPreviewIndex) return;
	ReachPreviewIndex = Index;

	if (Index >= 0)
	{
		const int32 SavedIndex = DuelCardIndex;
		DuelCardIndex = Index;
		RefreshDuelTargetHighlight();
		DuelCardIndex = SavedIndex;
		bReachPreviewShown = true;
	}
	else if (bReachPreviewShown)
	{
		bReachPreviewShown = false;
		if (TargetGridManager && !bIsMovementMode && !bIsDuelTargetingMode) TargetGridManager->ClearAllRanges();
	}
}

TArray<ABWCharacter*> ABWPlayerController::GetAdjacentEnemies()
{
	TArray<ABWCharacter*> Result;
	if (!PossessedBWCharacter || !TargetGridManager) return Result;

	static const FIntPoint Dirs[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	for (const FIntPoint& Dir : Dirs)
	{
		ABWCharacter* Other = TargetGridManager->GetCharacterAtCoordinate(PossessedBWCharacter->OwnGridCoordinate + Dir);
		if (Other && Other->bIsEnemy && Other->CurrentHP > 0 && !Other->IsActorBeingDestroyed()) Result.Add(Other);
	}
	return Result;
}

bool ABWPlayerController::IsPendingDuelTargetAdjacent() const
{
	if (!PendingDuelTarget || !PossessedBWCharacter || PendingDuelTarget->IsActorBeingDestroyed() || PendingDuelTarget->CurrentHP <= 0) return false;
	const FIntPoint Delta = PendingDuelTarget->OwnGridCoordinate - PossessedBWCharacter->OwnGridCoordinate;
	return FMath::Abs(Delta.X) + FMath::Abs(Delta.Y) == 1;
}

bool ABWPlayerController::CanUseBasicStrike()
{
	if (!PossessedBWCharacter || !PossessedBWCharacter->DeckComponent) return false;
	UDeckComponent* Deck = PossessedBWCharacter->DeckComponent;
	// [수정] 기본 공격은 손패와 상관없이 언제나 (공격카드는 이 공격을 강화한다)
	return Deck->CanOpenDuel() && !PossessedBWCharacter->IsStunned() && GetEnemiesInReach(GetReachForHandIndex(INDEX_NONE)).Num() > 0;
}

bool ABWPlayerController::HasUsableAttackCardInHand() const
{
	if (!PossessedBWCharacter || !PossessedBWCharacter->DeckComponent) return false;
	for (const FBWCardData& Card : PossessedBWCharacter->DeckComponent->Hand)
	{
		if (Card.CardKind == ECardKind::Attack && UBWDimensionRules::CanUseCard(Battlefield, Card)) return true;
	}
	return false;
}

void ABWPlayerController::SetBattlefield(EDimensionType Dimension, bool bOverloaded)
{
	Battlefield = UBWDimensionRules::MakeBattlefield(Dimension, bOverloaded);
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Battlefield: %s%s"), *UEnum::GetValueAsString(Dimension), Battlefield.bOverloaded ? TEXT(" (overloaded)") : TEXT(""));
	ApplyDimensionAtmosphere();
}

void ABWPlayerController::ApplyDimensionAtmosphere()
{
	// 차원별 분위기 (지도 기획서 6장 성격): 파트로바스=마법(보랏빛 신비), 베렌드=상남자(노을빛 흙먼지),
	// 겔레온=기술(차갑고 채도 낮은 금속빛), 타이가=암살(어두운 밤). 차원이 없으면 레벨 기본값 그대로
	struct FAtmosphere
	{
		FLinearColor Sun; float SunIntensity;
		FLinearColor Sky; float SkyIntensity;
		FLinearColor Fog; float FogDensity;
		FLinearColor Tint; float Saturation; float Contrast; float Vignette; float Bloom;
	};
	FAtmosphere A;
	switch (Battlefield.Dimension)
	{
	case EDimensionType::Patrovas:
		A = { FLinearColor(0.75f, 0.68f, 1.0f), 4.5f, FLinearColor(0.7f, 0.6f, 1.0f), 1.1f, FLinearColor(0.35f, 0.22f, 0.6f), 0.04f, FLinearColor(0.92f, 0.86f, 1.08f), 1.15f, 1.05f, 0.45f, 1.2f };
		break;
	case EDimensionType::Berend:
		A = { FLinearColor(1.0f, 0.7f, 0.42f), 7.5f, FLinearColor(1.0f, 0.8f, 0.6f), 1.0f, FLinearColor(0.55f, 0.36f, 0.2f), 0.03f, FLinearColor(1.08f, 0.97f, 0.84f), 1.05f, 1.1f, 0.4f, 0.8f };
		break;
	case EDimensionType::Geleon:
		A = { FLinearColor(0.8f, 0.92f, 1.0f), 6.0f, FLinearColor(0.7f, 0.85f, 0.95f), 0.9f, FLinearColor(0.28f, 0.36f, 0.42f), 0.025f, FLinearColor(0.92f, 1.0f, 1.04f), 0.72f, 1.2f, 0.35f, 0.6f };
		break;
	case EDimensionType::Taiga:
		A = { FLinearColor(0.45f, 0.55f, 0.9f), 1.8f, FLinearColor(0.4f, 0.45f, 0.8f), 0.35f, FLinearColor(0.05f, 0.06f, 0.14f), 0.05f, FLinearColor(0.82f, 0.86f, 1.05f), 0.85f, 1.15f, 0.7f, 1.3f };
		break;
	default:
		if (AtmosphereVolume) AtmosphereVolume->bEnabled = false;
		return;
	}
	if (Battlefield.bOverloaded)
	{
		// 과부하: 붉은 기운이 돌고 가장자리가 더 어둡다
		A.Tint *= FLinearColor(1.1f, 0.86f, 0.84f);
		A.Fog = FMath::Lerp(A.Fog, FLinearColor(0.45f, 0.05f, 0.05f), 0.5f);
		A.Vignette += 0.15f;
		A.Saturation += 0.1f;
	}

	UWorld* World = GetWorld();
	if (!World) return;
	if (ADirectionalLight* Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(World, ADirectionalLight::StaticClass())))
	{
		if (ULightComponent* Light = Sun->GetLightComponent())
		{
			Light->SetLightColor(A.Sun);
			Light->SetIntensity(A.SunIntensity);
		}
	}
	if (ASkyLight* Sky = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(World, ASkyLight::StaticClass())))
	{
		if (USkyLightComponent* SkyComp = Sky->GetLightComponent())
		{
			SkyComp->SetLightColor(A.Sky);
			SkyComp->SetIntensity(A.SkyIntensity);
			SkyComp->RecaptureSky();
		}
	}
	if (AExponentialHeightFog* Fog = Cast<AExponentialHeightFog>(UGameplayStatics::GetActorOfClass(World, AExponentialHeightFog::StaticClass())))
	{
		if (UExponentialHeightFogComponent* FogComp = Fog->GetComponent())
		{
			FogComp->SetFogInscatteringColor(A.Fog);
			FogComp->SetFogDensity(A.FogDensity);
		}
	}

	// 화면 색감은 레벨 전체에 걸리는 후처리 볼륨 하나로 (없으면 만든다)
	if (!AtmosphereVolume)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AtmosphereVolume = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, Params);
	}
	if (AtmosphereVolume)
	{
		AtmosphereVolume->bUnbound = true;
		AtmosphereVolume->bEnabled = true;
		AtmosphereVolume->Priority = 10.0f;
		FPostProcessSettings& S = AtmosphereVolume->Settings;
		S.bOverride_SceneColorTint = true;   S.SceneColorTint = A.Tint;
		S.bOverride_ColorSaturation = true;  S.ColorSaturation = FVector4(A.Saturation, A.Saturation, A.Saturation, 1.0f);
		S.bOverride_ColorContrast = true;    S.ColorContrast = FVector4(A.Contrast, A.Contrast, A.Contrast, 1.0f);
		S.bOverride_VignetteIntensity = true; S.VignetteIntensity = A.Vignette;
		S.bOverride_BloomIntensity = true;   S.BloomIntensity = A.Bloom;
	}
}

void ABWPlayerController::InitBattlefieldFromGameInstance()
{
	// 지금 서 있는 차원 줄의 기믹. 연속 체류 4턴 이상이면 과부하 (지도 기획서 7-1)
	EDimensionType Dimension = EDimensionType::None;
	bool bOverloaded = false;
	if (UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance()))
	{
		if (GI->CurrentDimIdx >= 0 && GI->CurrentDimIdx < 4) Dimension = (EDimensionType)(GI->CurrentDimIdx + 1);
		int32 Consecutive = 0, Neglected = 0;
		GI->GetCountryStats(Dimension, Consecutive, Neglected);
		bOverloaded = Consecutive >= 4;
	}
	SetBattlefield(Dimension, bOverloaded);
}

bool ABWPlayerController::ChooseGeleonMode(bool bUseEffects)
{
	if (Battlefield.Dimension != EDimensionType::Geleon || Battlefield.bGeleonKindLocked) return false;
	Battlefield.bGeleonUseEffects = bUseEffects;
	Battlefield.GeleonAttackCardName.Reset();
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Geleon: %s"), bUseEffects ? TEXT("effect cards + basic strike + first attack card only") : TEXT("no effect cards, all attack cards"));
	return true;
}

void ABWPlayerController::OpenGeleonChoice()
{
	TSubclassOf<UBWGeleonChoiceWidget> WidgetClass = GeleonChoiceWidgetClass ? GeleonChoiceWidgetClass : TSubclassOf<UBWGeleonChoiceWidget>(UBWGeleonChoiceWidget::StaticClass());
	UBWGeleonChoiceWidget* Widget = CreateWidget<UBWGeleonChoiceWidget>(this, WidgetClass);
	if (!Widget)
	{
		BeginRound(); // 화면을 못 띄우면 기본값(효과카드 안 씀)으로 진행
		return;
	}

	// 버튼에 덱 전체(뽑을 더미·손패·버린 더미)의 종류별 장수를 보여 준다
	int32 AttackCount = 0, EffectCount = 0;
	if (PossessedBWCharacter && PossessedBWCharacter->DeckComponent)
	{
		const UDeckComponent* Deck = PossessedBWCharacter->DeckComponent;
		for (const TArray<FBWCardData>* Pile : { &Deck->Deck, &Deck->Hand, &Deck->Discard })
		{
			for (const FBWCardData& Card : *Pile) ++(Card.CardKind == ECardKind::Effect ? EffectCount : AttackCount);
		}
	}
	Widget->SetDeckCounts(AttackCount, EffectCount);
	Widget->AddToViewport(600);

	bAwaitingGeleonChoice = true;
	bActionInProgress = true; // 고르기 전에는 칸·카드 입력을 받지 않는다
	Prompt(TEXT("겔레온 - 이번 전투에 효과카드를 쓸지 고르세요"), 0.0f, EBWPromptStyle::Highlight);
}

bool ABWPlayerController::ConfirmGeleonMode(bool bUseEffects)
{
	if (!bAwaitingGeleonChoice || !ChooseGeleonMode(bUseEffects)) return false;
	Battlefield.bGeleonKindLocked = true;
	bAwaitingGeleonChoice = false;

	// 버튼이 아닌 곳(블루프린트, 단축키)에서 골라도 선택 화면을 닫는다
	TArray<UUserWidget*> ChoiceWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, ChoiceWidgets, UBWGeleonChoiceWidget::StaticClass(), false);
	for (UUserWidget* Widget : ChoiceWidgets) Widget->RemoveFromParent();
	bActionInProgress = false;
	BeginRound();
	return true;
}

void ABWPlayerController::ResolvePushAfterDuel(ABWCharacter* Pusher, ABWCharacter* Target, bool bPusherIsPlayer)
{
	if (!Pusher || !Target || !TargetGridManager || Target->CurrentHP <= 0 || Target->IsActorBeingDestroyed()) return;

	// 미는 쪽에서 맞은 쪽으로 1칸
	const FIntPoint Dir = Target->OwnGridCoordinate - Pusher->OwnGridCoordinate;
	bool bHitWall = false;
	if (TargetGridManager->TryPushCharacter(Target, Dir, bHitWall))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Pushed to %s"), *Target->OwnGridCoordinate.ToString());
		return;
	}
	if (!bHitWall) return;

	// 11-2 베렌드: 벽에 닿으면 추가 피해 + 기절 (과부하면 적도 나를 벽에 밀쳐 추가 피해, 기절은 없음)
	bool bExtraDamage = false, bStun = false;
	UBWDimensionRules::GetWallSlamRule(Battlefield, bPusherIsPlayer, bExtraDamage, bStun);
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Wall slam (extra damage %d, stun %d)"), bExtraDamage ? 1 : 0, bStun ? 1 : 0);
	if (bExtraDamage) Target->TakeHit(UBWDimensionRules::BerendWallDamage, false);
	if (bStun && Target->CurrentHP > 0 && !Target->IsActorBeingDestroyed()) Target->ApplyStatus(EStatusEffect::Stun, UBWDimensionRules::BerendStunTurns, 0);
}

void ABWPlayerController::PlayCardUnusableFeedback(const FString& Reason)
{
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s"), *Reason);
	Prompt(Reason, 3.0f, EBWPromptStyle::Warning);
	if (CardUnusableSound.LoadSynchronous()) UGameplayStatics::PlaySound2D(GetWorld(), CardUnusableSound.Get());
}

bool ABWPlayerController::OpenDuel(ABWCharacter* Enemy, int32 HandIndex)
{
	ABWCharacter* PlayerChar = PossessedBWCharacter;
	UDeckComponent* Deck = PlayerChar ? PlayerChar->DeckComponent : nullptr;
	if (!Deck || !TargetGridManager || !Enemy || !Enemy->bIsEnemy || Enemy->CurrentHP <= 0 || Enemy->IsActorBeingDestroyed()) return false;

	// 기절하면 이번 턴에 합을 걸지 못한다
	if (PlayerChar->IsStunned())
	{
		PlayCardUnusableFeedback(TEXT("기절 상태라 합을 걸 수 없습니다."));
		return false;
	}

	// 내가 선공으로 여는 합은 한 턴에 한 번
	if (!Deck->CanOpenDuel())
	{
		PlayCardUnusableFeedback(TEXT("이번 턴에 이미 합을 열었습니다."));
		return false;
	}

	// [수정] 사거리(상하좌우 칸 수) 안의 적만. 기본 공격은 보통 1칸(맞닿은 적), 사거리가 긴 공격카드로 강화하면 멀리서도 건다
	const int32 Reach = GetReachForHandIndex(HandIndex);
	if (!IsEnemyInReach(Enemy, Reach))
	{
		PlayCardUnusableFeedback(Reach <= 1 ? FString(TEXT("맞닿은 적에게만 합을 걸 수 있습니다."))
			: FString::Printf(TEXT("사거리 %d칸 밖의 적입니다."), Reach));
		return false;
	}

	FBWCardData AttackCard;
	if (HandIndex == INDEX_NONE)
	{
		// [수정] 기본 공격은 언제나 쓸 수 있다
		AttackCard = PlayerChar->GetBasicAttackCard();
	}
	else
	{
		if (!Deck->Hand.IsValidIndex(HandIndex) || Deck->Hand[HandIndex].CardKind != ECardKind::Attack) return false;
		if (!UBWDimensionRules::CanUseCard(Battlefield, Deck->Hand[HandIndex]))
		{
			PlayCardUnusableFeedback(FString::Printf(TEXT("겔레온: 이번 전투에 강화할 수 있는 공격카드는 '%s'뿐입니다. 기본 공격은 언제나 쓸 수 있습니다."), *Battlefield.GeleonAttackCardName));
			return false;
		}
		const EBWCardUseResult Check = Deck->CanUseCard(HandIndex, PlayerChar->CurrentMP);
		if (Check != EBWCardUseResult::Ok)
		{
			PlayCardUnusableFeedback(FString::Printf(TEXT("카드를 쓸 수 없습니다: %s"), *UEnum::GetValueAsString(Check)));
			return false;
		}
		if (HandWidget) HandWidget->PlayDiscardAnimation(HandIndex);
		Deck->UseCard(HandIndex, AttackCard);
		// 겔레온 효과카드 사용 모드: 이 카드가 이번 전투의 1종. 정해지는 순간 손패의 다른 공격카드를 흐리게 다시 그린다
		const bool bWasUnset = Battlefield.GeleonAttackCardName.IsEmpty();
		UBWDimensionRules::NoteAttackCardUsed(Battlefield, AttackCard);
		if (bWasUnset && !Battlefield.GeleonAttackCardName.IsEmpty() && HandWidget) HandWidget->RefreshHand(Deck->GetCurrentHand());
	}
	Deck->NotifyDuelOpened();
	Battlefield.bGeleonKindLocked = true;
	PendingDuelTarget = nullptr;
	CancelSelection();

	// [수정] 사용자 결정: 합을 걸면 이번 턴 남은 이동은 모두 쓴다 (접촉만으로는 이동이 끝나지 않음)
	PlayerChar->RemainingMove = 0;

	// 연출 순서: 합 화면(숫자 굴림) → 이긴 쪽이 찌르는 순간 피해 → 제자리로 돌아온 뒤 다시 조작
	bActionInProgress = true;
	PlayPlayerDuel(PlayerChar, Enemy, AttackCard, HandIndex != INDEX_NONE, [this]()
	{
		bActionInProgress = false;
		if (TargetGridManager) TargetGridManager->ClearAllRanges();
	});
	return true;
}

FString ABWPlayerController::DescribePlayerDefense(const ABWCharacter* PlayerChar) const
{
	if (!PlayerChar) return TEXT("방어");
	switch (PlayerChar->Defense.Type)
	{
	case EDefenseType::Evade:       return FString::Printf(TEXT("회피 %d%%"), FMath::RoundToInt(FMath::Min(PlayerChar->Defense.EvadeChance, UBWDuelRules::MaxEvadeChance) * 100.0f));
	case EDefenseType::MagicShield: return FString::Printf(TEXT("마나 가드 (MP %d)"), PlayerChar->CurrentMP);
	default:                        return FString::Printf(TEXT("방어 %d"), PlayerChar->Defense.FlatReduction);
	}
}

void ABWPlayerController::ShowDuel(const FBWDuelPresentation& Data, TFunction<void()> OnFinished)
{
	if (!DuelWidget)
	{
		TSubclassOf<UBWDuelWidget> WidgetClass = DuelWidgetClass ? DuelWidgetClass : TSubclassOf<UBWDuelWidget>(UBWDuelWidget::StaticClass());
		DuelWidget = CreateWidget<UBWDuelWidget>(this, WidgetClass);
	}
	if (!DuelWidget)
	{
		if (OnFinished) OnFinished();
		return;
	}
	DuelWidget->PlayDuel(Data, MoveTemp(OnFinished));
}

void ABWPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 연출 도중 레벨이 바뀌어도 시간 배율과 카메라가 원래대로 돌아오게
	if (HitStopTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(HitStopTicker);
		HitStopTicker.Reset();
		UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
	}
	if (ShakeTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ShakeTicker);
		ShakeTicker.Reset();
		if (BattleCamera) BattleCamera->SetActorLocation(ShakeHome);
	}
	Super::EndPlay(EndPlayReason);
}

void ABWPlayerController::PlayImpactFeedback(ABWCharacter* Attacker, ABWCharacter* Target, const FBWDefenseResult& Defended, int32 HPLost, bool bCritical)
{
	if (!Target) return;
	const FVector From = Attacker ? Attacker->GetActorLocation() : Target->GetActorLocation() - Target->GetActorForwardVector() * 100.0f;

	// 회피: 옆으로 비키기만 하고 멈칫·흔들림은 없다
	if (Defended.bEvaded)
	{
		Target->PlayDodge(From);
		return;
	}
	if (Defended.DamageAfterDefense <= 0 && HPLost <= 0) return;

	Target->PlayHitReaction(From, bCritical);
	StartHitStop(bCritical ? CritHitStopTime : HitStopTime);
	StartCameraShake(bCritical ? CritShakeStrength : HitShakeStrength, bCritical ? ShakeDuration * 1.6f : ShakeDuration);
	if (bCritical) Target->ShowPopup(TEXT("치명타!"), FLinearColor(1.0f, 0.8f, 0.2f), 1.5f);
}

void ABWPlayerController::StartHitStop(float RealSeconds)
{
	if (RealSeconds <= 0.0f) return;
	// 아주 짧게 세상을 거의 멈춘다. 되돌리기는 실제 시간 기준(코어 티커)이라 시간 배율의 영향을 받지 않는다
	if (HitStopTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(HitStopTicker);
	UGameplayStatics::SetGlobalTimeDilation(this, 0.03f);
	TWeakObjectPtr<ABWPlayerController> WeakThis(this);
	HitStopTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
	{
		if (ABWPlayerController* PC = WeakThis.Get())
		{
			UGameplayStatics::SetGlobalTimeDilation(PC, 1.0f);
			PC->HitStopTicker.Reset();
		}
		return false;
	}), RealSeconds);
}

void ABWPlayerController::StartCameraShake(float Amplitude, float Duration)
{
	if (!BattleCamera || Amplitude <= 0.0f || Duration <= 0.0f) return;
	if (!ShakeTicker.IsValid())
	{
		ShakeHome = BattleCamera->GetActorLocation();
		TWeakObjectPtr<ABWPlayerController> WeakThis(this);
		ShakeTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float DeltaTime)
		{
			ABWPlayerController* PC = WeakThis.Get();
			return PC ? PC->TickCameraShake(DeltaTime) : false;
		}));
	}
	// 이미 흔들리는 중이면 더 센 쪽으로 다시 시작
	ShakeStrength = FMath::Max(Amplitude, ShakeTimeLeft > 0.0f ? ShakeStrength : 0.0f);
	ShakeTimeLeft = ShakeTotalTime = Duration;
}

bool ABWPlayerController::TickCameraShake(float DeltaTime)
{
	if (!BattleCamera)
	{
		ShakeTicker.Reset();
		return false;
	}
	ShakeTimeLeft -= DeltaTime;
	if (ShakeTimeLeft <= 0.0f)
	{
		BattleCamera->SetActorLocation(ShakeHome);
		ShakeTicker.Reset();
		return false;
	}
	// 남은 시간에 비례해 약해지는 무작위 흔들림
	const float Falloff = ShakeTimeLeft / FMath::Max(ShakeTotalTime, KINDA_SMALL_NUMBER);
	const FVector Offset = FMath::VRand() * ShakeStrength * Falloff * Falloff;
	BattleCamera->SetActorLocation(ShakeHome + Offset);
	return true;
}

void ABWPlayerController::ShowTurnBanner(const FString& Title, const FString& Sub, const FLinearColor& Accent)
{
	if (!TurnBannerWidget) TurnBannerWidget = CreateWidget<UBWTurnBannerWidget>(this, UBWTurnBannerWidget::StaticClass());
	if (TurnBannerWidget) TurnBannerWidget->ShowBanner(Title, Sub, Accent);
}

void ABWPlayerController::PreloadCombatAssets()
{
	auto Keep = [this](UObject* Asset) { if (Asset) PreloadedAssets.AddUnique(Asset); };
	auto KeepCard = [&Keep](const FBWCardData& Card)
	{
		Keep(Card.NiagaraEffect.LoadSynchronous());
		Keep(Card.SkillEffect.LoadSynchronous());
		Keep(Card.CardSound.LoadSynchronous());
		Keep(Card.BelowImage.LoadSynchronous()); // 카드 그림: 위젯은 참조만 읽어서 미리 불러 둬야 보인다
		Keep(Card.AboveImage.LoadSynchronous());
	};

	if (PossessedBWCharacter && PossessedBWCharacter->DeckComponent)
	{
		const UDeckComponent* Deck = PossessedBWCharacter->DeckComponent;
		for (const FBWCardData& Card : Deck->Hand) KeepCard(Card);
		for (const FBWCardData& Card : Deck->Deck) KeepCard(Card);
		for (const FBWCardData& Card : Deck->Discard) KeepCard(Card);
		Keep(PossessedBWCharacter->HitFlashMaterial.LoadSynchronous());
		Keep(PossessedBWCharacter->DeathEffect.LoadSynchronous());
	}
	TArray<AActor*> Characters;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), Characters);
	for (AActor* Actor : Characters)
	{
		const ABWCharacter* Char = Cast<ABWCharacter>(Actor);
		if (!Char || !Char->bIsEnemy) continue;
		for (const FBWEnemyTurnPattern& Pattern : Char->CombatPatterns)
		{
			for (const FBWEnemyPattern& Action : Pattern.Actions)
			{
				Keep(Action.ActionNiagara.LoadSynchronous());
				Keep(Action.ActionEffect.LoadSynchronous());
				Keep(Action.ActionSound.LoadSynchronous());
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[BWCombat] Preloaded %d combat assets"), PreloadedAssets.Num());
	if (HandWidget && PossessedBWCharacter && PossessedBWCharacter->DeckComponent) HandWidget->RefreshHand(PossessedBWCharacter->DeckComponent->GetCurrentHand());
}

void ABWPlayerController::PlayPlayerDuel(ABWCharacter* PlayerChar, ABWCharacter* Enemy, const FBWCardData& AttackCard, bool bCardBoost, TFunction<void()> OnFinished)
{
	if (!PlayerChar || !Enemy || !TargetGridManager || Enemy->CurrentHP <= 0)
	{
		if (OnFinished) OnFinished();
		return;
	}

	// 5-1: 양쪽이 굴려 큰 쪽의 숫자가 피해 (5-3: 치명타 → 배율 → 방어 → 방어도 → HP). 국가 기믹은 내 쪽 굴림 조건에 반영
	// 결과는 지금 정하고(같은 시드 = 같은 결과), 화면은 그 결과를 보여 준다
	// [수정] 내 숫자 범위 = 기본 공격 (+ 공격카드로 강화했으면 카드 몫)
	const FBWDuelSide PlayerSide = UBWDimensionRules::ApplyToPlayerSide(Battlefield, PlayerChar->MakeAttackSide(bCardBoost ? &AttackCard : nullptr));
	const FBWDuelSide EnemySide = Enemy->MakeEnemyDuelSide();
	const FBWDuelResult Result = UBWDuelRules::ResolveDuel(PlayerSide, EnemySide, TargetGridManager->GetCombatRandom());
	if (Result.bAttackerRerolled)
	{
		PlayerChar->DuelRerollsLeft = FMath::Max(0, PlayerChar->DuelRerollsLeft - 1);
		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Reroll augment used (%d left this battle)"), PlayerChar->DuelRerollsLeft);
	}

	FBWDuelPresentation Show;
	Show.LeftName = TEXT("나");
	Show.LeftSkill = bCardBoost ? FString::Printf(TEXT("기본 공격 + %s"), *AttackCard.CardName) : AttackCard.CardName;
	Show.LeftRange = PlayerSide.SkillRange;
	Show.LeftRoll = Result.AttackerRoll;
	Show.RightName = Enemy->EnemyDisplayName;
	const FBWEnemyPattern* Action = Enemy->GetCurrentAction();
	Show.RightSkill = Action ? Action->ActionName : TEXT("스킬");
	Show.RightRange = EnemySide.SkillRange;
	Show.RightRoll = Result.DefenderRoll;
	Show.Winner = Result.bDraw ? -1 : (Result.bAttackerWon ? 0 : 1);
	Show.Damage = Result.FinalDamage;
	Show.bCritical = Result.bCritical;
	Show.TieRerolls = Result.TieRerolls;
	Show.bRerolled = Result.bAttackerRerolled;

	// 진 쪽이 받을 최종 피해도 지금 정해서 화면에 보여 준다 (내가 지면 내 방어가 적용된다)
	FBWPlannedHit Hit;
	if (!Result.bDraw)
	{
		Hit = PlanDuelHit(Result.bAttackerWon ? Enemy : PlayerChar, Result.FinalDamage, Result.bAttackerWon);
		FillHitPresentation(Show, Hit, !Result.bAttackerWon);
	}

	TWeakObjectPtr<ABWCharacter> WeakPlayer(PlayerChar), WeakEnemy(Enemy);
	ShowDuel(Show, [this, WeakPlayer, WeakEnemy, AttackCard, PlayerSide, EnemySide, Result, Hit, OnFinished]()
	{
		ABWCharacter* P = WeakPlayer.Get();
		ABWCharacter* E = WeakEnemy.Get();
		if (!P || !E || Result.bDraw)
		{
			if (P && E) ApplyPlayerDuelOutcome(P, E, AttackCard, PlayerSide, EnemySide, Result, Hit);
			if (OnFinished) OnFinished();
			return;
		}

		// 이긴 쪽이 진 쪽으로 찌르는 순간 피해가 들어간다
		ABWCharacter* Winner = Result.bAttackerWon ? P : E;
		ABWCharacter* Loser = Result.bAttackerWon ? E : P;
		PlayLungeThen(Winner, Loser,
			[this, WeakPlayer, WeakEnemy, AttackCard, PlayerSide, EnemySide, Result, Hit]()
			{
				if (WeakPlayer.IsValid() && WeakEnemy.IsValid()) ApplyPlayerDuelOutcome(WeakPlayer.Get(), WeakEnemy.Get(), AttackCard, PlayerSide, EnemySide, Result, Hit);
			},
			OnFinished);
	});
}

FBWPlannedHit ABWPlayerController::PlanDuelHit(ABWCharacter* Target, int32 Damage, bool bPlayerDeals, int32 ShieldMP)
{
	if (!Target) return FBWPlannedHit();
	// 11-1 타이가: 내가 이겨 피해를 준 최초 3회는 방어도 무시 (과부하면 적의 첫 공격 1회가 내 방어도 무시)
	bool bIgnoreBlock = false;
	if (Damage > 0)
	{
		bIgnoreBlock = bPlayerDeals ? UBWDimensionRules::ConsumeTaigaCharge(Battlefield) : UBWDimensionRules::ConsumeTaigaEnemyCharge(Battlefield);
	}
	return Target->PlanHit(Damage, bIgnoreBlock, ShieldMP);
}

void ABWPlayerController::FillHitPresentation(FBWDuelPresentation& Show, const FBWPlannedHit& Hit, bool bPlayerIsHit) const
{
	Show.bPlayerIsHit = bPlayerIsHit;
	Show.bEvaded = Hit.Defense.bEvaded;
	Show.FinalDamage = Hit.Defense.DamageAfterDefense;
	Show.DefenseReduced = Hit.RawDamage - Hit.Defense.DamageAfterDefense;
	Show.BlockAbsorbed = Hit.BlockAbsorbed;
	Show.HPLoss = Hit.HPLoss;
	if (Hit.bIgnoreBlock) Show.Extra += TEXT("방어도 무시");

	// 내 방어 수치: 방어 = "-2", 회피 = "회피!"/"회피 실패", 매직실드 = "실드 -5"
	if (bPlayerIsHit && PossessedBWCharacter)
	{
		switch (PossessedBWCharacter->Defense.Type)
		{
		case EDefenseType::Evade:       Show.DefenseText = Hit.Defense.bEvaded ? TEXT("회피!") : TEXT("회피 실패"); break;
		case EDefenseType::MagicShield: Show.DefenseText = Hit.Defense.MPSpent > 0 ? FString::Printf(TEXT("실드 -%d"), Hit.Defense.ShieldBlocked) : FString(TEXT("가드 없음")); break;
		default:                        Show.DefenseText = FString::Printf(TEXT("-%d"), PossessedBWCharacter->Defense.FlatReduction); break;
		}
	}
}

void ABWPlayerController::ApplyPlayerDuelOutcome(ABWCharacter* PlayerChar, ABWCharacter* Enemy, const FBWCardData& AttackCard, const FBWDuelSide& PlayerSide, const FBWDuelSide& EnemySide, const FBWDuelResult& Result, const FBWPlannedHit& Hit)
{
	if (!PlayerChar || !Enemy) return;
	ABWCharacter* Loser = nullptr;
	if (!Result.bDraw)
	{
		Loser = Result.bAttackerWon ? Enemy : PlayerChar;

		// 합 화면에서 보여 준 그대로 반영 (방어·타이가 방어도 무시는 PlanDuelHit에서 이미 정함)
		const FBWDefenseResult Defended = Loser->ApplyPlannedHit(Hit);
		PlayImpactFeedback(Result.bAttackerWon ? PlayerChar : Enemy, Loser, Defended, Hit.HPLoss, Result.bCritical);

		// 증강 '가시 갑옷': 적이 이긴 합에서도 받은 피해의 일부를 되돌려준다
		if (!Result.bAttackerWon) ReflectThorns(Enemy, Defended.DamageAfterDefense);

		// 9장: 이긴 쪽 스킬의 상태이상을 진 쪽에 건다 (살아 있을 때만)
		const FBWDuelSide& WinnerSide = Result.bAttackerWon ? PlayerSide : EnemySide;
		if (WinnerSide.AppliedStatus != EStatusEffect::None && Loser->CurrentHP > 0 && !Loser->IsActorBeingDestroyed())
		{
			Loser->ApplyStatus(WinnerSide.AppliedStatus, WinnerSide.StatusDuration, WinnerSide.StatusValue);
		}

		// 밀기 효과가 붙은 스킬이 이기면 진 쪽을 1칸 민다 (베렌드 벽 충돌은 ResolvePushAfterDuel)
		if (WinnerSide.bPushesTarget)
		{
			ResolvePushAfterDuel(Result.bAttackerWon ? PlayerChar : Enemy, Loser, Result.bAttackerWon);
		}
	}

	// 연출: 진 쪽 칸에서 카드 이펙트와 소리
	if (Loser)
	{
		const FVector FxLoc = Loser->GetActorLocation() + FVector(0, 0, 40);
		BWSpawnFX(GetWorld(), AttackCard.NiagaraEffect, AttackCard.SkillEffect, FxLoc);
		if (AttackCard.CardSound.LoadSynchronous()) UGameplayStatics::PlaySoundAtLocation(GetWorld(), AttackCard.CardSound.Get(), FxLoc);
	}

	const FString Summary = Result.bDraw
		? FString::Printf(TEXT("[합] %s vs %s: 3연속 동점, 피해 없음"), *AttackCard.CardName, *Enemy->EnemyDisplayName)
		: FString::Printf(TEXT("[합] %s %d vs %s %d → %s 승리, 피해 %d%s"), *AttackCard.CardName, Result.AttackerRoll, *Enemy->EnemyDisplayName, Result.DefenderRoll,
			Result.bAttackerWon ? TEXT("내가") : TEXT("적이"), Result.FinalDamage, Result.bCritical ? TEXT(" (치명타)") : TEXT(""));
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s"), *Summary);
	Prompt(Summary, 3.5f, EBWPromptStyle::Result);

	OnDuelResolved.Broadcast(Result, Enemy, AttackCard);
}

void ABWPlayerController::Prompt(const FString& Message, float Duration, EBWPromptStyle Style)
{
	if (BattleHUDWidget) BattleHUDWidget->ShowPrompt(Message, Duration, Style);
}

void ABWPlayerController::HandleCardBurned(const FBWCardData& BurnedCard)
{
	Prompt(FString::Printf(TEXT("손패가 가득 차서(최대 %d장) '%s'을(를) 버렸습니다"),
		PossessedBWCharacter && PossessedBWCharacter->DeckComponent ? PossessedBWCharacter->DeckComponent->MaxHandSize : 10, *BurnedCard.CardName),
		3.5f, EBWPromptStyle::Warning);
}

void ABWPlayerController::WaitForMovementThen(ABWCharacter* Mover, TFunction<void()> Done)
{
	// 이동 연출(VisualMoveQueue)이 끝날 때까지 기다렸다가 Done을 부른다 (최대 3초)
	TWeakObjectPtr<ABWCharacter> WeakMover(Mover);
	TSharedRef<float> Elapsed = MakeShared<float>(0.0f);
	TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
	GetWorldTimerManager().SetTimer(*Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakMover, Elapsed, Handle, Done]()
	{
		*Elapsed += 0.05f;
		if (WeakMover.IsValid() && WeakMover->IsMoving() && *Elapsed < 3.0f) return;

		// [수정] ClearTimer가 지금 실행 중인 이 람다(캡처 포함)를 해제하므로, 필요한 것을 먼저 지역 변수로 옮긴다
		TFunction<void()> DoneCopy = Done;
		ABWPlayerController* Self = this;
		TSharedRef<FTimerHandle> HandleCopy = Handle;
		Self->GetWorldTimerManager().ClearTimer(*HandleCopy);
		if (DoneCopy) DoneCopy();
	}), 0.05f, true);
}

void ABWPlayerController::PlayLungeThen(ABWCharacter* Attacker, ABWCharacter* Target, TFunction<void()> OnImpact, TFunction<void()> OnFinished)
{
	if (!Attacker || !Target)
	{
		if (OnImpact) OnImpact();
		if (OnFinished) OnFinished();
		return;
	}

	// 상대 쪽으로 반 칸 찌르고(이 순간 판정) 제자리로 돌아온다. 멀리 있는 적(사거리 공격)이면 반 칸만 내딛는다
	const FVector Home = Attacker->GetActorLocation();
	const float Dist = FVector::Dist2D(Home, Target->GetActorLocation());
	const float MaxStep = TargetGridManager ? TargetGridManager->TileSpacing * 0.45f : 90.0f;
	FVector Strike = FMath::Lerp(Home, Target->GetActorLocation(), Dist > KINDA_SMALL_NUMBER ? FMath::Min(0.45f, MaxStep / Dist) : 0.0f);
	Strike.Z = Home.Z;
	Attacker->FaceTowards(Target->GetActorLocation());
	Attacker->AddMovementStep(Strike, false);

	TWeakObjectPtr<ABWCharacter> WeakAttacker(Attacker);
	WaitForMovementThen(Attacker, [this, WeakAttacker, Home, OnImpact, OnFinished]()
	{
		if (OnImpact) OnImpact();
		if (!WeakAttacker.IsValid() || WeakAttacker->IsActorBeingDestroyed())
		{
			if (OnFinished) OnFinished();
			return;
		}
		WeakAttacker->AddMovementStep(Home, false);
		WaitForMovementThen(WeakAttacker.Get(), [OnFinished]() { if (OnFinished) OnFinished(); });
	});
}

void ABWPlayerController::HandleEnemyClicked(ABWCharacter* Enemy)
{
	if (!PossessedBWCharacter || !Enemy || !TargetGridManager) return;
	UDeckComponent* Deck = PossessedBWCharacter->DeckComponent;

	if (IsEnemyInReach(Enemy, GetReachForHandIndex(INDEX_NONE)))
	{
		// [수정] 기본 공격 사거리(보통 맞닿은 적) 안의 적을 누르면 기본 공격 (강화하려면 공격카드를 그 적에게 쓴다)
		if (Deck) OpenDuel(Enemy, INDEX_NONE);
		return;
	}

	// 떨어져 있으면 남은 이동 칸 안에서 그 적에게 맞닿는 칸까지 다가간다 (닿으면 합이 열린다)
	if (PossessedBWCharacter->RemainingMove <= 0)
	{
		Prompt(TEXT("이번 턴 이동 칸을 다 썼습니다. 턴을 넘기세요"), 3.0f, EBWPromptStyle::Warning);
		return;
	}
	const TArray<FIntPoint> Path = TargetGridManager->FindRushPath(PossessedBWCharacter->OwnGridCoordinate, Enemy->OwnGridCoordinate, PossessedBWCharacter->RemainingMove, false);
	if (Path.Num() == 0)
	{
		Prompt(TEXT("그 적에게 다가갈 길이 없습니다"), 3.0f, EBWPromptStyle::Warning);
		return;
	}
	MovePlayerAlongPath(Path);
}

void ABWPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	TraceGridUnderCursor();

	// [수정] 숫자 키로 카드를 집은 동안: 좌클릭 = 사용(원래 자리 위면 내려놓기), 우클릭 = 내려놓기. 칸 클릭으로는 넘기지 않는다
	if (HandWidget && HandWidget->IsHoldingCard())
	{
		if (bActionInProgress || bEnemyPhase) HandWidget->CancelHeldCard();
		else if (WasInputKeyJustPressed(EKeys::LeftMouseButton)) HandWidget->ReleaseHeldCard();
		else if (WasInputKeyJustPressed(EKeys::RightMouseButton)) HandWidget->CancelHeldCard();
	}
	else
	{
		if (WasInputKeyJustPressed(EKeys::LeftMouseButton)) OnLeftClick();
		if (WasInputKeyJustPressed(EKeys::RightMouseButton)) OnRightClick();
	}

	UpdateReachPreview();

	// [추가] Q: 기본 공격 (맞닿은 적이 하나면 바로, 여럿이면 고른다)
	if (WasInputKeyJustPressed(EKeys::Q) && !(HandWidget && HandWidget->IsHoldingCard())) EnterBasicAttackMode();

	// [수정] 숫자 키 1~9, 0(키패드 포함): 손패 N번째 카드를 집어 마우스에 붙인다 (손패 위젯이 없으면 바로 사용)
	static const FKey CardKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero };
	static const FKey PadKeys[] = { EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive,
		EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine, EKeys::NumPadZero };
	for (int32 i = 0; i < UE_ARRAY_COUNT(CardKeys); ++i)
	{
		if (!WasInputKeyJustPressed(CardKeys[i]) && !WasInputKeyJustPressed(PadKeys[i])) continue;
		if (bActionInProgress || bEnemyPhase) break;
		if (!PossessedBWCharacter || !PossessedBWCharacter->DeckComponent || !PossessedBWCharacter->DeckComponent->Hand.IsValidIndex(i))
		{
			Prompt(FString::Printf(TEXT("%d번째 카드가 없습니다"), i + 1), 2.0f, EBWPromptStyle::Warning);
			break;
		}
		if (HandWidget) HandWidget->ToggleHoldCard(i);
		else UseCardShortcut(i);
		break;
	}
}

void ABWPlayerController::UseCardShortcut(int32 HandIndex)
{
	if (!PossessedBWCharacter || !PossessedBWCharacter->DeckComponent) return;
	if (!PossessedBWCharacter->DeckComponent->Hand.IsValidIndex(HandIndex))
	{
		Prompt(FString::Printf(TEXT("%d번째 카드가 없습니다"), HandIndex + 1), 2.0f, EBWPromptStyle::Warning);
		return;
	}
	// 집거나 끌어 놓은 카드는 마우스 아래 적을 대상으로 먼저 본다
	TGuardValue<bool> CursorTarget(bPreferCursorTarget, true);
	SelectCard(HandIndex);
}

void ABWPlayerController::OnRightClick() 
{ 
	if (SelectedCardIndex != -1 || bIsMovementMode || bIsDuelTargetingMode) CancelSelection(); 
}

void ABWPlayerController::CancelSelection()
{
	SelectedCardIndex = -1;
	bIsMovementMode = false;
	bIsDuelTargetingMode = false;
	DuelCardIndex = INDEX_NONE;
	if (HandWidget) HandWidget->SelectedCardIndex = -1;
	if (TargetGridManager) TargetGridManager->ClearAllRanges();
}

AGridTile* ABWPlayerController::PickTileUnderCursor()
{
	if (!TargetGridManager) return nullptr;

	FVector RayOrigin, RayDir;
	if (!DeprojectMousePositionToWorld(RayOrigin, RayDir) || FMath::IsNearlyZero(RayDir.Z)) return nullptr;

	// 그리드 바닥 높이(칸 0,0의 높이)와 만나는 점
	const float PlaneZ = TargetGridManager->GetTileWorldLocation(FIntPoint::ZeroValue).Z;
	const float T = (PlaneZ - RayOrigin.Z) / RayDir.Z;
	if (T <= 0.0f) return nullptr;

	FIntPoint Coord;
	return TargetGridManager->WorldToGrid(RayOrigin + RayDir * T, Coord) ? TargetGridManager->GetTileAtCoordinate(Coord) : nullptr;
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

	// [수정] 처음 맞은 물체 대신 마우스 레이와 그리드 바닥 평면의 교점으로 칸을 고른다
	// (말 모델·이펙트·글자가 칸을 가려도 바닥을 누를 수 있도록)
	{
		{
			AGridTile* HitTile = PickTileUnderCursor();
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
					if (!bIsSkillTargetingMode && !bIsMovementMode && !bIsDuelTargetingMode) TargetGridManager->DrawPlacementGuidelines();
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
									// [수정] 적이 공개한 이번 턴 스킬: {V}=최댓값, {D}=속도, {R}=최솟값
									if (const FBWEnemyPattern* Action = Enemy->GetCurrentAction())
									{
										const FBWDuelSide Side = Enemy->MakeEnemyDuelSide();
										BattleHUDWidget->ShowTooltip(Action->ActionName, Action->Description, Side.SkillRange.Max, Enemy->RolledSpeed, Side.SkillRange.Min);
										bFoundIntent = true;
										break;
									}
								}
							}
						}
						// 2. 아군 배치 구역 툴팁
						else if (!bIsSkillTargetingMode && CurrentTileUnderCursor->CurrentState == ETileState::AllyZone)
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
								int32 MovePts = PossessedBWCharacter ? PossessedBWCharacter->RemainingMove : 0;
								BattleHUDWidget->ShowTooltip(BattleHUDWidget->MoveRange_Tooltip.Title, BattleHUDWidget->MoveRange_Tooltip.Description, MovePts);
								bFoundIntent = true;
							}
						}
						
						if (!bFoundIntent) BattleHUDWidget->HideTooltip();
					}

					if (bIsDuelTargetingMode && PossessedBWCharacter)
					{
						// [수정] 합 대상 고르기: 맞닿은 적 칸만 강조
						RefreshDuelTargetHighlight();
					}
					else if (bIsSkillTargetingMode && !bIsMovementMode && PossessedBWCharacter && PossessedBWCharacter->DeckComponent)
					{
						TArray<FBWCardData> HandCards = PossessedBWCharacter->DeckComponent->GetCurrentHand();
						if (SelectedCardIndex >= 0 && SelectedCardIndex < HandCards.Num())
						{
							TargetGridManager->DrawSkillPreview(PossessedBWCharacter->OwnGridCoordinate, CurrentTileUnderCursor->GridCoordinate, HandCards[SelectedCardIndex]);
						}
						// [복구] 스킬 선택 중에도 마우스 아래 타일은 노란색(Hovered)으로 표시
						CurrentTileUnderCursor->SetTileState(ETileState::Hovered);
					}
					else if (bIsMovementMode && PossessedBWCharacter)
					{
						TargetGridManager->DrawMovementPreview(PossessedBWCharacter->OwnGridCoordinate, PossessedBWCharacter->RemainingMove);
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
		if (!bIsSkillTargetingMode && !bIsMovementMode && !bIsDuelTargetingMode) TargetGridManager->DrawPlacementGuidelines();
		else if (!bIsMovementMode && !bIsDuelTargetingMode) TargetGridManager->ClearAllRanges();
		else if (bIsMovementMode && PossessedBWCharacter) TargetGridManager->DrawMovementPreview(PossessedBWCharacter->OwnGridCoordinate, PossessedBWCharacter->RemainingMove);
		
		LastHoveredTile = nullptr;
		DestroyPlacementPreview();
	}
	CurrentTileUnderCursor = nullptr;
}

void ABWPlayerController::UpdatePlacementPreview()
{
	if (!bIsSkillTargetingMode && !bIsMovementMode && !bIsDuelTargetingMode && CurrentTileUnderCursor && TargetGridManager->IsValidAllyPlacementArea(CurrentTileUnderCursor->GridCoordinate))
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
	if (bActionInProgress || bEnemyPhase) return;
	if (bIsMovementMode || bIsDuelTargetingMode) CancelSelection();
	if (!PossessedBWCharacter || !PossessedBWCharacter->DeckComponent) return;

	UDeckComponent* Deck = PossessedBWCharacter->DeckComponent;
	const EBWCardUseResult Check = Deck->CanUseCard(Index, PossessedBWCharacter->CurrentMP);
	if (Check != EBWCardUseResult::Ok)
	{
		PlayCardUnusableFeedback(FString::Printf(TEXT("카드를 쓸 수 없습니다: %s"), *UEnum::GetValueAsString(Check)));
		return;
	}
	if (!UBWDimensionRules::CanUseCard(Battlefield, Deck->Hand[Index]))
	{
		PlayCardUnusableFeedback(Deck->Hand[Index].CardKind == ECardKind::Effect
			? FString(TEXT("겔레온: 이번 전투는 효과카드를 쓰지 않기로 했습니다."))
			: FString::Printf(TEXT("겔레온: 이번 전투에 강화할 수 있는 공격카드는 '%s'뿐입니다. 기본 공격은 언제나 쓸 수 있습니다."), *Battlefield.GeleonAttackCardName));
		return;
	}

	// [수정] 공격카드: 내가 선공인 합에만 쓴다. 접촉으로 열린 합이 있으면 바로 그 적과, 아니면 맞닿은 적을 고른다
	if (Deck->Hand[Index].CardKind == ECardKind::Attack)
	{
		if (PossessedBWCharacter->IsStunned()) { PlayCardUnusableFeedback(TEXT("기절 상태라 합을 걸 수 없습니다.")); return; }
		if (!Deck->CanOpenDuel()) { PlayCardUnusableFeedback(TEXT("이번 턴에 이미 합을 열었습니다.")); return; }
		const int32 Reach = GetReachForHandIndex(Index);
		// 집은 카드를 사거리 안의 적 위에서 놓았으면 그 적과
		if (bPreferCursorTarget && IsEnemyInReach(FocusedCharacter, Reach))
		{
			OpenDuel(FocusedCharacter, Index);
			return;
		}
		const TArray<ABWCharacter*> InReach = GetEnemiesInReach(Reach);
		if (InReach.Num() == 0)
		{
			PlayCardUnusableFeedback(Reach <= 1 ? FString(TEXT("이 공격카드는 맞닿은 적에게만 쓸 수 있습니다. 먼저 적에게 다가가세요"))
				: FString::Printf(TEXT("사거리 %d칸 안에 적이 없습니다. 조금 더 다가가세요"), Reach));
			return;
		}

		// [수정] 사거리 안의 적이 하나여도 자동으로 걸지 않고 고르게 한다
		EnterDuelTargeting(Index);
		Prompt(FString::Printf(TEXT("합을 걸 적을 누르세요 (사거리 %d · 금색 칸 · 우클릭 취소)"), Reach), 0.0f);
		return;
	}

	// [수정] 효과카드: 고르는 즉시 나에게 적용 (회복, 방어도, 이동, MP, 뽑기, 강화, 집중)
	UseEffectCard(Index);
}

void ABWPlayerController::UseEffectCard(int32 Index)
{
	ABWCharacter* PlayerChar = PossessedBWCharacter;
	if (!PlayerChar || !PlayerChar->DeckComponent) return;

	FBWCardData Card;
	if (HandWidget) HandWidget->PlayDiscardAnimation(Index);
	if (!PlayerChar->DeckComponent->UseCard(Index, Card)) return;
	Battlefield.bGeleonKindLocked = true;

	// 적용 순서는 카드에 적힌 순서대로. 하나라도 실제로 바뀐 것만 안내에 넣는다
	TArray<FString> Parts;
	if (Card.EffectHeal > 0)
	{
		const int32 Healed = PlayerChar->Heal(Card.EffectHeal);
		Parts.Add(FString::Printf(TEXT("HP +%d"), Healed));
	}
	if (Card.EffectBlock > 0)
	{
		PlayerChar->Block += Card.EffectBlock;
		PlayerChar->ShowPopup(FString::Printf(TEXT("방어도 +%d"), Card.EffectBlock), FLinearColor(0.45f, 0.75f, 1.0f));
		Parts.Add(FString::Printf(TEXT("방어도 +%d"), Card.EffectBlock));
	}
	if (Card.EffectMove > 0)
	{
		PlayerChar->RemainingMove += Card.EffectMove;
		PlayerChar->ShowPopup(FString::Printf(TEXT("이동 +%d"), Card.EffectMove), FLinearColor(0.5f, 1.0f, 0.6f));
		Parts.Add(FString::Printf(TEXT("이동 +%d"), Card.EffectMove));
	}
	if (Card.EffectMP > 0)
	{
		const int32 Before = PlayerChar->CurrentMP;
		PlayerChar->CurrentMP = FMath::Min(PlayerChar->MaxMP, PlayerChar->CurrentMP + Card.EffectMP);
		PlayerChar->ShowPopup(FString::Printf(TEXT("MP +%d"), PlayerChar->CurrentMP - Before), FLinearColor(0.55f, 0.6f, 1.0f));
		Parts.Add(FString::Printf(TEXT("MP +%d"), PlayerChar->CurrentMP - Before));
	}
	if (Card.EffectAttackMaxBonus > 0)
	{
		PlayerChar->TurnAttackMaxBonus += Card.EffectAttackMaxBonus;
		PlayerChar->ShowPopup(FString::Printf(TEXT("공격 최댓값 +%d"), Card.EffectAttackMaxBonus), FLinearColor(1.0f, 0.75f, 0.3f));
		Parts.Add(FString::Printf(TEXT("이번 턴 공격카드 최댓값 +%d"), Card.EffectAttackMaxBonus));
	}
	if (Card.SelfStatus != EStatusEffect::None)
	{
		PlayerChar->ApplyStatus(Card.SelfStatus, Card.SelfStatusDuration, 0);
		FBWStatusInstance Shown;
		Shown.Type = Card.SelfStatus;
		Shown.RemainingTurns = Card.SelfStatusDuration;
		Parts.Add(UBWStatusRules::DescribeStatuses({ Shown }));
	}
	if (Card.EffectDraw > 0)
	{
		PlayerChar->DeckComponent->DrawCards(Card.EffectDraw);
		Parts.Add(FString::Printf(TEXT("카드 %d장 뽑기"), Card.EffectDraw));
	}

	// 연출: 내 칸에서 카드 이펙트와 소리
	const FVector FxLoc = PlayerChar->GetActorLocation() + FVector(0, 0, 40);
	BWSpawnFX(GetWorld(), Card.NiagaraEffect, Card.SkillEffect, FxLoc);
	if (Card.CardSound.LoadSynchronous()) UGameplayStatics::PlaySoundAtLocation(GetWorld(), Card.CardSound.Get(), FxLoc);

	if (HandWidget) HandWidget->RefreshHand(PlayerChar->DeckComponent->GetCurrentHand());
	if (bIsMovementMode && TargetGridManager) TargetGridManager->DrawMovementPreview(PlayerChar->OwnGridCoordinate, PlayerChar->RemainingMove);

	const FString Summary = FString::Printf(TEXT("[효과] %s: %s"), *Card.CardName, *FString::Join(Parts, TEXT(", ")));
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s"), *Summary);
	Prompt(Summary, 2.5f, EBWPromptStyle::Result);
}

void ABWPlayerController::OnLeftClick()
{
	TraceGridUnderCursor();
	if (TargetGridManager && !TargetGridManager->IsGridReady()) return;

	// 이동·합 연출 중이거나 적 턴에는 입력을 받지 않는다
	if (bActionInProgress || bEnemyPhase) return;

	if (!CurrentTileUnderCursor || !TargetGridManager) return;
	ABWCharacter* ClickedChar = TargetGridManager->GetCharacterAtCoordinate(CurrentTileUnderCursor->GridCoordinate);

	if (bIsMovementMode && PossessedBWCharacter)
	{
		// [수정] 적을 누르면 그 적에게 맞닿는 칸까지 다가가고, 빈 칸이면 그 칸으로 이동
		if (ClickedChar && ClickedChar->bIsEnemy) HandleEnemyClicked(ClickedChar);
		else TryMovePlayerTo(CurrentTileUnderCursor->GridCoordinate);
		return;
	}

	if (bIsDuelTargetingMode && PossessedBWCharacter)
	{
		// [수정] 강조된 맞닿은 적을 누르면 합을 연다
		ABWCharacter* Target = TargetGridManager->GetCharacterAtCoordinate(CurrentTileUnderCursor->GridCoordinate);
		if (Target && Target->bIsEnemy) OpenDuel(Target, DuelCardIndex);
		return;
	}

	if (!bIsSkillTargetingMode)
	{
		PlaceCharacterAndStartCombat(CurrentTileUnderCursor->GridCoordinate);
	}
	else if (SelectedCardIndex == -1 && ClickedChar && ClickedChar->bIsEnemy && PossessedBWCharacter)
	{
		// [추가] 아무것도 고르지 않고 적을 누르면: 맞닿아 있으면 합, 떨어져 있으면 다가간다
		HandleEnemyClicked(ClickedChar);
	}
}

void ABWPlayerController::RequestEndTurn()
{
	// 연출 중이거나 이미 적 턴이면 무시 (턴 종료 버튼 연타 방지)
	if (bActionInProgress || bEnemyPhase || !PossessedBWCharacter) return;
	bEnemyPhase = true;
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Player Turn Ended. Gathering Enemies for sequential execution..."));
	Prompt(TEXT("적 턴"), 0.0f, EBWPromptStyle::Result);
	ShowTurnBanner(TEXT("적 턴"), FString(), FLinearColor(1.0f, 0.4f, 0.35f));

	CancelSelection();
	PendingDuelTarget = nullptr;

	// 4-4: 1 출혈 등 상태이상 정산(남은 턴도 여기서 줄인다), 2 남은 손패는 버린 덱으로, 3 HP 직업은 방어도를 얻는다
	if (PossessedBWCharacter)
	{
		PossessedBWCharacter->EndOwnTurnStatuses();
		if (PossessedBWCharacter->DeckComponent) PossessedBWCharacter->DeckComponent->EndTurn();
		PossessedBWCharacter->EndTurnResources();
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

	// 4-3: 적은 번호 순서(전투 시작 때 배치된 순서)대로 한 마리씩 행동한다
	EnemyTurnQueue.Sort([](const ABWCharacter& A, const ABWCharacter& B) { return A.EnemyOrder < B.EnemyOrder; });

	// 순차 실행 시작
	ProcessNextEnemyTurn();
}

void ABWPlayerController::ProcessNextEnemyTurn()
{
	// 승패가 정해졌으면 남은 적 행동을 멈춘다
	if (TargetGridManager && TargetGridManager->bBattleOver)
	{
		EnemyTurnQueue.Empty();
		return;
	}

	if (EnemyTurnQueue.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] All enemies acted. Delaying 0.5s before Player Turn..."));
		
		// [수정] 모든 적 행동 종료 후 0.5초 대기 후 플레이어 턴 시작
		FTimerHandle TurnResetTimer;
		GetWorldTimerManager().SetTimer(TurnResetTimer, this, &ABWPlayerController::BeginRound, 0.5f, false);

		return;
	}

	ABWCharacter* NextEnemy = EnemyTurnQueue[0];
	EnemyTurnQueue.RemoveAt(0);

	if (NextEnemy && NextEnemy->IsValidLowLevel() && NextEnemy->CurrentHP > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[BWCombat] Enemy Acting: %s"), *NextEnemy->GetName());

		// [수정] 한 마리가 이동·합 연출을 모두 마친 뒤에 다음 적이 움직인다
		TWeakObjectPtr<ABWCharacter> WeakEnemy(NextEnemy);
		auto FinishThisEnemy = [this, WeakEnemy]()
		{
			// 적의 자기 턴 끝: 출혈 정산과 상태이상 남은 턴 감소
			if (WeakEnemy.IsValid() && WeakEnemy->CurrentHP > 0) WeakEnemy->EndOwnTurnStatuses();
			FTimerHandle NextEnemyTimer;
			GetWorldTimerManager().SetTimer(NextEnemyTimer, this, &ABWPlayerController::ProcessNextEnemyTurn, 0.5f, false);
		};

		// 기절한 적은 이번 턴 행동을 건너뛴다
		if (NextEnemy->IsStunned())
		{
			NextEnemy->SkipTurnForStun();
			NextEnemy->ShowPopup(TEXT("기절 - 행동 불가"), FLinearColor(0.7f, 0.5f, 1.0f));
			FinishThisEnemy();
		}
		else
		{
			ExecuteEnemyActionThen(NextEnemy, FinishThisEnemy);
		}
	}
	else
	{
		// 유효하지 않은 적이면 즉시 다음 단계
		ProcessNextEnemyTurn();
	}
}

void ABWPlayerController::ExecuteEnemyAction(ABWCharacter* Enemy)
{
	ExecuteEnemyActionThen(Enemy, nullptr);
}

void ABWPlayerController::ExecuteEnemyActionThen(ABWCharacter* Enemy, TFunction<void()> OnDone)
{
	auto Finish = [OnDone]() { if (OnDone) OnDone(); };
	if (!Enemy || Enemy->CurrentHP <= 0 || !TargetGridManager) { Finish(); return; }

	const FBWEnemyPattern* Action = Enemy->GetCurrentAction();
	if (!Action)
	{
		Enemy->AdvancePattern();
		Finish();
		return;
	}

	bool bAttack = false;
	switch (Action->ActionType)
	{
	case EBWEnemyActionType::Heal:
		// 회복·강화 행동을 하는 적은 이동하지 않고 그 행동만 한다
		Enemy->CurrentHP = FMath::Min(Enemy->MaxHP, Enemy->CurrentHP + Action->Value);
		Enemy->ShowPopup(FString::Printf(TEXT("회복 +%d"), Action->Value), FLinearColor(0.3f, 1.0f, 0.4f));
		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s heals %d (HP %d)"), *Enemy->EnemyDisplayName, Action->Value, Enemy->CurrentHP);
		break;
	case EBWEnemyActionType::BuffAttack:
		// 강화 후 공격: 숫자 범위를 올린 뒤 같은 턴에 돌진한다
		Enemy->CurrentAttackBuff += Action->Value;
		Enemy->ShowPopup(FString::Printf(TEXT("강화 +%d"), Action->Value), FLinearColor(1.0f, 0.6f, 0.2f));
		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s powers up +%d (buff %d)"), *Enemy->EnemyDisplayName, Action->Value, Enemy->CurrentAttackBuff);
		bAttack = true;
		break;
	default:
		bAttack = true;
		break;
	}

	ABWCharacter* PlayerChar = PossessedBWCharacter;
	if (!bAttack || !PlayerChar || PlayerChar->CurrentHP <= 0)
	{
		Enemy->AdvancePattern();
		Finish();
		return;
	}

	// 자기 속도만큼 나를 향해 최단 경로로 돌진. 접촉하면 멈추고 적이 선공인 합 (이미 접촉해 있으면 이동 없이)
	const TArray<FIntPoint> Path = TargetGridManager->FindRushPath(Enemy->OwnGridCoordinate, PlayerChar->OwnGridCoordinate, Enemy->RolledSpeed, true);
	for (const FIntPoint& Step : Path)
	{
		if (AGridTile* StepTile = TargetGridManager->GetTileAtCoordinate(Step)) Enemy->AddMovementStep(StepTile->GetActorLocation() + FVector(0, 0, 20));
	}
	if (Path.Num() > 0) Enemy->OwnGridCoordinate = Path.Last();
	Enemy->RemainingMove = FMath::Max(0, Enemy->RemainingMove - Path.Num());

	// [수정] 걸어오는 연출이 끝난 뒤에 접촉을 판정하고, 찌르는 순간 피해가 들어간다
	TWeakObjectPtr<ABWCharacter> WeakEnemy(Enemy), WeakPlayer(PlayerChar);
	const int32 Steps = Path.Num();
	WaitForMovementThen(Enemy, [this, WeakEnemy, WeakPlayer, Steps, Finish]()
	{
		ABWCharacter* E = WeakEnemy.Get();
		ABWCharacter* P = WeakPlayer.Get();
		if (!E || !P || E->CurrentHP <= 0 || P->CurrentHP <= 0) { if (E) E->AdvancePattern(); Finish(); return; }

		const FIntPoint Delta = P->OwnGridCoordinate - E->OwnGridCoordinate;
		if (FMath::Abs(Delta.X) + FMath::Abs(Delta.Y) != 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s moved %d and could not reach the player"), *E->EnemyDisplayName, Steps);
			E->AdvancePattern();
			Finish();
			return;
		}

		// 합 화면(적의 숫자 굴림) → 적이 찌르는 순간 피해 → 다음 패턴
		PlayEnemyDuel(E, [WeakEnemy, Finish]() { if (WeakEnemy.IsValid()) WeakEnemy->AdvancePattern(); Finish(); });
	});
}

void ABWPlayerController::ReflectThorns(ABWCharacter* Enemy, int32 DamageTaken)
{
	if (!PossessedBWCharacter || !Enemy || Enemy->CurrentHP <= 0 || Enemy->IsActorBeingDestroyed()) return;
	const int32 Reflected = FMath::FloorToInt(DamageTaken * PossessedBWCharacter->AugmentTotals.ThornFraction);
	if (Reflected <= 0) return;
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Thorn armor reflects %d to %s"), Reflected, *Enemy->EnemyDisplayName);
	Enemy->TakeHit(Reflected, false);
}

void ABWPlayerController::OpenAugmentPick(int32 Act)
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;

	// 직업은 조종 중인 말, 없으면 배치할 말의 기본값
	// 직업은 캐릭터 선택에서 고른 것 (지도에서 부를 때는 조종 중인 말이 없다)
	EClassType PlayerClass = GI->SelectedClass;
	if (PossessedBWCharacter) PlayerClass = PossessedBWCharacter->CharacterClass;

	GI->BeginAugmentPick(Act, PlayerClass);
	if (GI->AugmentChoices.Num() == 0) return;

	TSubclassOf<UBWAugmentSelectWidget> WidgetClass = AugmentSelectWidgetClass ? AugmentSelectWidgetClass : TSubclassOf<UBWAugmentSelectWidget>(UBWAugmentSelectWidget::StaticClass());
	if (UBWAugmentSelectWidget* Widget = CreateWidget<UBWAugmentSelectWidget>(this, WidgetClass))
	{
		Widget->AddToViewport(600);
	}
}

bool ABWPlayerController::ComputeEnemyStrike(ABWCharacter* Enemy, FBWDuelSide& OutEnemySide, FBWDuelResult& OutResult)
{
	ABWCharacter* PlayerChar = PossessedBWCharacter;
	if (!Enemy || !PlayerChar || !TargetGridManager || PlayerChar->CurrentHP <= 0) return false;

	// 내 쪽은 카드를 내지 않는다. 받는 피해 배율(취약, 겔레온 과부하)만 반영
	FBWDuelSide TargetSide;
	PlayerChar->ApplyStatusesToDuelSide(TargetSide);
	TargetSide = UBWDimensionRules::ApplyToPlayerSide(Battlefield, TargetSide);
	OutEnemySide = Enemy->MakeEnemyDuelSide();
	OutResult = UBWDuelRules::ResolveEnemyStrike(OutEnemySide, TargetSide, TargetGridManager->GetCombatRandom());
	return true;
}

bool ABWPlayerController::ResolveEnemyDuel(ABWCharacter* Enemy)
{
	FBWDuelSide EnemySide;
	FBWDuelResult Result;
	if (!ComputeEnemyStrike(Enemy, EnemySide, Result)) return false;
	ApplyEnemyStrikeOutcome(Enemy, EnemySide, Result, PlanDuelHit(PossessedBWCharacter, Result.FinalDamage, false));
	return true;
}

void ABWPlayerController::PlayEnemyDuel(ABWCharacter* Enemy, TFunction<void()> OnFinished)
{
	// [추가] 매직실드(INT·FTH)이고 걸 MP가 있으면, 적이 굴리기 전에 가드에 걸 MP를 고른다
	if (Enemy && PossessedBWCharacter && PossessedBWCharacter->GetMaxGuardMP() > 0)
	{
		TWeakObjectPtr<ABWCharacter> WeakEnemy(Enemy);
		OpenGuardChoice(Enemy, [this, WeakEnemy, OnFinished](int32 ShieldMP)
		{
			if (!WeakEnemy.IsValid()) { if (OnFinished) OnFinished(); return; }
			PlayEnemyDuelWithGuard(WeakEnemy.Get(), ShieldMP, OnFinished);
		});
		return;
	}
	PlayEnemyDuelWithGuard(Enemy, 0, MoveTemp(OnFinished));
}

void ABWPlayerController::OpenGuardChoice(ABWCharacter* Enemy, TFunction<void(int32)> OnChosen)
{
	TSubclassOf<UBWGuardChoiceWidget> WidgetClass = GuardChoiceWidgetClass ? GuardChoiceWidgetClass : TSubclassOf<UBWGuardChoiceWidget>(UBWGuardChoiceWidget::StaticClass());
	UBWGuardChoiceWidget* Widget = CreateWidget<UBWGuardChoiceWidget>(this, WidgetClass);
	if (!Widget || !PossessedBWCharacter)
	{
		if (OnChosen) OnChosen(0);
		return;
	}

	// 보여 주는 적 공격 범위: 받는 피해 배율(취약, 겔레온)까지 반영
	FBWDuelSide TargetSide;
	PossessedBWCharacter->ApplyStatusesToDuelSide(TargetSide);
	TargetSide = UBWDimensionRules::ApplyToPlayerSide(Battlefield, TargetSide);
	const FBWDuelSide EnemySide = Enemy->MakeEnemyDuelSide();
	const float Mult = EnemySide.DamageMultiplier * TargetSide.IncomingDamageMultiplier;
	const FBWDiceRange Shown(FMath::FloorToInt(EnemySide.SkillRange.Min * Mult), FMath::FloorToInt(EnemySide.SkillRange.Max * Mult));
	const FBWEnemyPattern* Action = Enemy->GetCurrentAction();

	Widget->Setup(PossessedBWCharacter, Enemy->EnemyDisplayName, Action ? Action->ActionName : FString(TEXT("공격")), Shown, MoveTemp(OnChosen));
	Widget->AddToViewport(550);
	Prompt(FString::Printf(TEXT("%s의 공격 - 마나 가드에 걸 MP를 고르세요"), *Enemy->EnemyDisplayName), 0.0f, EBWPromptStyle::Highlight);
}

void ABWPlayerController::PlayEnemyDuelWithGuard(ABWCharacter* Enemy, int32 ShieldMP, TFunction<void()> OnFinished)
{
	FBWDuelSide EnemySide;
	FBWDuelResult Result;
	if (!ComputeEnemyStrike(Enemy, EnemySide, Result))
	{
		if (OnFinished) OnFinished();
		return;
	}

	FBWDuelPresentation Show;
	Show.LeftName = TEXT("나");
	Show.LeftSkill = DescribePlayerDefense(PossessedBWCharacter);
	if (PossessedBWCharacter->Defense.Type == EDefenseType::MagicShield) Show.LeftSkill = ShieldMP > 0 ? FString::Printf(TEXT("마나 가드 (MP %d)"), ShieldMP) : FString(TEXT("가드 안 함"));
	Show.bLeftRolls = false;
	Show.RightName = Enemy->EnemyDisplayName;
	const FBWEnemyPattern* Action = Enemy->GetCurrentAction();
	Show.RightSkill = Action ? Action->ActionName : TEXT("공격");
	Show.RightRange = EnemySide.SkillRange;
	Show.RightRoll = Result.AttackerRoll;
	Show.Winner = 1;
	Show.Damage = Result.FinalDamage;
	Show.bCritical = Result.bCritical;
	// 적의 공격 수치 → 내 방어 수치 → 가운데에 최종 피해
	const FBWPlannedHit Hit = PlanDuelHit(PossessedBWCharacter, Result.FinalDamage, false, ShieldMP);
	FillHitPresentation(Show, Hit, true);

	TWeakObjectPtr<ABWCharacter> WeakEnemy(Enemy);
	ShowDuel(Show, [this, WeakEnemy, EnemySide, Result, Hit, OnFinished]()
	{
		ABWCharacter* E = WeakEnemy.Get();
		if (!E || !PossessedBWCharacter)
		{
			if (OnFinished) OnFinished();
			return;
		}
		PlayLungeThen(E, PossessedBWCharacter,
			[this, WeakEnemy, EnemySide, Result, Hit]() { if (WeakEnemy.IsValid()) ApplyEnemyStrikeOutcome(WeakEnemy.Get(), EnemySide, Result, Hit); },
			OnFinished);
	});
}

void ABWPlayerController::ApplyEnemyStrikeOutcome(ABWCharacter* Enemy, const FBWDuelSide& EnemySide, const FBWDuelResult& Result, const FBWPlannedHit& Hit)
{
	ABWCharacter* PlayerChar = PossessedBWCharacter;
	if (!Enemy || !PlayerChar || PlayerChar->CurrentHP <= 0) return;

	// 합 화면에서 보여 준 그대로 반영 (타이가 과부하의 방어도 무시도 PlanDuelHit에서 정함)
	const bool bIgnoreBlock = Hit.bIgnoreBlock;
	const FBWDefenseResult Defended = PlayerChar->ApplyPlannedHit(Hit);
	PlayImpactFeedback(Enemy, PlayerChar, Defended, Hit.HPLoss, Result.bCritical);
	ReflectThorns(Enemy, Defended.DamageAfterDefense);

	if (EnemySide.AppliedStatus != EStatusEffect::None && PlayerChar->CurrentHP > 0)
	{
		PlayerChar->ApplyStatus(EnemySide.AppliedStatus, EnemySide.StatusDuration, EnemySide.StatusValue);
	}
	if (EnemySide.bPushesTarget) ResolvePushAfterDuel(Enemy, PlayerChar, false);

	// 연출: 적 행동 이펙트와 소리를 내 칸에서
	if (const FBWEnemyPattern* Action = Enemy->GetCurrentAction())
	{
		const FVector FxLoc = PlayerChar->GetActorLocation() + FVector(0, 0, 40);
		BWSpawnFX(GetWorld(), Action->ActionNiagara, Action->ActionEffect, FxLoc);
		if (Action->ActionSound.LoadSynchronous()) UGameplayStatics::PlaySoundAtLocation(GetWorld(), Action->ActionSound.Get(), FxLoc);
	}

	const FString Summary = FString::Printf(TEXT("[적 합] %s 공격 %d → 방어 후 %d%s%s%s"), *Enemy->EnemyDisplayName, Result.FinalDamage, Defended.DamageAfterDefense,
		Result.bCritical ? TEXT(" (치명타)") : TEXT(""), bIgnoreBlock ? TEXT(" (방어도 무시)") : TEXT(""),
		Defended.MPSpent > 0 ? *FString::Printf(TEXT(" · 마나 가드 MP %d로 %d 막음"), Defended.MPSpent, Defended.ShieldBlocked) : TEXT(""));
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s"), *Summary);
	Prompt(Summary, 3.0f, EBWPromptStyle::Result);

	OnEnemyStrike.Broadcast(Result, Enemy);
}

void ABWPlayerController::BeginRound()
{
	// 4-1 턴 시작: 손패를 채우고, 내 속도와 모든 적의 속도를 굴려 공개한다
	PendingDuelTarget = nullptr;

	// 11-3 파트로바스: 매 턴 시작에 내 스킬 최댓값이 커지거나 좁아진다
	if (Battlefield.Dimension == EDimensionType::Patrovas && TargetGridManager)
	{
		const float Mult = UBWDimensionRules::RollPatrovasMultiplier(Battlefield, TargetGridManager->GetCombatRandom());
		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Patrovas: my skill max x%.2f this turn"), Mult);
	}

	if (PossessedBWCharacter)
	{
		if (PossessedBWCharacter->DeckComponent) PossessedBWCharacter->DeckComponent->StartTurn();
		PossessedBWCharacter->BeginTurnResources();
		PossessedBWCharacter->RollSpeed();

		// 기절: 이번 턴에 이동하지 못한다 (속도는 공개만 하고 이동 칸은 0)
		if (PossessedBWCharacter->IsStunned()) PossessedBWCharacter->RemainingMove = 0;
		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Player speed roll: %d (%d~%d)%s"), PossessedBWCharacter->RolledSpeed, PossessedBWCharacter->SpeedDice.Min, PossessedBWCharacter->SpeedDice.Max,
			PossessedBWCharacter->IsStunned() ? TEXT(" - stunned, cannot move") : TEXT(""));
		ShowTurnBanner(TEXT("내 턴"), FString::Printf(TEXT("%d턴 · 속도 %d%s"), PossessedBWCharacter->CombatTurnNumber, PossessedBWCharacter->RolledSpeed,
			PossessedBWCharacter->IsStunned() ? TEXT(" · 기절") : TEXT("")), FLinearColor(0.45f, 0.85f, 1.0f));

		Prompt(PossessedBWCharacter->IsStunned()
			? TEXT("내 턴 - 기절해서 움직일 수 없습니다")
			: FString::Printf(TEXT("내 턴 - 속도 %d. 적을 누르면 다가가고, 맞닿은 적을 누르면 기본 공격 (공격카드로 강화)"), PossessedBWCharacter->RolledSpeed), 0.0f, EBWPromptStyle::Highlight);
	}
	bEnemyPhase = false;

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), FoundActors);
	for (AActor* Actor : FoundActors)
	{
		ABWCharacter* Char = Cast<ABWCharacter>(Actor);
		if (Char && Char->bIsEnemy && Char->CurrentHP > 0)
		{
			Char->RollSpeed();
			UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Enemy %s speed roll: %d (%d~%d)"), *Char->EnemyDisplayName, Char->RolledSpeed, Char->SpeedDice.Min, Char->SpeedDice.Max);
			Char->UpdateEnemyIntent();
			Char->VisualizeEnemyIntent();
		}
	}
}

bool ABWPlayerController::PlaceCharacterAndStartCombat(FIntPoint Coord)
{
	if (!CharacterClassToSpawn || !TargetGridManager || PossessedBWCharacter) return false;
	if (!TargetGridManager->TryPlaceCharacter(Coord, CharacterClassToSpawn)) return false;

	DestroyPlacementPreview();
	if (BattleHUDWidget) BattleHUDWidget->HideTooltip(); // 배치 안내 툴팁이 남지 않게

	// [수정] 마지막 액터가 아니라 방금 배치한 칸의 말을 조종한다 (적이 먼저 생성되어 있어도 안전)
	PossessedBWCharacter = TargetGridManager->GetCharacterAtCoordinate(Coord);
	if (!PossessedBWCharacter) return false;

	// 캐릭터 선택에서 고른 직업·능력치·이어지는 HP (덱도 그 직업으로 다시 불러온다)
	PossessedBWCharacter->ApplyRunState();

	bIsSkillTargetingMode = true;
	Possess(PossessedBWCharacter);

	// [수정] Possess는 시점을 말로 옮기므로, 전투 내내 레벨의 전투 카메라를 유지한다
	if (BattleCamera) SetViewTarget(BattleCamera);
	if (HandWidget) { HandWidget->LinkToPlayerDeck(PossessedBWCharacter); HandWidget->RefreshHand(PossessedBWCharacter->DeckComponent->GetCurrentHand()); }
	if (UDeckComponent* PlayerDeck = PossessedBWCharacter->DeckComponent)
	{
		PlayerDeck->OnCardBurned.RemoveDynamic(this, &ABWPlayerController::HandleCardBurned);
		PlayerDeck->OnCardBurned.AddDynamic(this, &ABWPlayerController::HandleCardBurned);
	}
	if (BattleHUDWidget) BattleHUDWidget->InitializeBattleHUD(PossessedBWCharacter);

	// 전투 시작 자원 (INT·FTH MP 6, HP 직업 방어도)
	PossessedBWCharacter->BeginCombatResources();
	PreloadCombatAssets();

	// 가진 증강 반영 (10장)
	if (UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance()))
	{
		PossessedBWCharacter->ApplyAugments(UBWAugmentRules::Aggregate(GI->GetOwnedAugmentData()));
	}

	// 이번 전투의 국가 기믹 (현재 차원, 과부하 여부)
	InitBattlefieldFromGameInstance();

	// 첫 턴 시작 (손패, 속도 굴림, 적 의도 표시). 겔레온은 카드 종류를 고른 뒤에 시작한다 (11-4)
	if (Battlefield.Dimension == EDimensionType::Geleon) OpenGeleonChoice();
	else BeginRound();

	OnCombatStarted.Broadcast();
	return true;
}

bool ABWPlayerController::TryMovePlayerTo(FIntPoint Target)
{
	if (!PossessedBWCharacter || !TargetGridManager) return false;

	TArray<FIntPoint> Path = TargetGridManager->FindMovePath(PossessedBWCharacter->OwnGridCoordinate, Target, PossessedBWCharacter->RemainingMove, false);
	if (Path.Num() == 0) return false;

	MovePlayerAlongPath(Path);
	return true;
}

void ABWPlayerController::MovePlayerAlongPath(const TArray<FIntPoint>& Path)
{
	if (!PossessedBWCharacter || !TargetGridManager || Path.Num() == 0) return;

	// 한 칸씩 부드럽게 이동 (BWCharacter::Tick이 큐를 소비)
	for (const FIntPoint& Step : Path)
	{
		if (AGridTile* StepTile = TargetGridManager->GetTileAtCoordinate(Step))
		{
			PossessedBWCharacter->AddMovementStep(StepTile->GetActorLocation() + FVector(0, 0, 20));
		}
	}
	PossessedBWCharacter->OwnGridCoordinate = Path.Last();
	PossessedBWCharacter->RemainingMove = FMath::Max(0, PossessedBWCharacter->RemainingMove - Path.Num());
	CancelSelection();

	// 이동 경로는 적과 맞닿는 칸에서 끝난다. [수정] 사용자 결정: 접촉만으로는 남은 이동이 사라지지 않고,
	// 합을 걸었을 때 이동을 모두 쓴다 (기획서 4-2와 다름). 남은 이동이 있으면 이동 모드를 이어 간다
	ABWCharacter* Opponent = TargetGridManager->FindContactOpponent(Path.Last(), false);

	// 걸어가는 연출이 끝난 뒤에 처리 (그동안은 입력을 막는다)
	bActionInProgress = true;
	TWeakObjectPtr<ABWCharacter> WeakOpponent(Opponent);
	WaitForMovementThen(PossessedBWCharacter, [this, WeakOpponent]()
	{
		bActionInProgress = false;
		if (!PossessedBWCharacter) return;
		const int32 Left = PossessedBWCharacter->RemainingMove;
		ABWCharacter* Opp = WeakOpponent.Get();

		// 남은 이동이 있으면 이동 모드 유지 (바닥을 계속 누를 수 있다)
		if (Left > 0)
		{
			bIsMovementMode = true;
			if (TargetGridManager) TargetGridManager->DrawMovementPreview(PossessedBWCharacter->OwnGridCoordinate, Left);
		}
		else if (TargetGridManager)
		{
			TargetGridManager->ClearAllRanges();
		}

		if (!Opp)
		{
			Prompt(Left > 0 ? FString::Printf(TEXT("남은 이동 %d칸 - 계속 이동하거나 적을 누르세요"), Left) : TEXT("이동을 다 썼습니다"), Left > 0 ? 0.0f : 3.0f);
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Contact with %s at %s"), *Opp->EnemyDisplayName, *PossessedBWCharacter->OwnGridCoordinate.ToString());
		OnContactWithEnemy.Broadcast(Opp);

		UDeckComponent* Deck = PossessedBWCharacter->DeckComponent;
		if (Deck && Deck->CanOpenDuel() && !PossessedBWCharacter->IsStunned())
		{
			// 적을 누르면(또는 Q) 기본 공격, 공격카드를 쓰면(클릭, 위로 끌기, 숫자 키) 강화한 공격으로 이 적과 합
			PendingDuelTarget = Opp;
			Prompt(HasUsableAttackCardInHand()
				? FString::Printf(TEXT("%s와 맞닿았습니다! 적을 누르면 기본 공격, 공격카드를 쓰면 강화 공격 (남은 이동 %d칸)"), *Opp->EnemyDisplayName, Left)
				: FString::Printf(TEXT("%s와 맞닿았습니다! 적을 누르거나 Q로 기본 공격 (남은 이동 %d칸)"), *Opp->EnemyDisplayName, Left), 0.0f, EBWPromptStyle::Highlight);
		}
		else Prompt(TEXT("적과 맞닿았지만 이번 턴에는 합을 걸 수 없습니다"), 3.0f, EBWPromptStyle::Warning);
	});
}

AGridTile* ABWPlayerController::GetTileUnderCursor() { return CurrentTileUnderCursor; }
