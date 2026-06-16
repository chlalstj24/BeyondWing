#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BWTooltipWidget.generated.h"

class UTextBlock;

UCLASS()
class BEYONDWING_API UBWTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 제목 설정
	UFUNCTION(BlueprintCallable, Category = "Tooltip")
	void SetTooltipTitle(const FString& InTitle);

	// 설명 설정 (플레이스홀더 변환 포함)
	UFUNCTION(BlueprintCallable, Category = "Tooltip")
	void SetTooltipDescription(const FString& RawDescription, int32 Value = 0, int32 Dist = 0, int32 Range = 0);

	// 카드 데이터를 통한 자동 설정
	UFUNCTION(BlueprintCallable, Category = "Tooltip")
	void SetupFromCardData(const FBWCardData& CardData, class ABWCharacter* OwnerChar = nullptr);

	// 적 패턴을 통한 자동 설정
	UFUNCTION(BlueprintCallable, Category = "Tooltip")
	void SetupFromEnemyPattern(const FBWEnemyPattern& Pattern);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip")
	float MaxWrapWidth = 700.0f; // [수정] 줄바꿈 너비 2배 확장 (350 -> 700)

protected:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* TXT_Title;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TXT_Description;

private:
	FString FormatTemplate(const FString& Raw, int32 V, int32 D, int32 R);
};
