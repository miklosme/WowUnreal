#pragma once
#include "CoreMinimal.h"
#include "WowFrameTypes.h"

class FMpqManager;

/**
 * Parses WoW FrameXML files into frame definitions.
 * Handles the XML format used by WoW's Interface/FrameXML/ system.
 */
class WOWUI_API FWowFrameXmlParser
{
public:
	/** Parse an XML file from raw bytes */
	static TArray<FWowXmlDirective> ParseXml(const TArray<uint8>& Data, const FString& FileName);

	/** Load and parse FrameXML from MPQ (Interface/FrameXML/FrameXML.toc) */
	static TArray<FWowXmlDirective> LoadFrameXml(FMpqManager* Mpq);

	/** Parse enum values from string names */
	static EWowFrameType ParseFrameType(const FString& TypeName);
	static EWowAnchorPoint ParseAnchorPoint(const FString& PointName);
	static EWowFrameStrata ParseStrata(const FString& StrataName);
	static EWowDrawLayer ParseDrawLayer(const FString& LayerName);
};
