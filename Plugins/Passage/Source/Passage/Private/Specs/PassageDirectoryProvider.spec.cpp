#include "DoneWrapper.h"
#include "JsonRpc.h"
#include "PassageDirectoryProvider.h"
#include "CoreMinimal.h"

// UE4 JSON module
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"


DEFINE_SPEC(FPassageDirectoryProviderSpec, "Passage.PassageDirectoryProvider",
            EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)

void FPassageDirectoryProviderSpec::Define()
{
	Describe("AddParticipants()", [this]()
	{
		LatentIt("should execute OnParticipantJoined delegate",
		         {0, 0, 1},
		         [this](const FDoneDelegate& Done)
		         {
			         const auto RemoteHandler = MakeShared<FJsonRpcEmptyHandler>();
			         const auto Pair = FJsonRpcPairedChannel::Create();
			         const TSharedPtr<FJsonRpc> Remote = MakeShared<FJsonRpc>(Pair.Remote, RemoteHandler);

			         const auto World = UWorld::CreateWorld(EWorldType::Game, false);

			         const auto Provider = NewObject<UPassageDirectoryProvider>(World);
			         const auto LocalHandler = Provider->GetHandler();
			         const TSharedPtr<FJsonRpc> Local = MakeShared<FJsonRpc>(Pair.Local, LocalHandler);

			         const auto DoneWrapped = NewObject<UDoneWrapper>(World);
			         DoneWrapped->Delegate = Done;
			         Provider->OnParticipantJoined.AddDynamic(
				         DoneWrapped, &UDoneWrapper::Done_UParticipant);

			         const auto Reader = TJsonReaderFactory<>::Create(TEXT(R"(
				[
					{
						"id": "local",
						"isLocal": true,
						"active": true,
						"screenName": "Local Participant",
						"serverLocation": "127.0.0.1",
						"isPublishingMedia": false,
						"data": {
							"agoraToken": "123456789ABCDEF"
						}
					},
					{
						"id": "remote",
						"isLocal": false,
						"active": true,
						"screenName": "Remote Participant",
						"serverLocation": "127.0.0.1",
						"isPublishingMedia": false,
						"data": {
							"agoraToken": "0123456789ABCDEF"
						}
					}
				]
				)"));
			         TSharedPtr<FJsonValue> ParticipantsValue;
			         FJsonSerializer::Deserialize(Reader, ParticipantsValue);

			         const auto Future = Remote->Call("AddParticipants", ParticipantsValue);

			         if (Future.WaitFor({0, 0, 1}))
			         {
				         const auto Response = Future.Get();
				         TestFalse("Received unexpected error response", Response.IsError);
				         if (!Response.IsError)
				         {
							 const auto Participants = 
								 Provider->GetParticipantsOnServer();
							 TestEqual("Participant Id", Participants[0]->Id, "local");
							 TestEqual("Participant Id", Participants[1]->Id, "remote");
				         }
			         }
			         else
			         {
				         TestFalse("Test failed due to Future timeout", true);
			         }

			         Remote->Close();
			         Local->Close();

			         World->DestroyWorld(false);
		         });
	});

	Describe("RemoveParticipants()", [this]()
	{
		LatentIt("should execute OnParticipantLeft delegate",
		         {0, 0, 1},
		         [this](const FDoneDelegate& Done)
		         {
			         const auto RemoteHandler = MakeShared<FJsonRpcEmptyHandler>();
			         const auto Pair = FJsonRpcPairedChannel::Create();
			         const TSharedPtr<FJsonRpc> Remote = MakeShared<FJsonRpc>(Pair.Remote, RemoteHandler);

			         const auto World = UWorld::CreateWorld(EWorldType::Game, false);

			         const auto Provider = NewObject<UPassageDirectoryProvider>(World);
			         const auto LocalHandler = Provider->GetHandler();
			         const TSharedPtr<FJsonRpc> Local = MakeShared<FJsonRpc>(Pair.Local, LocalHandler);

			         const auto DoneWrapped = NewObject<UDoneWrapper>(World);
			         DoneWrapped->Delegate = Done;
			         Provider->OnParticipantLeft.AddDynamic(
				         DoneWrapped, &UDoneWrapper::Done_UParticipant);

			         const auto Reader = TJsonReaderFactory<>::Create(TEXT(R"(
				[
					{
						"id": "local",
						"isLocal": true,
						"active": true,
						"screenName": "Local Participant",
						"serverLocation": "127.0.0.1",
						"isPublishingMedia": false,
						"data": {
							"agoraToken": "123456789ABCDEF"
						}
					},
					{
						"id": "remote",
						"isLocal": false,
						"active": true,
						"screenName": "Remote Participant",
						"serverLocation": "127.0.0.1",
						"isPublishingMedia": false,
						"data": {
							"agoraToken": "0123456789ABCDEF"
						}
					}
				]
)"));
			         TSharedPtr<FJsonValue> Participants;
			         FJsonSerializer::Deserialize(Reader, Participants);

					 auto Future = Remote->Call("AddParticipants", Participants);

					 if (!Future.WaitFor({ 0, 0, 1 }))
					 {
						 TestFalse("Test failed due to Future timeout", true);
						 Remote->Close();
						 Local->Close();
						 World->DestroyWorld(false);
						 return;
					 }

					 const TArray< TSharedPtr<FJsonValue> > IdsArray =
						 TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueString>("remote")};
					 const auto Ids = MakeShared<FJsonValueArray>(IdsArray);

					 Future = Remote->Call("RemoveParticipants", Ids);
		         	 if(!Future.WaitFor({0,0,1}))
					 {
					 	 TestFalse("Test failed due to Future timeout", true);
						 Remote->Close();
						 Local->Close();
						 World->DestroyWorld(false);
						 return;
					 }

		         	 Remote->Close();
					 Local->Close();
					 World->DestroyWorld(false);

		         });
	});

	Describe("UpdateParticipants()", [this]()
	{
		It("should change the Participant property",[this]()
		{
				const auto RemoteHandler = MakeShared<FJsonRpcEmptyHandler>();
				const auto Pair = FJsonRpcPairedChannel::Create();
				const TSharedPtr<FJsonRpc> Remote = MakeShared<FJsonRpc>(Pair.Remote, RemoteHandler);

				const auto World = UWorld::CreateWorld(EWorldType::Game, false);

				const auto Provider = NewObject<UPassageDirectoryProvider>(World);
				const auto LocalHandler = Provider->GetHandler();
				const TSharedPtr<FJsonRpc> Local = MakeShared<FJsonRpc>(Pair.Local, LocalHandler);

				auto Reader = TJsonReaderFactory<>::Create(TEXT(R"(
				[
					{
						"id": "local",
						"isLocal": true,
						"active": true,
						"screenName": "Local Participant",
						"serverLocation": "127.0.0.1",
						"isPublishingMedia": false,
						"data": {
							"agoraToken": "123456789ABCDEF"
						}
					},
					{
						"id": "remote",
						"isLocal": false,
						"active": true,
						"screenName": "Remote Participant",
						"serverLocation": "127.0.0.1",
						"isPublishingMedia": false,
						"data": {
							"agoraToken": "0123456789ABCDEF"
						}
					}
				]
)"));
				TSharedPtr<FJsonValue> ParticipantsValue;
				FJsonSerializer::Deserialize(Reader, ParticipantsValue);

				auto Future = Remote->Call("AddParticipants", ParticipantsValue);

				if (Future.WaitFor({ 0, 0, 1 }))
				{
					const auto Response = Future.Get();
					TestFalse("Received unexpected error response", Response.IsError);
					if (!Response.IsError)
					{
						const auto Participants =
							Provider->GetParticipantsOnServer();
						TestEqual("Participant Id", Participants[0]->Id, "local");
						TestEqual("Participant Id", Participants[1]->Id, "remote");
					}
				}
				else
				{
					TestFalse("Test failed due to Future timeout", true);
				}

				Reader = TJsonReaderFactory<>::Create(TEXT(R"(
				[
					{
						"id": "local",
						"isLocal": true,
						"active": false,
						"screenName": "Local Participant Update",
						"serverLocation": "127.0.0.2",
						"isPublishingMedia": true,
						"data": {
							"agoraToken": "Update"
						}
					},
					{
						"id": "remote",
						"isLocal": false,
						"active": false,
						"screenName": "Remote Participant Update",
						"serverLocation": "127.0.0.2",
						"isPublishingMedia": true,
						"data": {
							"agoraToken": "Update"
						}
					}
				]
)"));
				FJsonSerializer::Deserialize(Reader, ParticipantsValue);

				Future = Remote->Call("UpdateParticipants", ParticipantsValue);

				if (Future.WaitFor({ 0, 0, 1 }))
				{
					const auto Response = Future.Get();
					TestFalse("Received unexpected error response", Response.IsError);
					if (!Response.IsError)
					{
						const auto Participants =
							Provider->GetParticipantsOnServer();
						TestEqual("Participant Id", Participants[0]->Id, "local");
						TestEqual("Participant Active", Participants[0]->Active, false);
						TestEqual("Participant ScreenName", Participants[0]->ScreenName, "Local Participant Update");
						TestEqual("Participant ServerLocation", Participants[0]->ServerLocation, "127.0.0.2");
						TestEqual("Participant IsPublishingMedia", Participants[0]->IsPublishingMedia
							, true);
						TestEqual("Participant Data.agoraToken", Participants[0]->Data["agoraToken"], "update");

						TestEqual("Participant Id", Participants[1]->Id, "remote");
						TestEqual("Participant Active", Participants[1]->Active, false);
						TestEqual("Participant ScreenName", Participants[1]->ScreenName, "Remote Participant Update");
						TestEqual("Participant ServerLocation", Participants[1]->ServerLocation, "127.0.0.2");
						TestEqual("Participant IsPublishingMedia", Participants[1]->IsPublishingMedia, true);
						TestEqual("Participant Data.agoraToken", Participants[1]->Data["agoraToken"], "update");
					}
				}
				else
				{
					TestFalse("Test failed due to Future timeout", true);
				}

				Remote->Close();
				Local->Close();

				World->DestroyWorld(false);


		});
	});

}
