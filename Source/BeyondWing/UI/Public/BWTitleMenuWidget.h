#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BWTitleMenuWidget.generated.h"

UCLASS()
class BEYONDWING_API UBWTitleMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 게임 시작 (지도 생성 후 레벨 이동)
	UFUNCTION(BlueprintCallable, Category = "BW Title")
	void StartNewGame(FName LevelName);

	// 게임 종료
	UFUNCTION(BlueprintCallable, Category = "BW Title")
	void QuitGame();

protected:
	virtual void NativeConstruct() override;
};
