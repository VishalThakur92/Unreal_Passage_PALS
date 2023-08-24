// Copyright Epic Games, Inc. All Rights Reserved.

#include "PassageWebSocketsModule.h"
#include "PassageWebSocketsLog.h"

#if WITH_WEBSOCKETS
#include "PlatformWebSocket.h"
#endif // #if WITH_WEBSOCKETS

DEFINE_LOG_CATEGORY(LogPassageWebSockets);

// FPassageWebSocketsModule
IMPLEMENT_MODULE(FPassageWebSocketsModule, PassageWebSockets);

FPassageWebSocketsModule* FPassageWebSocketsModule::Singleton = nullptr;

/*static*/ FString FPassageWebSocketsModule::BuildUpgradeHeader(const TMap<FString, FString>& Headers)
{
	FString HeaderString;
	for (const auto& OneHeader : Headers)
	{
		HeaderString += FString::Printf(TEXT("%s: %s\r\n"), *OneHeader.Key, *OneHeader.Value);
	}
	return HeaderString;
}

void FPassageWebSocketsModule::StartupModule()
{
	Singleton = this;

#if WITH_WEBSOCKETS
	const FString Protocols[] = {TEXT("ws"), TEXT("wss"), TEXT("v10.stomp"), TEXT("v11.stomp"), TEXT("v12.stomp"), TEXT("xmpp")};

	WebSocketsManager = new FPlatformWebSocketsManager;
	WebSocketsManager->InitWebSockets(Protocols);
#endif
}

void FPassageWebSocketsModule::ShutdownModule()
{
#if WITH_WEBSOCKETS
	if (WebSocketsManager)
	{
		WebSocketsManager->ShutdownWebSockets();
		delete WebSocketsManager;
		WebSocketsManager = nullptr;
	}
#endif

	Singleton = nullptr;
}

FPassageWebSocketsModule& FPassageWebSocketsModule::Get()
{
	return *Singleton;
}

#if WITH_WEBSOCKETS
TSharedRef<IWebSocket> FPassageWebSocketsModule::CreateWebSocket(const FString& Url, const TArray<FString>& Protocols, const TMap<FString, FString>& UpgradeHeaders)
{
	check(WebSocketsManager);

	TArray<FString> ProtocolsCopy = Protocols;
	ProtocolsCopy.RemoveAll([](const FString& Protocol){ return Protocol.IsEmpty(); });	
	TSharedRef<IWebSocket> WebSocket = WebSocketsManager->CreateWebSocket(Url, ProtocolsCopy, UpgradeHeaders);
	OnWebSocketCreated.Broadcast(WebSocket, Protocols, Url);
	
	return WebSocket;
}

TSharedRef<IWebSocket> FPassageWebSocketsModule::CreateWebSocket(const FString& Url, const FString& Protocol, const TMap<FString, FString>& UpgradeHeaders)
{
	TArray<FString> Protocols;
	if (!Protocol.IsEmpty())
	{
		Protocols.Add(Protocol);
	}
	
	return CreateWebSocket(Url, Protocols, UpgradeHeaders);
}
#endif // #if WITH_WEBSOCKETS
