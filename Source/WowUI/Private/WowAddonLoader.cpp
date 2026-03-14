#include "WowAddonLoader.h"
#include "Mpq/MpqManager.h"
#include "WowLuaVM.h"
#include "WowFrameXmlParser.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowAddon, Log, All);

FWowTocData FWowAddonLoader::ParseToc(const TArray<uint8>& Data)
{
	FWowTocData Toc;

	FString Content;
	if (Data.Num() >= 3 && Data[0] == 0xEF && Data[1] == 0xBB && Data[2] == 0xBF)
	{
		FUTF8ToTCHAR Conv((const ANSICHAR*)Data.GetData() + 3, Data.Num() - 3);
		Content = FString(Conv.Length(), Conv.Get());
	}
	else
	{
		FUTF8ToTCHAR Conv((const ANSICHAR*)Data.GetData(), Data.Num());
		Content = FString(Conv.Length(), Conv.Get());
	}

	TArray<FString> Lines;
	Content.ParseIntoArrayLines(Lines);

	for (const FString& Line : Lines)
	{
		FString Trimmed = Line.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			continue;
		}

		// Metadata line: ## Key: Value
		if (Trimmed.StartsWith(TEXT("##")))
		{
			FString MetaLine = Trimmed.Mid(2).TrimStartAndEnd();
			FString Key, Value;
			if (MetaLine.Split(TEXT(":"), &Key, &Value))
			{
				Key = Key.TrimStartAndEnd();
				Value = Value.TrimStartAndEnd();

				if (Key.Equals(TEXT("Title"), ESearchCase::IgnoreCase))
				{
					Toc.Title = Value;
				}
				else if (Key.Equals(TEXT("Notes"), ESearchCase::IgnoreCase))
				{
					Toc.Notes = Value;
				}
				else if (Key.Equals(TEXT("Author"), ESearchCase::IgnoreCase))
				{
					Toc.Author = Value;
				}
				else if (Key.Equals(TEXT("Version"), ESearchCase::IgnoreCase))
				{
					Toc.Version = Value;
				}
				else if (Key.Equals(TEXT("Interface"), ESearchCase::IgnoreCase))
				{
					Toc.Interface = FCString::Atoi(*Value);
				}
				else if (Key.Equals(TEXT("RequiredDeps"), ESearchCase::IgnoreCase) ||
				         Key.Equals(TEXT("Dependencies"), ESearchCase::IgnoreCase))
				{
					Value.ParseIntoArray(Toc.RequiredDeps, TEXT(","));
					for (FString& Dep : Toc.RequiredDeps)
					{
						Dep = Dep.TrimStartAndEnd();
					}
				}
				else if (Key.Equals(TEXT("OptionalDeps"), ESearchCase::IgnoreCase))
				{
					Value.ParseIntoArray(Toc.OptionalDeps, TEXT(","));
					for (FString& Dep : Toc.OptionalDeps)
					{
						Dep = Dep.TrimStartAndEnd();
					}
				}
				else if (Key.Equals(TEXT("SavedVariables"), ESearchCase::IgnoreCase))
				{
					Value.ParseIntoArray(Toc.SavedVariables, TEXT(","));
					for (FString& SV : Toc.SavedVariables)
					{
						SV = SV.TrimStartAndEnd();
					}
				}
				else if (Key.Equals(TEXT("SavedVariablesPerCharacter"), ESearchCase::IgnoreCase))
				{
					Value.ParseIntoArray(Toc.SavedVariablesPerCharacter, TEXT(","));
					for (FString& SV : Toc.SavedVariablesPerCharacter)
					{
						SV = SV.TrimStartAndEnd();
					}
				}
				else if (Key.Equals(TEXT("LoadOnDemand"), ESearchCase::IgnoreCase))
				{
					Toc.bLoadOnDemand = Value.Equals(TEXT("1")) || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase);
				}
				else if (Key.Equals(TEXT("Disabled"), ESearchCase::IgnoreCase))
				{
					Toc.bDisabled = Value.Equals(TEXT("1")) || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase);
				}
			}
			continue;
		}

		// Comment lines starting with # (but not ##)
		if (Trimmed.StartsWith(TEXT("#")))
		{
			continue;
		}

		// File entry - normalize path separators
		FString FilePath = Trimmed.Replace(TEXT("/"), TEXT("\\"));
		Toc.Files.Add(FilePath);
	}

	return Toc;
}

TArray<FString> FWowAddonLoader::DiscoverAddons(FMpqManager* Mpq)
{
	TArray<FString> AddonNames;

	if (!Mpq)
	{
		return AddonNames;
	}

	// In WoW 3.3.5, addons live under Interface/AddOns/ in the data directory
	// Since MPQ doesn't support directory listing, we check well-known addon names
	// and also check the filesystem under the data path

	// Check filesystem for addon directories
	FString AddOnsPath = FPaths::Combine(Mpq->GetDataPath(), TEXT("Interface"), TEXT("AddOns"));
	IFileManager& FM = IFileManager::Get();

	TArray<FString> Directories;
	FM.FindFiles(Directories, *FPaths::Combine(AddOnsPath, TEXT("*")), false, true);

	for (const FString& Dir : Directories)
	{
		FString AddonName = FPaths::GetCleanFilename(Dir);
		// Verify a .toc file exists
		FString TocPath = FString::Printf(TEXT("Interface\\AddOns\\%s\\%s.toc"), *AddonName, *AddonName);
		TArray<uint8> TocData;
		if (Mpq->FileExists(TocPath) || FM.FileExists(*FPaths::Combine(AddOnsPath, AddonName, AddonName + TEXT(".toc"))))
		{
			AddonNames.Add(AddonName);
		}
	}

	UE_LOG(LogWowAddon, Log, TEXT("Discovered %d addons"), AddonNames.Num());
	return AddonNames;
}

