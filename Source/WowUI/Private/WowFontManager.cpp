#include "WowFontManager.h"
#include "Mpq/MpqManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowFont, Log, All);

FWowFontManager::FWowFontManager()
{
	// Set up the fonts directory in the project's Saved folder
	FontsDirectory = FPaths::ProjectSavedDir() / TEXT("Fonts");
}

FWowFontManager::~FWowFontManager()
{
	Shutdown();
}

bool FWowFontManager::Initialize(FMpqManager* Mpq)
{
	if (!Mpq || !Mpq->IsInitialized())
	{
		UE_LOG(LogWowFont, Error, TEXT("Cannot initialize font manager: Invalid MPQ manager"));
		return false;
	}

	if (bInitialized)
	{
		UE_LOG(LogWowFont, Warning, TEXT("Font manager already initialized"));
		return true;
	}

	UE_LOG(LogWowFont, Log, TEXT("Initializing WoW font manager..."));

	// Create the fonts directory
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*FontsDirectory))
	{
		if (!PlatformFile.CreateDirectoryTree(*FontsDirectory))
		{
			UE_LOG(LogWowFont, Error, TEXT("Failed to create fonts directory: %s"), *FontsDirectory);
			return false;
		}
	}

	// List of WoW 3.3.5 fonts to extract from MPQ
	TArray<TPair<FString, FString>> FontsToExtract = {
		{TEXT("Fonts/FRIZQT__.TTF"), TEXT("FRIZQT__.TTF")},		// Main UI font
		{TEXT("Fonts/ARIALN.TTF"), TEXT("ARIALN.TTF")},			// Smaller text
		{TEXT("Fonts/MORPHEUS.TTF"), TEXT("MORPHEUS.TTF")},		// Headers/titles
		{TEXT("Fonts/SKURRI.TTF"), TEXT("SKURRI.TTF")}			// Combat text
	};

	// Extract fonts from MPQ
	int32 ExtractedCount = 0;
	for (const auto& FontPair : FontsToExtract)
	{
		FString LocalPath = FontsDirectory / FontPair.Value;

		// Skip if already extracted
		if (PlatformFile.FileExists(*LocalPath))
		{
			AvailableFonts.Add(FontPair.Value, LocalPath);
			ExtractedCount++;
			UE_LOG(LogWowFont, Verbose, TEXT("Font already extracted: %s"), *FontPair.Value);
			continue;
		}

		if (ExtractFont(Mpq, FontPair.Key, LocalPath))
		{
			AvailableFonts.Add(FontPair.Value, LocalPath);
			ExtractedCount++;
			UE_LOG(LogWowFont, Log, TEXT("Extracted font: %s -> %s"), *FontPair.Key, *LocalPath);
		}
		else
		{
			UE_LOG(LogWowFont, Warning, TEXT("Failed to extract font: %s"), *FontPair.Key);
		}
	}

	if (ExtractedCount == 0)
	{
		UE_LOG(LogWowFont, Error, TEXT("No fonts could be extracted"));
		return false;
	}

	// Set up the font mappings
	SetupFontMappings();

	bInitialized = true;
	UE_LOG(LogWowFont, Log, TEXT("WoW font manager initialized successfully (%d fonts available)"), ExtractedCount);
	UE_LOG(LogWowFont, Warning, TEXT("Note: TTF fonts extracted to %s may need system installation for proper loading"), *FontsDirectory);
	return true;
}

void FWowFontManager::Shutdown()
{
	if (bInitialized)
	{
		FontMappings.Empty();
		AvailableFonts.Empty();
		bInitialized = false;
		UE_LOG(LogWowFont, Log, TEXT("WoW font manager shutdown"));
	}
}

FSlateFontInfo FWowFontManager::GetFont(const FString& WowFontName, int32 Size) const
{
	if (!bInitialized)
	{
		UE_LOG(LogWowFont, Warning, TEXT("Font manager not initialized, returning default font"));
		return FSlateFontInfo();
	}

	// Look up the TTF file for this WoW font name
	const FString* TtfFileName = FontMappings.Find(WowFontName);
	if (!TtfFileName)
	{
		UE_LOG(LogWowFont, Verbose, TEXT("Unknown WoW font name '%s', using default"), *WowFontName);
		return GetDefaultFont(Size);
	}

	// Find the actual file path
	const FString* FilePath = AvailableFonts.Find(*TtfFileName);
	if (!FilePath)
	{
		UE_LOG(LogWowFont, Warning, TEXT("TTF file '%s' not available, using default"), **TtfFileName);
		return GetDefaultFont(Size);
	}

	return CreateFontFromFile(*FilePath, Size);
}

FSlateFontInfo FWowFontManager::GetDefaultFont(int32 Size) const
{
	if (!bInitialized)
	{
		return FSlateFontInfo();
	}

	// Try to use FRIZQT__ as the default font (main WoW UI font)
	const FString* FrizqtPath = AvailableFonts.Find(TEXT("FRIZQT__.TTF"));
	if (FrizqtPath)
	{
		return CreateFontFromFile(*FrizqtPath, Size);
	}

	// Fallback to any available font
	for (const auto& FontPair : AvailableFonts)
	{
		return CreateFontFromFile(FontPair.Value, Size);
	}

	// Last resort: engine default font
	return FSlateFontInfo();
}

