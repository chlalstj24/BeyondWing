#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BWBattleHUDWidget.generated.h"

UCLASS()
class BEYONDWING_API UBWBattleHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// C++에서 안전하게 덱 컴포넌트를 가져오는 함수
	UFUNCTION(BlueprintCallable, Category = "Combat|HUD")
	class UDeckComponent* GetPlayerDeckComponent();

	// 턴 종료를 C++에서 직접 실행
	UFUNCTION(BlueprintCallable, Category = "Combat|HUD")
	void RequestEndTurn();

	// 캐릭터 데이터를 받아 HUD 초기화
	UFUNCTION(BlueprintCallable, Category = "Combat|HUD")
	void InitializeBattleHUD(class ABWCharacter* InCharacter);

protected:
	virtual void NativeConstruct() override;

	// AP가 바뀔 때마다 블루프린트에서 호출될 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|HUD")
	void K2_UpdateAP(int32 CurrentAP, int32 MaxAP);

private:
	UFUNCTION()
	void HandleAPChanged(int32 CurrentAP, int32 MaxAP);
};
