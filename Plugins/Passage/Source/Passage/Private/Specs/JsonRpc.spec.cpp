#include "JsonRpc.h"
#include "CoreMinimal.h"

BEGIN_DEFINE_SPEC(FJsonRpcSpec, "Passage.JsonRpc",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)

	// we need a bespoke remote handler to handle this thing
class FEchoHandler final : public FJsonRpcHandler
{
public:
	virtual TSharedFuture<FJsonRpcResponse> Handle(
		FString Method, TSharedPtr<FJsonValue> Params) override
	{
		TPromise<FJsonRpcResponse> Promise;

		Promise.SetValue({
			false,
			Params,
			nullptr,
		});

		return Promise.GetFuture().Share();
	}
};

TSharedPtr<FJsonRpc> NotifyLocal;
TSharedPtr<FJsonRpc> NotifyRemote;

END_DEFINE_SPEC(FJsonRpcSpec)
void FJsonRpcSpec::Define()
{
	Describe("Call()", [this]()
		{
			It("should return boolean", EAsyncExecution::ThreadPool,
				[this]()
				{
					const auto Pair = FJsonRpcPairedChannel::Create();
					const auto Remote = MakeShared<FJsonRpc>(Pair.Remote, MakeShared<FEchoHandler>());
					const auto Local = MakeShared<FJsonRpc>(Pair.Local, MakeShared<FJsonRpcEmptyHandler>());
					
					auto Future = Local->Call(
						"anything",
						MakeShared <FJsonValueBoolean>(true)
					);
					if(Future.WaitFor({0,0,1}))
					{
						const auto Response = Future.Get();

						TestFalse("Response.IsError", Response.IsError);
						TestTrue("Result->AsBool()", Response.Result->AsBool());
					} else
					{
						TestFalse("Wait timed out", true);
					}

					Future = Local->Call(
						"anythingElse",
						MakeShared <FJsonValueBoolean>(false)
					);
					if(Future.WaitFor({ 0,0,1 }))
					{
						const auto Response = Future.Get();

						TestFalse("Response.IsError", Response.IsError);
						TestFalse("Result->AsBool()", Response.Result->AsBool());
					}
					else
					{
						TestFalse("Wait timed out", true);
					}
				});

			It("should return number", EAsyncExecution::ThreadPool,
				[this]()
				{
					const auto Pair = FJsonRpcPairedChannel::Create();
					const auto Remote = MakeShared<FJsonRpc>(Pair.Remote, MakeShared<FEchoHandler>());
					const auto Local = MakeShared<FJsonRpc>(Pair.Local, MakeShared<FJsonRpcEmptyHandler>());

					const auto Future = Local->Call("anything", MakeShared<FJsonValueNumber>(42));
					if(Future.WaitFor({0,0,1}))
					{
						const auto Response = Future.Get();

						TestFalse("Response.IsError", Response.IsError);
						const int32 Number = Response.Result->AsNumber();
						TestEqual("Result->AsNumber()", 42, Number);
					}
					else
					{
						TestFalse("Wait timed out", true);
					}
				});

			It("should return array", EAsyncExecution::ThreadPool, 
				[this]()
				{
					const auto Pair = FJsonRpcPairedChannel::Create();
					const auto Remote = MakeShared<FJsonRpc>(Pair.Remote, MakeShared<FEchoHandler>());
					const auto Local = MakeShared<FJsonRpc>(Pair.Local, MakeShared<FJsonRpcEmptyHandler>());

					const auto Reader = TJsonReaderFactory<>::Create(
						TEXT(R"(["a", "b", "c"])"));
					TSharedPtr<FJsonValue> Value;
					FJsonSerializer::Deserialize(Reader, Value);
					auto Array = Value->AsArray();
					
					const auto Future = Local->Call("anything",
						MakeShared<FJsonValueArray>(Array));
					if (Future.WaitFor({ 0,0,1 }))
					{
						const auto Response = Future.Get();
						const auto Result = Response.Result->AsArray();
						TestFalse("Response.IsError", Response.IsError);
						TestEqual("Result[0]->AsString()", 
							TEXT("a"), Result[0]->AsString());
						TestEqual("Result[1]->AsString()",
							TEXT("b"), Result[1]->AsString());
						TestEqual("Result[2]->AsString()",
							TEXT("c"), Result[2]->AsString());
					}
					else
					{
						TestFalse("Wait timed out", true);
					}
				});

			It("should return object", EAsyncExecution::ThreadPool,
				[this]()
				{
					const auto Pair = FJsonRpcPairedChannel::Create();
					const auto Remote = MakeShared<FJsonRpc>(Pair.Remote, MakeShared<FEchoHandler>());
					const auto Local = MakeShared<FJsonRpc>(Pair.Local, MakeShared<FJsonRpcEmptyHandler>());

					const auto Reader = TJsonReaderFactory<>::Create(
						TEXT(R"({"a": "b", "c": "d"})"));
					TSharedPtr<FJsonValue> Value;
					FJsonSerializer::Deserialize(Reader, Value);
					auto Object = Value->AsObject();

					const auto Future = Local->Call("anything",
						MakeShared<FJsonValueObject>(Object));
					if (Future.WaitFor({ 0,0,1 }))
					{
						const auto Response = Future.Get();
						const auto Result = Response.Result->AsObject();
						TestFalse("Response.IsError", Response.IsError);
						TestEqual(R"(Result->Values["a"]->AsString())",
							Result->Values["a"]->AsString(), TEXT("b"));
						TestEqual(R"(Result->Values["b"]->AsString())",
							Result->Values["c"]->AsString(), TEXT("d"));
					}
					else
					{
						TestFalse("Wait timed out", true);
					}
				});

			LatentIt("should notify delegate", {0,0,5},
				[this](const FDoneDelegate& Done)
				{
					const auto Pair = FJsonRpcPairedChannel::Create();
					NotifyRemote = MakeShared<FJsonRpc>(Pair.Remote,
						MakeShared<FJsonRpcEmptyHandler>());
					NotifyLocal = MakeShared<FJsonRpc>(Pair.Local,
						MakeShared<FJsonRpcEmptyHandler>());

					NotifyLocal->OnNotify().AddLambda(
						[this, Done](const FString& Method, const TSharedPtr<FJsonValue> Params)
						{
							TestEqual("Method", Method, "methodName");
							const auto ParamValue = Params->AsBool();
							TestTrue("Params", ParamValue);
							Done.Execute();
						});

					NotifyRemote->Notify("methodName", MakeShared<FJsonValueBoolean>(true));
				});

			It("should close without crashing", [this]()
				{
					const auto Pair = FJsonRpcPairedChannel::Create();
					const auto Local = MakeShared<FJsonRpc>(Pair.Local,
						MakeShared<FJsonRpcEmptyHandler>());

					// We intentionally don't make a remote because we're just
					// going to send messages down an empty channel, into a
					// black hole. This will leave any calls pending.

					Local->Call("Doesn't Matter", MakeShared<FJsonValueNull>());

					Local->Close();
				});
		});
}