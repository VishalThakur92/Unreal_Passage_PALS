#include "ConnectionStatus.h"
#include "ConnectionStatusSpecWrapper.h"


DEFINE_SPEC(FConnectionStatusSpec, "Passage.ConnectionStatus",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)
void FConnectionStatusSpec::Define()
{
	Describe("SetStatus()", [this]() {
		LatentIt("should execute OnStatusChange delegate", 100, [this](const FDoneDelegate& Done) {
			auto Connection = NewObject<UConnectionStatus>();

			auto Dunzo = NewObject<UConnectionStatusSpecWrapper>();
			Dunzo->Delegate = Done;

			Connection->OnStatusChange.AddDynamic(Dunzo, &UConnectionStatusSpecWrapper::StatusDone);

			Connection->SetStatus(EConnectionStatus::Connected);
			}
		);

		LatentIt("should execute OnConnecting delegate", 100, [this](const FDoneDelegate& Done) {
			auto Connection = NewObject<UConnectionStatus>();
			auto Dunzo = NewObject<UConnectionStatusSpecWrapper>();
			Dunzo->Delegate = Done;

			Connection->OnConnecting.AddDynamic(Dunzo, &UConnectionStatusSpecWrapper::Done);

			Connection->SetStatus(EConnectionStatus::Connecting);
			}
		);

		LatentIt("should execute OnConnected delegate", 100, [this](const FDoneDelegate& Done) {
			auto Connection = NewObject<UConnectionStatus>();
			auto Dunzo = NewObject<UConnectionStatusSpecWrapper>();
			Dunzo->Delegate = Done;

			Connection->OnConnected.AddDynamic(Dunzo, &UConnectionStatusSpecWrapper::Done);

			Connection->SetStatus(EConnectionStatus::Connected);
			}
		);

		LatentIt("should execute OnReconnecting delegate", 100, [this](const FDoneDelegate& Done) {
			auto Connection = NewObject<UConnectionStatus>();
			auto Dunzo = NewObject<UConnectionStatusSpecWrapper>();
			Dunzo->Delegate = Done;

			Connection->OnReconnecting.AddDynamic(Dunzo, &UConnectionStatusSpecWrapper::Done);

			Connection->SetStatus(EConnectionStatus::Reconnecting);
			}
		);

		LatentIt("should execute OnClosed delegate", 100, [this](const FDoneDelegate& Done) {
			auto Connection = NewObject<UConnectionStatus>();
			auto Dunzo = NewObject<UConnectionStatusSpecWrapper>();
			Dunzo->Delegate = Done;

			Connection->OnClosed.AddDynamic(Dunzo, &UConnectionStatusSpecWrapper::Done);

			Connection->SetStatus(EConnectionStatus::Closed);
			}
		);

		LatentIt("should execute OnFailed delegate", 100, [this](const FDoneDelegate& Done) {
			auto Connection = NewObject<UConnectionStatus>();
			auto Dunzo = NewObject<UConnectionStatusSpecWrapper>();
			Dunzo->Delegate = Done;

			Connection->OnFailed.AddDynamic(Dunzo, &UConnectionStatusSpecWrapper::Done);

			Connection->SetStatus(EConnectionStatus::Failed);
			}
		);
		}
	);

	Describe("GetStatus()", [this]() {

		It("should start in Unconnected state", [this]() {
			auto Connection = NewObject<UConnectionStatus>();
			TestEqual("GetStatus()", Connection->GetStatus(), EConnectionStatus::Unconnected);
			}
		);

		It("should reject change to Unconnected status", [this]() {
			auto Connection = NewObject<UConnectionStatus>();
			TestEqual("GetStatus()", Connection->GetStatus(), EConnectionStatus::Unconnected);
			Connection->SetStatus(EConnectionStatus::Connected);
			TestEqual("GetStatus()", Connection->GetStatus(), EConnectionStatus::Connected);
			Connection->SetStatus(EConnectionStatus::Unconnected);
			TestNotEqual("GetStatus()", Connection->GetStatus(), EConnectionStatus::Unconnected);
			}
		);

		It("should accept any status change except Unconnected", [this]() {
			auto Connection = NewObject<UConnectionStatus>();
			Connection->SetStatus(EConnectionStatus::Connected);
			TestEqual("GetStatus()", Connection->GetStatus(), EConnectionStatus::Connected);
			Connection->SetStatus(EConnectionStatus::Reconnecting);
			TestEqual("GetStatus()", Connection->GetStatus(), EConnectionStatus::Reconnecting);
			Connection->SetStatus(EConnectionStatus::Closed);
			TestEqual("GetStatus()", Connection->GetStatus(), EConnectionStatus::Closed);
			Connection->SetStatus(EConnectionStatus::Failed);
			TestEqual("GetStatus()", Connection->GetStatus(), EConnectionStatus::Failed);
			}
		);

	}	
	);
}