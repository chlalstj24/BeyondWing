#include "BWCardWidget.h"
#include "BWHandWidget.h"
#include "BWPlayerController.h"
#include "InputCoreTypes.h"
#include "Components/EditableTextBox.h"

void UBWCardWidget::SetHovered(bool bInHovered)
{
	if (bIsHovered != bInHovered)
	{
		bIsHovered = bInHovered;
		OnHoverStateChanged(bIsHovered);

		if (OwningHand)
		{
			if (bIsHovered)
			{
				OwningHand->OnCardHovered(this);
			}
			else
			{
				OwningHand->OnCardUnhovered(this);
			}
		}
	}
}

void UBWCardWidget::SetSelected()
{
	UE_LOG(LogTemp, Warning, TEXT("[BWCardWidget] SetSelected Called for Index: %d (%s)"), HandIndex, *CardData.CardName);
	// [추가] 숫자 키로 집은 카드를 놓는 클릭이면 무시 (그 클릭으로 아래 카드까지 쓰이지 않게)
	if (OwningHand && OwningHand->ShouldIgnoreCardClick()) return;
	ABWPlayerController* PC = Cast<ABWPlayerController>(GetOwningPlayer());
	if (PC)
	{
		PC->SelectCard(HandIndex);
	}
}

UTexture2D* UBWCardWidget::GetCardTexture(TSoftObjectPtr<UTexture2D> SoftPtr)
{
	if (SoftPtr.IsNull()) 
	{
		return nullptr;
	}

	UTexture2D* LoadedTex = SoftPtr.Get();
	if (!LoadedTex)
	{
		LoadedTex = SoftPtr.LoadSynchronous();
	}

	return LoadedTex;
}

FReply UBWCardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// C++ 판정을 비우고 블루프린트 버튼이 처리하도록 함
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FText UBWCardWidget::GetCostText() const
{
	FString Text = CardData.CardKind == ECardKind::Effect ? TEXT("효과")
		: FString::Printf(TEXT("%s%d~%d"), bShowAsBoost ? TEXT("+") : TEXT(""), CardData.SkillMin, CardData.SkillMax);
	if (CardData.MPCost > 0) Text += FString::Printf(TEXT(" · MP %d"), CardData.MPCost);
	return FText::FromString(Text);
}

FText UBWCardWidget::GetKeywordText() const
{
	TArray<FString> Parts;
	auto StatusName = [](EStatusEffect Status) -> const TCHAR*
	{
		switch (Status)
		{
		case EStatusEffect::Stun:       return TEXT("기절");
		case EStatusEffect::Slow:       return TEXT("둔화");
		case EStatusEffect::Weak:       return TEXT("약화");
		case EStatusEffect::Vulnerable: return TEXT("취약");
		case EStatusEffect::Bleed:      return TEXT("출혈");
		case EStatusEffect::Confusion:  return TEXT("혼란");
		case EStatusEffect::Focus:      return TEXT("집중");
		default:                        return TEXT("");
		}
	};
	if (CardData.AppliedStatus != EStatusEffect::None) Parts.Add(FString::Printf(TEXT("%s %d"), StatusName(CardData.AppliedStatus), CardData.StatusDuration));
	if (CardData.bPushesTarget) Parts.Add(TEXT("밀기"));
	if (CardData.EffectHeal > 0) Parts.Add(FString::Printf(TEXT("회복 %d"), CardData.EffectHeal));
	if (CardData.EffectBlock > 0) Parts.Add(FString::Printf(TEXT("방어도 %d"), CardData.EffectBlock));
	if (CardData.EffectMove > 0) Parts.Add(FString::Printf(TEXT("이동 +%d"), CardData.EffectMove));
	if (CardData.EffectMP > 0) Parts.Add(FString::Printf(TEXT("MP +%d"), CardData.EffectMP));
	if (CardData.EffectDraw > 0) Parts.Add(FString::Printf(TEXT("뽑기 %d"), CardData.EffectDraw));
	if (CardData.EffectAttackMaxBonus > 0) Parts.Add(FString::Printf(TEXT("최댓값 +%d"), CardData.EffectAttackMaxBonus));
	if (CardData.SelfStatus != EStatusEffect::None) Parts.Add(StatusName(CardData.SelfStatus));
	return FText::FromString(Parts.Num() > 0 ? FString::Join(Parts, TEXT(" · ")) : FString(TEXT("-")));
}

void UBWCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// [수정] 예전 '피해량' 칸은 이제 핵심 효과 칸 (피해는 합에서 굴린 숫자라 카드에 고정값이 없다)
	if (UEditableTextBox* Label = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("피해이름"))))
	{
		Label->SetText(FText::FromString(TEXT("효과")));
	}
}