bool FWowAddonLoader::LoadAddon(const FString& AddonName, FMpqManager* Mpq, FWowLuaVM* LuaVM)
{
	if (!Mpq || !LuaVM)
	{
		UE_LOG(LogWowAddon, Error, TEXT("LoadAddon %s: null MPQ or Lua VM"), *AddonName);
		return false;
	}

	// Read the TOC file
	FString TocPath = FString::Printf(TEXT("Interface\\AddOns\\%s\\%s.toc"), *AddonName, *AddonName);
	TArray<uint8> TocData;

	// Try MPQ first, then filesystem
	if (!Mpq->ReadFile(TocPath, TocData))
	{
		FString FsTocPath = FPaths::Combine(Mpq->GetDataPath(), TocPath.Replace(TEXT("\\"), TEXT("/")));
		if (!FFileHelper::LoadFileToArray(TocData, *FsTocPath))
		{
			UE_LOG(LogWowAddon, Warning, TEXT("Could not find TOC for addon: %s"), *AddonName);
			return false;
		}
	}

	FWowTocData Toc = ParseToc(TocData);

	if (Toc.bDisabled)
	{
		UE_LOG(LogWowAddon, Log, TEXT("Addon %s is disabled, skipping"), *AddonName);
		return false;
	}

	UE_LOG(LogWowAddon, Log, TEXT("Loading addon: %s (%s) - %d files"), *AddonName, *Toc.Title, Toc.Files.Num());

	FString BasePath = FString::Printf(TEXT("Interface\\AddOns\\%s\\"), *AddonName);

	for (const FString& File : Toc.Files)
	{
		FString FullPath = BasePath + File;
		TArray<uint8> FileData;

		// Try MPQ first, then filesystem
		bool bLoaded = Mpq->ReadFile(FullPath, FileData);
		if (!bLoaded)
		{
			FString FsPath = FPaths::Combine(Mpq->GetDataPath(), FullPath.Replace(TEXT("\\"), TEXT("/")));
			bLoaded = FFileHelper::LoadFileToArray(FileData, *FsPath);
		}

		if (!bLoaded)
		{
			UE_LOG(LogWowAddon, Warning, TEXT("  Could not load: %s"), *FullPath);
			continue;
		}

		if (File.EndsWith(TEXT(".lua"), ESearchCase::IgnoreCase))
		{
			LuaVM->ExecuteBuffer(FileData, FullPath);
		}
		else if (File.EndsWith(TEXT(".xml"), ESearchCase::IgnoreCase))
		{
			TArray<FWowXmlDirective> Directives = FWowFrameXmlParser::ParseXml(FileData, FullPath);

			// Process directives: execute scripts, handle includes
			for (const FWowXmlDirective& Dir : Directives)
			{
				switch (Dir.Type)
				{
				case FWowXmlDirective::EType::Script:
				{
					if (!Dir.FilePath.IsEmpty())
					{
						FString ScriptPath = BasePath + Dir.FilePath;
						TArray<uint8> ScriptData;
						if (Mpq->ReadFile(ScriptPath, ScriptData) ||
						    FFileHelper::LoadFileToArray(ScriptData, *FPaths::Combine(Mpq->GetDataPath(), ScriptPath.Replace(TEXT("\\"), TEXT("/")))))
						{
							LuaVM->ExecuteBuffer(ScriptData, ScriptPath);
						}
					}
					break;
				}
				case FWowXmlDirective::EType::Include:
				{
					if (!Dir.FilePath.IsEmpty())
					{
						FString IncPath = BasePath + Dir.FilePath;
						TArray<uint8> IncData;
						if (Mpq->ReadFile(IncPath, IncData) ||
						    FFileHelper::LoadFileToArray(IncData, *FPaths::Combine(Mpq->GetDataPath(), IncPath.Replace(TEXT("\\"), TEXT("/")))))
						{
							// Recursively parse included XML
							FWowFrameXmlParser::ParseXml(IncData, IncPath);
						}
					}
					break;
				}
				case FWowXmlDirective::EType::Frame:
				{
					// Frame creation will be handled by the frame manager
					UE_LOG(LogWowAddon, Verbose, TEXT("  Frame: %s"), *Dir.FrameDef.Name);
					break;
				}
				case FWowXmlDirective::EType::Font:
				{
					UE_LOG(LogWowAddon, Verbose, TEXT("  Font: %s"), *Dir.FontName);
					break;
				}
				}
			}
		}
	}

	UE_LOG(LogWowAddon, Log, TEXT("Addon %s loaded successfully"), *AddonName);
	return true;
}
