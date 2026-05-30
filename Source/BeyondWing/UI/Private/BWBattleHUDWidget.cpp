#include "BWBattleHUDWidget.h"
#include "BeyondWing/Core/Public/BWPlayerController.h"
#include "BeyondWing/Character/Public/BWCharacter.h"
#include "BeyondWing/Combat/Public/DeckComponent.h"
#include "Kismet/GameplayStatics.h"

void UBWBattleHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// 스스로 찾지 않고 컨트롤러의 InitializeBattleHUD 호출을 기다림
}

void UBWBattleHUDWidget::HandleAPChanged(int32 CurrentAP, int32 MaxAP)
{
	K2_UpdateAP(CurrentAP, MaxAP);
}

UDeckComponent* UBWBattleHUDWidget::GetPlayerDeckComponent()
{
	// 1. 플레이어 컨트롤러를 통해 현재 조종 중인 캐릭터를 찾음
	ABWPlayerController* PC = Cast<ABWPlayerController>(GetOwningPlayer());
	if (PC && PC->PossessedBWCharacter)
	{
		return PC->PossessedBWCharacter->DeckComponent;
	}

	// 2. 만약 컨트롤러에 없다면 세상에 있는 캐릭터를 직접 찾음 (Fallback)
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), FoundActors);
	if (FoundActors.Num() > 0)
	{
		ABWCharacter* FoundChar = Cast<ABWCharacter>(FoundActors.Last());
		if (FoundChar)
		{
			return FoundChar->DeckComponent;
		}
	}

	return nullptr;
}

void UBWBattleHUDWidget::RequestEndTurn()
{
	UDeckComponent* DeckComp = GetPlayerDeckComponent();
	if (DeckComp)
	{
		DeckComp->EndTurn();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[BWBattleHUDWidget] DeckComponent를 찾을 수 없어 턴을 종료할 수 없습니다."));
	}
}

void UBWBattleHUDWidget::InitializeBattleHUD(ABWCharacter* InCharacter)
{
	if (InCharacter && InCharacter->DeckComponent)
	{
		// AP 신호등 연결 (중복 방지)
		InCharacter->DeckComponent->OnAPChanged.RemoveDynamic(this, &UBWBattleHUDWidget::HandleAPChanged);
		InCharacter->DeckComponent->OnAPChanged.AddDynamic(this, &UBWBattleHUDWidget::HandleAPChanged);
		
		// 현재 수치 즉시 반영
		K2_UpdateAP(InCharacter->DeckComponent->CurrentAP, InCharacter->DeckComponent->MaxAP);
		UE_LOG(LogTemp, Warning, TEXT("[BWBattleHUDWidget] AP HUD Linked Successfully."));
	}
}