bool FWowFontManager::ExtractFont(FMpqManager* Mpq, const FString& MpqPath, const FString& LocalPath)
{
	TArray<uint8> FontData;
	if (!Mpq->ReadFile(MpqPath, FontData))
	{
		return false;
	}

	if (FontData.IsEmpty())
	{
		UE_LOG(LogWowFont, Warning, TEXT("Font file is empty: %s"), *MpqPath);
		return false;
	}

	// Write the TTF data to the local file
	if (!FFileHelper::SaveArrayToFile(FontData, *LocalPath))
	{
		UE_LOG(LogWowFont, Error, TEXT("Failed to write font file: %s"), *LocalPath);
		return false;
	}

	return true;
}

FSlateFontInfo FWowFontManager::CreateFontFromFile(const FString& FilePath, int32 Size) const
{
	// Create a font info using UE5's standard approach
	// Try to load as a system font first, fallback to file path
	FSlateFontInfo FontInfo;

	// Use the base filename as the font name (e.g., "FRIZQT__" from "FRIZQT__.TTF")
	FString FontName = FPaths::GetBaseFilename(FilePath);

	// Set up the font info - UE5 will handle loading
	FontInfo.TypefaceFontName = FName(*FontName);
	FontInfo.Size = Size;

	// If the system font doesn't work, UE5 will fallback to default fonts
	// The TTF files extracted to Saved/Fonts may need to be installed system-wide
	// or we may need to use a different approach like UFont assets

	return FontInfo;
}

void FWowFontManager::SetupFontMappings()
{
	// Map common WoW font object names to TTF files
	// These are the names used in FrameXML and Lua code

	// FRIZQT__ is the main UI font used for most interface elements
	FontMappings.Add(TEXT("GameFontNormal"), TEXT("FRIZQT__.TTF"));
	FontMappings.Add(TEXT("GameFontNormalSmall"), TEXT("FRIZQT__.TTF"));
	FontMappings.Add(TEXT("GameFontNormalLarge"), TEXT("FRIZQT__.TTF"));
	FontMappings.Add(TEXT("GameFontHighlight"), TEXT("FRIZQT__.TTF"));
	FontMappings.Add(TEXT("GameFontHighlightSmall"), TEXT("FRIZQT__.TTF"));
	FontMappings.Add(TEXT("GameFontHighlightLarge"), TEXT("FRIZQT__.TTF"));
	FontMappings.Add(TEXT("GameFontDisable"), TEXT("FRIZQT__.TTF"));
	FontMappings.Add(TEXT("GameFontDisableSmall"), TEXT("FRIZQT__.TTF"));
	FontMappings.Add(TEXT("GameFontWhite"), TEXT("FRIZQT__.TTF"));
	FontMappings.Add(TEXT("GameFontRed"), TEXT("FRIZQT__.TTF"));
	FontMappings.Add(TEXT("GameFontGreen"), TEXT("FRIZQT__.TTF"));
	FontMappings.Add(TEXT("SystemFont"), TEXT("FRIZQT__.TTF"));

	// ARIALN for smaller text and tooltips
	FontMappings.Add(TEXT("GameTooltipText"), TEXT("ARIALN.TTF"));
	FontMappings.Add(TEXT("GameTooltipTextSmall"), TEXT("ARIALN.TTF"));
	FontMappings.Add(TEXT("TooltipFont"), TEXT("ARIALN.TTF"));
	FontMappings.Add(TEXT("QuestFont"), TEXT("ARIALN.TTF"));
	FontMappings.Add(TEXT("QuestFontNormalSmall"), TEXT("ARIALN.TTF"));

	// MORPHEUS for headers and titles
	FontMappings.Add(TEXT("GameFontNormalHuge"), TEXT("MORPHEUS.TTF"));
	FontMappings.Add(TEXT("GameFontHighlightHuge"), TEXT("MORPHEUS.TTF"));
	FontMappings.Add(TEXT("ErrorFont"), TEXT("MORPHEUS.TTF"));
	FontMappings.Add(TEXT("ZoneTextFont"), TEXT("MORPHEUS.TTF"));
	FontMappings.Add(TEXT("SubZoneTextFont"), TEXT("MORPHEUS.TTF"));

	// SKURRI for combat text and special effects
	FontMappings.Add(TEXT("CombatTextFont"), TEXT("SKURRI.TTF"));
	FontMappings.Add(TEXT("ChatFont"), TEXT("SKURRI.TTF"));

	UE_LOG(LogWowFont, Log, TEXT("Set up %d font mappings"), FontMappings.Num());
}

void FWowFontManager::RegisterFontMapping(const FString& WowFontName, const FString& InheritsFrom, int32 DefaultSize)
{
	if (!bInitialized)
	{
		UE_LOG(LogWowFont, Warning, TEXT("Cannot register font mapping: font manager not initialized"));
		return;
	}

	// If this font inherits from another, copy its mapping
	if (!InheritsFrom.IsEmpty())
	{
		const FString* ParentTtf = FontMappings.Find(InheritsFrom);
		if (ParentTtf)
		{
			FontMappings.Add(WowFontName, *ParentTtf);
			UE_LOG(LogWowFont, Verbose, TEXT("Registered font mapping: %s -> %s (inherited from %s)"),
				*WowFontName, **ParentTtf, *InheritsFrom);
		}
		else
		{
			// Parent not found, use default font
			FontMappings.Add(WowFontName, TEXT("FRIZQT__.TTF"));
			UE_LOG(LogWowFont, Warning, TEXT("Font %s inherits from unknown font %s, using default"),
				*WowFontName, *InheritsFrom);
		}
	}
	else
	{
		// No inheritance, use default font
		FontMappings.Add(WowFontName, TEXT("FRIZQT__.TTF"));
		UE_LOG(LogWowFont, Verbose, TEXT("Registered font mapping: %s -> FRIZQT__.TTF (no inheritance)"), *WowFontName);
	}
}