// Fill out your copyright notice in the Description page of Project Settings.


#include "ChaserPawn.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"
#include "RenderUtils.h"            // for FRenderTarget
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "ImageUtils.h"             // for CompressImageArray
#include "Http.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "DrawDebugHelpers.h"       // optional for debug

// Sets default values
AChaserPawn::AChaserPawn()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create mesh
	ChaserMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChaserMesh"));
	RootComponent = ChaserMesh;

	// Create vision capture
	ForwardCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("ForwardCapture"));
	ForwardCapture->SetupAttachment(RootComponent);
	ForwardCapture->SetRelativeLocation(FVector(100, 0, 0)); // e.g., 100 cm forward
	ForwardCapture->TextureTarget = nullptr; // assign in Blueprint

	LeftCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("LeftCapture"));
	LeftCapture->SetupAttachment(RootComponent);
	LeftCapture->SetRelativeLocation(FVector(0, -100, 0));     // 100 cm to the left
	LeftCapture->SetRelativeRotation(FRotator(0, -90, 0));    // yaw –90° to look left
	LeftCapture->TextureTarget = nullptr; // assign a second Render Target in Blueprint

	RightCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("RightCapture"));
	RightCapture->SetupAttachment(RootComponent);
	RightCapture->SetRelativeLocation(FVector(0, 100, 0));     // 100 cm to the right
	RightCapture->SetRelativeRotation(FRotator(0, 90, 0));    // yaw 90° to look right
	RightCapture->TextureTarget = nullptr; // assign a third Render Target in Blueprint
	
	DownCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("DownCapture"));
	DownCapture->SetupAttachment(RootComponent);
	DownCapture->SetRelativeLocation(FVector(0, 0, -50));     // 50 cm below
	DownCapture->SetRelativeRotation(FRotator(-90, 0, 0));   // pitch –90° to look down
	DownCapture->TextureTarget = nullptr; // assign a fourth Render Target

	UpCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("UpCapture"));
	UpCapture->SetupAttachment(RootComponent);
	UpCapture->SetRelativeLocation(FVector(0, -0, 50));     // 50 cm up
	UpCapture->SetRelativeRotation(FRotator(90, 0, 0));    // pitch 90° to look up
	UpCapture->TextureTarget = nullptr; // assign a fifth Render Target in Blueprint

	BackwardCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("BackwardCapture"));
	BackwardCapture->SetupAttachment(RootComponent);
	BackwardCapture->SetRelativeLocation(FVector(-100, 0, 0)); // e.g., 100 cm Backward
	BackwardCapture->SetRelativeRotation(FRotator(90, 180, 0));    // yaw 180° to look backwards
	BackwardCapture->TextureTarget = nullptr; // assign in Blueprint
}

void AChaserPawn::CaptureAndSendAll()
{
	struct Cam { USceneCaptureComponent2D* Comp; FString Id; };
	TArray<Cam> cams = {
		{ForwardCapture, "forward"}, {LeftCapture, "left"}, /*…*/ 
	  };
	for (auto& cam : cams)
	{
		if (!cam.Comp->TextureTarget) continue;
		cam.Comp->CaptureScene();
	}
	// 1) Read pixels from RenderTarget
	if (!ForwardCapture->TextureTarget) return;
	UTextureRenderTarget2D* RT = ForwardCapture->TextureTarget;

	FRenderTarget* RenderTargetResource = RT->GameThread_GetRenderTargetResource();
	TArray<FColor> RawPixels;
	RenderTargetResource->ReadPixels(RawPixels);

	// 2) Compress to PNG
	TArray<uint8> PngData;
	FImageUtils::CompressImageArray(
		RT->SizeX, RT->SizeY, RawPixels, PngData
	);

	// 3) Base64 encode
	FString B64 = FBase64::Encode(PngData);

	// 4) Build JSON payload
	TSharedPtr<FJsonObject> JsonObj = MakeShareable(new FJsonObject());
	JsonObj->SetStringField("camera_id", "forward");
	JsonObj->SetStringField("image_base64", B64);
	JsonObj->SetStringField("command", CurrentCommand); // e.g., set elsewhere

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);

	// 5) HTTP POST
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL("http://127.0.0.1:5001/infer");
	Request->SetVerb("POST");
	Request->SetHeader("Content-Type", "application/json");
	Request->SetContentAsString(OutputString);
	Request->OnProcessRequestComplete().BindUObject(
		this, &AChaserPawn::OnInferenceResponse
	);
	Request->ProcessRequest();
}

void AChaserPawn::OnInferenceResponse(
	FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful
) {
	if (!bWasSuccessful || !Response.IsValid()) {
		UE_LOG(LogTemp, Warning, TEXT("Inference request failed"));
		return;
	}
	// Parse JSON
	TSharedPtr<FJsonObject> JsonResponse;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(
		Response->GetContentAsString()
	);
	if (!FJsonSerializer::Deserialize(Reader, JsonResponse)) {
		UE_LOG(LogTemp, Warning, TEXT("Failed to parse inference JSON"));
		return;
	}
	FString Action = JsonResponse->GetStringField("action");
	float Confidence = JsonResponse->GetNumberField("confidence");
	UE_LOG(LogTemp, Log, TEXT("Action: %s (%.2f)"), *Action, Confidence);

	GetWorld()->GetTimerManager().SetTimer(
		CaptureTimerHandle,
		this, &AChaserPawn::CaptureAndSendAll,
		1.0f,
		false  // one-shot
	);
	// TODO: map Action → actual movement calls (AddMovementInput or SetActorRotation, etc.)
}


// Called when the game starts or when spawned
void AChaserPawn::BeginPlay()
{
	Super::BeginPlay();
	// Capture and send every (1 second) interval
	GetWorld()->GetTimerManager().SetTimer(
		CaptureTimerHandle,
		this, &AChaserPawn::CaptureAndSendAll,
		1.0f,// seconds
		true // loop
		);
}

// Called every frame
void AChaserPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AChaserPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Translation
	PlayerInputComponent->BindKey(EKeys::One,   IE_Pressed, this, &AChaserPawn::SetCommandForward);
	PlayerInputComponent->BindKey(EKeys::Two,   IE_Pressed, this, &AChaserPawn::SetCommandBackward);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AChaserPawn::SetCommandRight);
	PlayerInputComponent->BindKey(EKeys::Four,  IE_Pressed, this, &AChaserPawn::SetCommandLeft);
	PlayerInputComponent->BindKey(EKeys::Five,  IE_Pressed, this, &AChaserPawn::SetCommandUp);
	PlayerInputComponent->BindKey(EKeys::Six,   IE_Pressed, this, &AChaserPawn::SetCommandDown);

	// Yaw rotation
	PlayerInputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &AChaserPawn::SetCommandYawCW);
	PlayerInputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &AChaserPawn::SetCommandYawCCW);

	// Pitch rotation
	PlayerInputComponent->BindKey(EKeys::Nine,  IE_Pressed, this, &AChaserPawn::SetCommandPitchUp);
	PlayerInputComponent->BindKey(EKeys::Zero,  IE_Pressed, this, &AChaserPawn::SetCommandPitchDown);

	// Semantic
	PlayerInputComponent->BindKey(EKeys::Subtract, IE_Pressed, this, &AChaserPawn::SetCommandAlign);
	PlayerInputComponent->BindKey(EKeys::Equals,IE_Pressed, this, &AChaserPawn::SetCommandHold);
}

