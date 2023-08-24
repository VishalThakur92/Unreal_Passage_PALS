// Copyright Enva Division

#include "VerseConnectionFactory.h"

DEFINE_LOG_CATEGORY(LogVerseConnectionFactory);

UVerseConnection* UVerseConnectionFactory::CreateConnection()
{
    UE_LOG(LogVerseConnectionFactory, Log, TEXT("UVerseConnectionFactory::CreateConnection()"));
    // TODO: set any shared resource properties that need initialization
    return NewObject<UVerseConnection>(this);
}
