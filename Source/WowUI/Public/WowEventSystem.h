#pragma once
#include "CoreMinimal.h"

/**
 * Maps SMSG opcodes to WoW event names and dispatches events to registered frames.
 */
class WOWUI_API FWowEventSystem
{
public:
	/** Register a frame to receive an event */
	void RegisterEvent(int64 FrameHandle, const FString& EventName);

	/** Unregister a frame from an event */
	void UnregisterEvent(int64 FrameHandle, const FString& EventName);

	/** Unregister a frame from all events */
	void UnregisterAllEvents(int64 FrameHandle);

	/** Fire an event to all registered frames */
	void FireEvent(const FString& EventName, const TArray<FString>& Args = {});

	/** Map a server opcode to a WoW event name */
	static FString OpcodeToEvent(uint16 Opcode);

private:
	/** Event name -> set of registered frame handles */
	TMap<FString, TSet<int64>> EventRegistrations;
};
