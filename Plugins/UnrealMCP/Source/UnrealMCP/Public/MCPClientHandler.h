#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "HAL/PlatformTime.h"
#include <atomic>

class UEpicUnrealMCPBridge;

/**
 * Runnable class that handles a single MCP client connection.
 * Each connection gets its own FMCPClientHandler thread.
 */
class FMCPClientHandler : public FRunnable
{
public:
    FMCPClientHandler(UEpicUnrealMCPBridge* InBridge, FSocket* InClientSocket);
    virtual ~FMCPClientHandler();

    // FRunnable interface
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
    virtual void Exit() override;

    /** Returns true if the handler has finished (client disconnected or error). */
    bool IsFinished() const;

    /** Returns the underlying client socket for forced close on shutdown. */
    FSocket* GetClientSocket() const;

private:
    void ProcessMessage(const FString& Message);
    void SendJsonResponse(const TSharedPtr<FJsonObject>& ResponseObj);
    void SendJsonResponseString(const FString& Response);
    void ProcessUpsertProceduralMesh(const TSharedPtr<FJsonObject>& Params);

    UEpicUnrealMCPBridge* Bridge;
    FSocket* ClientSocket;
    std::atomic<bool> bRunning;
    std::atomic<bool> bFinished;
};
