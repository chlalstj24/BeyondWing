#include "BWCardWidget.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/OverlaySlot.h"
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
	// [수정] 동그라미 칸에는 MP만 (예전엔 "2~5 · MP 2"가 들어가 이름 칸까지 덮었다)
	return FText::FromString(CardData.MPCost > 0 ? FString::FromInt(CardData.MPCost) : FString(TEXT("-")));
}

void UBWCardWidget::InitCardUI()
{
	K2_InitCardUI();
	// '기타' 줄(옛 보조 스탯 칸)을 공격 범위 칸으로 쓴다
	const bool bAttack = CardData.CardKind == ECardKind::Attack;
	if (UEditableTextBox* Label = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("기타이름"))))
	{
		Label->SetText(FText::FromString(bAttack ? TEXT("공격") : TEXT("")));
	}
	if (UEditableTextBox* Value = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("기타수치"))))
	{
		Value->SetText(FText::FromString(bAttack ? FString::Printf(TEXT("%s%d~%d"), bShowAsBoost ? TEXT("+") : TEXT(""), CardData.SkillMin, CardData.SkillMax) : FString()));
	}

	// 글이 길면 글꼴을 줄여 칸 안에 넣는다 (같은 카드를 다시 그려도 줄어들지 않게 크기를 직접 정한다)
	const int32 KeywordLength = GetKeywordText().ToString().Len();
	if (UEditableTextBox* Keyword = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("피해량수치"))))
	{
		Keyword->WidgetStyle.TextStyle.Font.Size = KeywordLength > 13 ? 12.0f : KeywordLength > 8 ? 15.0f : 18.75f;
		Keyword->SynchronizeProperties();
	}
	if (UMultiLineEditableTextBox* Desc = Cast<UMultiLineEditableTextBox>(GetWidgetFromName(TEXT("스킬설명"))))
	{
		// 설명 칸은 글 길이만큼 늘어나는 설정이라, 카드 폭에 묶고 줄을 넘긴다
		if (UOverlaySlot* DescSlot = Cast<UOverlaySlot>(Desc->Slot))
		{
			DescSlot->SetHorizontalAlignment(HAlign_Fill);
			DescSlot->SetPadding(FMargin(31.0f, 335.0f, 31.0f, 0.0f));
		}
		Desc->SetAutoWrapText(true);
		Desc->WidgetStyle.TextStyle.Font.Size = CardData.Description.Len() > 50 ? 10.0f : 11.25f;
		Desc->SynchronizeProperties();
	}
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
	// [추가] 삶의 방식 카드 수치 (강화하면 숫자가 바로 바뀌어 보인다)
	if (CardData.EffectEvadeBonus > 0.0f) Parts.Add(FString::Printf(TEXT("회피 +%d%%"), FMath::RoundToInt(CardData.EffectEvadeBonus * 100.0f)));
	if (CardData.EffectDefenseBonus > 0) Parts.Add(FString::Printf(TEXT("방어 +%d"), CardData.EffectDefenseBonus));
	if (CardData.EffectCounter > 0) Parts.Add(FString::Printf(TEXT("반격 %d"), CardData.EffectCounter));
	if (CardData.EffectDark > 0) Parts.Add(FString::Printf(TEXT("어둠 +%d"), CardData.EffectDark));
	if (CardData.CritChanceBonus > 0.0f) Parts.Add(CardData.CritChanceBonus >= 1.0f ? FString(TEXT("확정 치명타")) : FString::Printf(TEXT("치명타 +%d%%"), FMath::RoundToInt(CardData.CritChanceBonus * 100.0f)));
	// 한 줄 칸이라 둘까지만 적는다. 나머지는 카드 설명에 있다
	if (Parts.Num() > 2) Parts.SetNum(2);
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
