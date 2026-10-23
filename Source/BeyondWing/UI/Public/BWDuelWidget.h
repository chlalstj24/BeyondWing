#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BWDuelWidget.generated.h"

class UTextBlock;
class UBorder;

// 합 화면에 보여 줄 내용. 왼쪽은 항상 나, 오른쪽은 항상 적
USTRUCT(BlueprintType)
struct FBWDuelPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Duel") FString LeftName;
	UPROPERTY(BlueprintReadOnly, Category = "Duel") FString LeftSkill;      // 카드 이름, 또는 적 선공일 때 내 방어 방식
	UPROPERTY(BlueprintReadOnly, Category = "Duel") FBWDiceRange LeftRange;
	UPROPERTY(BlueprintReadOnly, Category = "Duel") int32 LeftRoll = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Duel") bool bLeftRolls = true;  // 적이 선공인 합에서는 나는 굴리지 않는다

	UPROPERTY(BlueprintReadOnly, Category = "Duel") FString RightName;
	UPROPERTY(BlueprintReadOnly, Category = "Duel") FString RightSkill;
	UPROPERTY(BlueprintReadOnly, Category = "Duel") FBWDiceRange RightRange;
	UPROPERTY(BlueprintReadOnly, Category = "Duel") int32 RightRoll = 0;

	// 0 = 나(왼쪽) 승리, 1 = 적(오른쪽) 승리, -1 = 3연속 동점
	UPROPERTY(BlueprintReadOnly, Category = "Duel") int32 Winner = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Duel") int32 Damage = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Duel") bool bCritical = false;
	UPROPERTY(BlueprintReadOnly, Category = "Duel") int32 TieRerolls = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Duel") bool bRerolled = false;
	UPROPERTY(BlueprintReadOnly, Category = "Duel") FString Extra; // 방어도 무시, 기믹 등 덧붙일 말

	// [추가] 마지막에 가운데로 들어오는 최종 피해. Damage(공격 수치)에서 맞는 쪽의 방어를 뺀 값
	UPROPERTY(BlueprintReadOnly, Category = "Duel") bool bPlayerIsHit = false;   // 맞는 쪽이 나인가 (내 방어가 적용된다)
	UPROPERTY(BlueprintReadOnly, Category = "Duel") FString DefenseText;          // 내 방어 수치 ("-2", "회피!", "실드 -5")
	UPROPERTY(BlueprintReadOnly, Category = "Duel") int32 DefenseReduced = 0;     // 방어·회피·매직실드로 줄어든 양
	UPROPERTY(BlueprintReadOnly, Category = "Duel") bool bEvaded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Duel") int32 FinalDamage = 0;        // 방어 후 피해
	UPROPERTY(BlueprintReadOnly, Category = "Duel") int32 BlockAbsorbed = 0;      // 그중 방어도가 막은 양
	UPROPERTY(BlueprintReadOnly, Category = "Duel") int32 HPLoss = 0;
};

// 합 전용 연출 화면: 양쪽 스킬과 숫자 범위 → 숫자가 굴러가다 멈춤 → 승패와 피해. 끝나면 스스로 닫히고 콜백을 부른다.
// 위젯 블루프린트로 꾸밀 때는 같은 이름의 위젯을 두고, K2_OnDuelPhase에서 애니메이션·이펙트를 붙이면 된다.
UCLASS()
class BEYONDWING_API UBWDuelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_LeftName;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_LeftSkill;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_LeftRoll;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_RightName;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_RightSkill;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_RightRoll;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Result;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Versus;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Final;      // 가운데로 스윽 들어오는 최종 피해
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_FinalNote;  // "공격 11 − 방어 2" 같은 계산식

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") float IntroTime = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") float RollTime = 0.9f;
	// 굴러가는 숫자가 바뀌는 간격: 처음엔 아주 빠르게, 점점 느려지다가 멈춘다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") float FlickerIntervalStart = 0.02f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") float FlickerIntervalEnd = 0.17f;
	// 숫자가 멈출 때 살짝 커졌다 돌아오는 크기와 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") float LandPopScale = 1.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") float LandPopTime = 0.18f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") float HoldTime = 0.8f;   // 양쪽 수치 공개 뒤 머무는 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") float FinalSlideTime = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") float FinalHoldTime = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Duel") FBWDuelPresentation Current;

	// 화면에 띄우고 연출을 시작한다. 끝나면 화면에서 내려가고 OnFinished를 부른다
	void PlayDuel(const FBWDuelPresentation& Data, TFunction<void()> OnFinished);

	// 0 = 소개, 1 = 굴리는 중, 2 = 양쪽 수치 공개, 3 = 최종 피해. 블루프린트에서 연출을 덧붙이는 자리
	UFUNCTION(BlueprintImplementableEvent, Category = "Duel")
	void K2_OnDuelPhase(int32 Phase, const FBWDuelPresentation& Data);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildDefaultLayout();
	void EnterPhase(int32 NewPhase);
	FString RollText(bool bRolls, const FBWDiceRange& Range, int32 FinalRoll, bool bFinal) const;
	void FlickerRolls();
	void UpdateLandPop();
	int32 LastFlickerLeft = INDEX_NONE;
	int32 LastFlickerRight = INDEX_NONE;
	FString MakeFinalNote() const;
	void UpdateFinalSlide();

	TFunction<void()> FinishedCallback;
	int32 CurrentPhase = -1;
	float PhaseTime = 0.0f;
	float FlickerTime = 0.0f;
};
