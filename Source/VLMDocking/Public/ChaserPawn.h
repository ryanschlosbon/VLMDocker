// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "TimerManager.h"        // for FTimerHandle
#include "Http.h"                // for FHttpRequestPtr
#include "ChaserPawn.generated.h"

UCLASS()
class VLMDOCKING_API AChaserPawn : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AChaserPawn();
	
	// Expose a static mesh component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chaser")
	UStaticMeshComponent* ChaserMesh;
    
	// Scene capture component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vision")
	USceneCaptureComponent2D* ForwardCapture;
	UPROPERTY(VisibleAnywhere)
	USceneCaptureComponent2D* LeftCapture; // Dock port
	UPROPERTY(VisibleAnywhere)
	USceneCaptureComponent2D* RightCapture; 
	UPROPERTY(VisibleAnywhere)
	USceneCaptureComponent2D* DownCapture;
	UPROPERTY(VisibleAnywhere)
	USceneCaptureComponent2D* UpCapture;
	UPROPERTY(VisibleAnywhere)
	USceneCaptureComponent2D* BackwardCapture;

	// Docking port reference
	UPROPERTY(EditAnywhere, Category="Docking")
	AActor* DockingPort;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called every second to capture & send forward-camera image
	void CaptureAndSendAll();

	// HTTP response handler
	void OnInferenceResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	// Current language command to send
	FString CurrentCommand = TEXT("align with port");

	// Timer handle for recurring capture
	FTimerHandle CaptureTimerHandle;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// Translation
	void SetCommandForward();      // +X
	void SetCommandBackward();     // –X
	void SetCommandRight();        // +Y
	void SetCommandLeft();         // –Y
	void SetCommandUp();           // +Z
	void SetCommandDown();         // –Z

	// Rotation (2 axes)
	void SetCommandYawCW();        // yaw + about Z
	void SetCommandYawCCW();       // yaw – about Z
	void SetCommandPitchUp();      // pitch + about Y
	void SetCommandPitchDown();    // pitch – about Y

	// Semantic commands (optional)
	void SetCommandAlign();
	void SetCommandHold();

};
