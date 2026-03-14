#include "WowFrameXmlParser.h"
#include "Mpq/MpqManager.h"
#include "XmlParser/Public/XmlFile.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowXml, Log, All);

// ─── Enum parsers ──────────────────────────────────────────────────────────────

EWowFrameType FWowFrameXmlParser::ParseFrameType(const FString& TypeName)
{
	static const TMap<FString, EWowFrameType> Map = {
		{TEXT("Frame"),                  EWowFrameType::Frame},
		{TEXT("Button"),                 EWowFrameType::Button},
		{TEXT("CheckButton"),            EWowFrameType::CheckButton},
		{TEXT("EditBox"),                EWowFrameType::EditBox},
		{TEXT("ScrollFrame"),            EWowFrameType::ScrollFrame},
		{TEXT("ScrollingMessageFrame"),  EWowFrameType::ScrollingMessageFrame},
		{TEXT("MessageFrame"),           EWowFrameType::MessageFrame},
		{TEXT("SimpleHTML"),             EWowFrameType::SimpleHTML},
		{TEXT("Slider"),                 EWowFrameType::Slider},
		{TEXT("StatusBar"),              EWowFrameType::StatusBar},
		{TEXT("Cooldown"),               EWowFrameType::Cooldown},
		{TEXT("ColorSelect"),            EWowFrameType::ColorSelect},
		{TEXT("GameTooltip"),            EWowFrameType::GameTooltip},
		{TEXT("Minimap"),                EWowFrameType::Minimap},
		{TEXT("Model"),                  EWowFrameType::Model},
		{TEXT("PlayerModel"),            EWowFrameType::PlayerModel},
		{TEXT("DressUpModel"),           EWowFrameType::DressUpModel},
		{TEXT("TabardModel"),            EWowFrameType::TabardModel},
		{TEXT("WorldFrame"),             EWowFrameType::WorldFrame},
	};
	if (const EWowFrameType* Found = Map.Find(TypeName))
	{
		return *Found;
	}
	return EWowFrameType::Frame;
}

EWowAnchorPoint FWowFrameXmlParser::ParseAnchorPoint(const FString& PointName)
{
	static const TMap<FString, EWowAnchorPoint> Map = {
		{TEXT("TOPLEFT"),     EWowAnchorPoint::TOPLEFT},
		{TEXT("TOP"),         EWowAnchorPoint::TOP},
		{TEXT("TOPRIGHT"),    EWowAnchorPoint::TOPRIGHT},
		{TEXT("LEFT"),        EWowAnchorPoint::LEFT},
		{TEXT("CENTER"),      EWowAnchorPoint::CENTER},
		{TEXT("RIGHT"),       EWowAnchorPoint::RIGHT},
		{TEXT("BOTTOMLEFT"),  EWowAnchorPoint::BOTTOMLEFT},
		{TEXT("BOTTOM"),      EWowAnchorPoint::BOTTOM},
		{TEXT("BOTTOMRIGHT"), EWowAnchorPoint::BOTTOMRIGHT},
	};
	if (const EWowAnchorPoint* Found = Map.Find(PointName.ToUpper()))
	{
		return *Found;
	}
	return EWowAnchorPoint::CENTER;
}

EWowFrameStrata FWowFrameXmlParser::ParseStrata(const FString& StrataName)
{
	static const TMap<FString, EWowFrameStrata> Map = {
		{TEXT("BACKGROUND"),         EWowFrameStrata::BACKGROUND},
		{TEXT("LOW"),                EWowFrameStrata::LOW},
		{TEXT("MEDIUM"),             EWowFrameStrata::MEDIUM},
		{TEXT("HIGH"),               EWowFrameStrata::HIGH},
		{TEXT("DIALOG"),             EWowFrameStrata::DIALOG},
		{TEXT("FULLSCREEN"),         EWowFrameStrata::FULLSCREEN},
		{TEXT("FULLSCREEN_DIALOG"),  EWowFrameStrata::FULLSCREEN_DIALOG},
		{TEXT("TOOLTIP"),            EWowFrameStrata::TOOLTIP},
	};
	if (const EWowFrameStrata* Found = Map.Find(StrataName.ToUpper()))
	{
		return *Found;
	}
	return EWowFrameStrata::MEDIUM;
}

EWowDrawLayer FWowFrameXmlParser::ParseDrawLayer(const FString& LayerName)
{
	static const TMap<FString, EWowDrawLayer> Map = {
		{TEXT("BACKGROUND"), EWowDrawLayer::BACKGROUND},
		{TEXT("BORDER"),     EWowDrawLayer::BORDER},
		{TEXT("ARTWORK"),    EWowDrawLayer::ARTWORK},
		{TEXT("OVERLAY"),    EWowDrawLayer::OVERLAY},
		{TEXT("HIGHLIGHT"),  EWowDrawLayer::HIGHLIGHT},
	};
	if (const EWowDrawLayer* Found = Map.Find(LayerName.ToUpper()))
	{
		return *Found;
	}
	return EWowDrawLayer::ARTWORK;
}

// ─── Helper: parse color from node attributes ──────────────────────────────────

static FLinearColor ParseColor(const FXmlNode* Node)
{
	FLinearColor C = FLinearColor::White;
	if (!Node) return C;
	FString R = Node->GetAttribute(TEXT("r"));
	FString G = Node->GetAttribute(TEXT("g"));
	FString B = Node->GetAttribute(TEXT("b"));
	FString A = Node->GetAttribute(TEXT("a"));
	if (!R.IsEmpty()) C.R = FCString::Atof(*R);
	if (!G.IsEmpty()) C.G = FCString::Atof(*G);
	if (!B.IsEmpty()) C.B = FCString::Atof(*B);
	if (!A.IsEmpty()) C.A = FCString::Atof(*A);
	return C;
}

// ─── Helper: parse Size node ───────────────────────────────────────────────────

static void ParseSize(const FXmlNode* SizeNode, float& OutW, float& OutH)
{
	if (!SizeNode) return;
	FString X = SizeNode->GetAttribute(TEXT("x"));
	FString Y = SizeNode->GetAttribute(TEXT("y"));
	if (!X.IsEmpty()) OutW = FCString::Atof(*X);
	if (!Y.IsEmpty()) OutH = FCString::Atof(*Y);

	// Also check for <AbsDimension> child
	const FXmlNode* AbsDim = SizeNode->FindChildNode(TEXT("AbsDimension"));
	if (AbsDim)
	{
		X = AbsDim->GetAttribute(TEXT("x"));
		Y = AbsDim->GetAttribute(TEXT("y"));
		if (!X.IsEmpty()) OutW = FCString::Atof(*X);
		if (!Y.IsEmpty()) OutH = FCString::Atof(*Y);
	}
}

// ─── Helper: parse Anchor node ─────────────────────────────────────────────────

static FWowAnchor ParseAnchorNode(const FXmlNode* Node)
{
	FWowAnchor Anchor;
	Anchor.Point = FWowFrameXmlParser::ParseAnchorPoint(Node->GetAttribute(TEXT("point")));
	Anchor.RelativeTo = Node->GetAttribute(TEXT("relativeTo"));

	FString RelPoint = Node->GetAttribute(TEXT("relativePoint"));
	if (!RelPoint.IsEmpty())
	{
		Anchor.RelativePoint = FWowFrameXmlParser::ParseAnchorPoint(RelPoint);
	}
	else
	{
		Anchor.RelativePoint = Anchor.Point;
	}

	// Offset from attributes
	FString OX = Node->GetAttribute(TEXT("x"));
	FString OY = Node->GetAttribute(TEXT("y"));
	if (!OX.IsEmpty()) Anchor.OffsetX = FCString::Atof(*OX);
	if (!OY.IsEmpty()) Anchor.OffsetY = FCString::Atof(*OY);

	// Or from <Offset> child
	const FXmlNode* OffsetNode = Node->FindChildNode(TEXT("Offset"));
	if (OffsetNode)
	{
		const FXmlNode* AbsDim = OffsetNode->FindChildNode(TEXT("AbsDimension"));
		if (AbsDim)
		{
			OX = AbsDim->GetAttribute(TEXT("x"));
			OY = AbsDim->GetAttribute(TEXT("y"));
		}
		else
		{
			OX = OffsetNode->GetAttribute(TEXT("x"));
			OY = OffsetNode->GetAttribute(TEXT("y"));
		}
		if (!OX.IsEmpty()) Anchor.OffsetX = FCString::Atof(*OX);
		if (!OY.IsEmpty()) Anchor.OffsetY = FCString::Atof(*OY);
	}

	return Anchor;
}

// ─── Helper: parse anchors list ────────────────────────────────────────────────

static TArray<FWowAnchor> ParseAnchors(const FXmlNode* AnchorsNode)
{
	TArray<FWowAnchor> Result;
	if (!AnchorsNode) return Result;

	for (const FXmlNode* Child = AnchorsNode->GetFirstChildNode(); Child; Child = Child->GetNextNode())
	{
		if (Child->GetTag().Equals(TEXT("Anchor"), ESearchCase::IgnoreCase))
		{
			Result.Add(ParseAnchorNode(Child));
		}
	}
	return Result;
}

// ─── Helper: parse Texture element ─────────────────────────────────────────────

static FWowTextureElement ParseTextureNode(const FXmlNode* Node)
{
	FWowTextureElement Tex;
	Tex.Name = Node->GetAttribute(TEXT("name"));
	Tex.File = Node->GetAttribute(TEXT("file"));

	// Size from attributes or child
	const FXmlNode* SizeNode = Node->FindChildNode(TEXT("Size"));
	ParseSize(SizeNode, Tex.Width, Tex.Height);

	// Anchors
	const FXmlNode* AnchorsNode = Node->FindChildNode(TEXT("Anchors"));
	Tex.Anchors = ParseAnchors(AnchorsNode);

	// TexCoords
	const FXmlNode* TexCoords = Node->FindChildNode(TEXT("TexCoords"));
	if (TexCoords)
	{
		FString L = TexCoords->GetAttribute(TEXT("left"));
		FString R = TexCoords->GetAttribute(TEXT("right"));
		FString T = TexCoords->GetAttribute(TEXT("top"));
		FString B = TexCoords->GetAttribute(TEXT("bottom"));
		if (!L.IsEmpty()) Tex.Left = FCString::Atof(*L);
		if (!R.IsEmpty()) Tex.Right = FCString::Atof(*R);
		if (!T.IsEmpty()) Tex.Top = FCString::Atof(*T);
		if (!B.IsEmpty()) Tex.Bottom = FCString::Atof(*B);
	}

	// Color
	const FXmlNode* ColorNode = Node->FindChildNode(TEXT("Color"));
	if (ColorNode)
	{
		Tex.VertexColor = ParseColor(ColorNode);
	}

	return Tex;
}

// ─── Helper: parse FontString element ──────────────────────────────────────────

static FWowFontStringElement ParseFontStringNode(const FXmlNode* Node)
{
	FWowFontStringElement FS;
	FS.Name = Node->GetAttribute(TEXT("name"));
	FS.Inherits = Node->GetAttribute(TEXT("inherits"));
	FS.Text = Node->GetAttribute(TEXT("text"));

	FString JH = Node->GetAttribute(TEXT("justifyH"));
	FString JV = Node->GetAttribute(TEXT("justifyV"));
	if (!JH.IsEmpty()) FS.JustifyH = JH;
	if (!JV.IsEmpty()) FS.JustifyV = JV;

	// Size
	const FXmlNode* SizeNode = Node->FindChildNode(TEXT("Size"));
	ParseSize(SizeNode, FS.Width, FS.Height);

	// Anchors
	const FXmlNode* AnchorsNode = Node->FindChildNode(TEXT("Anchors"));
	FS.Anchors = ParseAnchors(AnchorsNode);

	// Color
	const FXmlNode* ColorNode = Node->FindChildNode(TEXT("Color"));
	if (ColorNode)
	{
		FS.Color = ParseColor(ColorNode);
	}

	// FontHeight
	const FXmlNode* FontHeightNode = Node->FindChildNode(TEXT("FontHeight"));
	if (FontHeightNode)
	{
		const FXmlNode* AbsVal = FontHeightNode->FindChildNode(TEXT("AbsValue"));
		if (AbsVal)
		{
			FString Val = AbsVal->GetAttribute(TEXT("val"));
			if (!Val.IsEmpty()) FS.FontHeight = FCString::Atof(*Val);
		}
	}

	FString FontFlags = Node->GetAttribute(TEXT("outline"));
	if (!FontFlags.IsEmpty()) FS.FontFlags = FontFlags;

	return FS;
}

// ─── Helper: parse Layers ──────────────────────────────────────────────────────

static TArray<FWowLayer> ParseLayers(const FXmlNode* LayersNode)
{
	TArray<FWowLayer> Result;
	if (!LayersNode) return Result;

	for (const FXmlNode* LayerNode = LayersNode->GetFirstChildNode(); LayerNode; LayerNode = LayerNode->GetNextNode())
	{
		if (!LayerNode->GetTag().Equals(TEXT("Layer"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		FWowLayer Layer;
		FString LevelStr = LayerNode->GetAttribute(TEXT("level"));
		if (!LevelStr.IsEmpty())
		{
			Layer.Level = FWowFrameXmlParser::ParseDrawLayer(LevelStr);
		}

		for (const FXmlNode* Elem = LayerNode->GetFirstChildNode(); Elem; Elem = Elem->GetNextNode())
		{
			FString Tag = Elem->GetTag();
			if (Tag.Equals(TEXT("Texture"), ESearchCase::IgnoreCase))
			{
				Layer.Textures.Add(ParseTextureNode(Elem));
			}
			else if (Tag.Equals(TEXT("FontString"), ESearchCase::IgnoreCase))
			{
				Layer.FontStrings.Add(ParseFontStringNode(Elem));
			}
		}

		Result.Add(MoveTemp(Layer));
	}
	return Result;
}

// ─── Helper: parse Scripts ─────────────────────────────────────────────────────

static TArray<FWowScriptHandler> ParseScripts(const FXmlNode* ScriptsNode)
{
	TArray<FWowScriptHandler> Result;
	if (!ScriptsNode) return Result;

	for (const FXmlNode* Child = ScriptsNode->GetFirstChildNode(); Child; Child = Child->GetNextNode())
	{
		FWowScriptHandler Handler;
		Handler.Event = Child->GetTag();
		Handler.Code = Child->GetContent();
		Handler.File = Child->GetAttribute(TEXT("function"));
		// Some scripts use a "file" attribute instead
		FString FileAttr = Child->GetAttribute(TEXT("file"));
		if (!FileAttr.IsEmpty())
		{
			Handler.File = FileAttr;
		}
		Result.Add(MoveTemp(Handler));
	}
	return Result;
}

// ─── Helper: parse Backdrop ────────────────────────────────────────────────────

static FWowBackdrop ParseBackdropNode(const FXmlNode* Node)
{
	FWowBackdrop BD;
	BD.BgFile = Node->GetAttribute(TEXT("bgFile"));
	BD.EdgeFile = Node->GetAttribute(TEXT("edgeFile"));

	FString ES = Node->GetAttribute(TEXT("edgeSize"));
	if (!ES.IsEmpty()) BD.EdgeSize = FCString::Atoi(*ES);

	FString Tile = Node->GetAttribute(TEXT("tile"));
	BD.Tile = Tile.Equals(TEXT("true"), ESearchCase::IgnoreCase);

	FString TS = Node->GetAttribute(TEXT("tileSize"));
	if (!TS.IsEmpty()) BD.TileSize = FCString::Atoi(*TS);

	// BackgroundInsets
	const FXmlNode* Insets = Node->FindChildNode(TEXT("BackgroundInsets"));
	if (!Insets) Insets = Node->FindChildNode(TEXT("EdgeSize"));
	if (Insets)
	{
		const FXmlNode* AbsInset = Insets->FindChildNode(TEXT("AbsInset"));
		if (AbsInset)
		{
			FString L = AbsInset->GetAttribute(TEXT("left"));
			FString R = AbsInset->GetAttribute(TEXT("right"));
			FString T = AbsInset->GetAttribute(TEXT("top"));
			FString B = AbsInset->GetAttribute(TEXT("bottom"));
			if (!L.IsEmpty()) BD.InsetLeft = FCString::Atof(*L);
			if (!R.IsEmpty()) BD.InsetRight = FCString::Atof(*R);
			if (!T.IsEmpty()) BD.InsetTop = FCString::Atof(*T);
			if (!B.IsEmpty()) BD.InsetBottom = FCString::Atof(*B);
		}
	}

	return BD;
}

// ─── Set of recognized frame type tags ─────────────────────────────────────────

static bool IsFrameTypeTag(const FString& Tag)
{
	static const TSet<FString> FrameTags = {
		TEXT("Frame"), TEXT("Button"), TEXT("CheckButton"), TEXT("EditBox"),
		TEXT("ScrollFrame"), TEXT("ScrollingMessageFrame"), TEXT("MessageFrame"),
		TEXT("SimpleHTML"), TEXT("Slider"), TEXT("StatusBar"), TEXT("Cooldown"),
		TEXT("ColorSelect"), TEXT("GameTooltip"), TEXT("Minimap"),
		TEXT("Model"), TEXT("PlayerModel"), TEXT("DressUpModel"),
		TEXT("TabardModel"), TEXT("WorldFrame")
	};
	return FrameTags.Contains(Tag);
}

// ─── Forward declare ───────────────────────────────────────────────────────────

static FWowFrameDef ParseFrameNode(const FXmlNode* Node);

// ─── Parse a frame node recursively ────────────────────────────────────────────

static FWowFrameDef ParseFrameNode(const FXmlNode* Node)
{
	FWowFrameDef Def;
	Def.Type = FWowFrameXmlParser::ParseFrameType(Node->GetTag());
	Def.Name = Node->GetAttribute(TEXT("name"));
	Def.Parent = Node->GetAttribute(TEXT("parent"));
	Def.Inherits = Node->GetAttribute(TEXT("inherits"));

	FString Virtual = Node->GetAttribute(TEXT("virtual"));
	Def.bVirtual = Virtual.Equals(TEXT("true"), ESearchCase::IgnoreCase);

	FString Hidden = Node->GetAttribute(TEXT("hidden"));
	Def.bHidden = Hidden.Equals(TEXT("true"), ESearchCase::IgnoreCase);

	FString SetAllPoints = Node->GetAttribute(TEXT("setAllPoints"));
	Def.bSetAllPoints = SetAllPoints.Equals(TEXT("true"), ESearchCase::IgnoreCase);

	FString StrataStr = Node->GetAttribute(TEXT("frameStrata"));
	if (!StrataStr.IsEmpty()) Def.Strata = FWowFrameXmlParser::ParseStrata(StrataStr);

	FString LevelStr = Node->GetAttribute(TEXT("frameLevel"));
	if (!LevelStr.IsEmpty()) Def.FrameLevel = FCString::Atoi(*LevelStr);

	// Type-specific attributes
	FString Orientation = Node->GetAttribute(TEXT("orientation"));
	if (!Orientation.IsEmpty()) Def.Orientation = Orientation;

	FString MinVal = Node->GetAttribute(TEXT("minValue"));
	FString MaxVal = Node->GetAttribute(TEXT("maxValue"));
	FString DefVal = Node->GetAttribute(TEXT("defaultValue"));
	if (!MinVal.IsEmpty()) Def.MinValue = FCString::Atof(*MinVal);
	if (!MaxVal.IsEmpty()) Def.MaxValue = FCString::Atof(*MaxVal);
	if (!DefVal.IsEmpty()) Def.DefaultValue = FCString::Atof(*DefVal);

	// Parse children
	for (const FXmlNode* Child = Node->GetFirstChildNode(); Child; Child = Child->GetNextNode())
	{
		FString Tag = Child->GetTag();

		if (Tag.Equals(TEXT("Size"), ESearchCase::IgnoreCase))
		{
			ParseSize(Child, Def.Width, Def.Height);
		}
		else if (Tag.Equals(TEXT("Anchors"), ESearchCase::IgnoreCase))
		{
			Def.Anchors = ParseAnchors(Child);
		}
		else if (Tag.Equals(TEXT("Layers"), ESearchCase::IgnoreCase))
		{
			Def.Layers = ParseLayers(Child);
		}
		else if (Tag.Equals(TEXT("Scripts"), ESearchCase::IgnoreCase))
		{
			Def.Scripts = ParseScripts(Child);
		}
		else if (Tag.Equals(TEXT("Backdrop"), ESearchCase::IgnoreCase))
		{
			Def.Backdrop = ParseBackdropNode(Child);
		}
		else if (Tag.Equals(TEXT("Frames"), ESearchCase::IgnoreCase))
		{
			for (const FXmlNode* ChildFrame = Child->GetFirstChildNode(); ChildFrame; ChildFrame = ChildFrame->GetNextNode())
			{
				if (IsFrameTypeTag(ChildFrame->GetTag()))
				{
					Def.Children.Add(ParseFrameNode(ChildFrame));
				}
			}
		}
		else if (Tag.Equals(TEXT("ButtonText"), ESearchCase::IgnoreCase))
		{
			// ButtonText can have a FontString child or just text content
			const FXmlNode* FS = Child->FindChildNode(TEXT("FontString"));
			if (FS)
			{
				Def.ButtonText = FS->GetAttribute(TEXT("text"));
			}
			else
			{
				Def.ButtonText = Child->GetContent();
			}
		}
		else if (Tag.Equals(TEXT("NormalTexture"), ESearchCase::IgnoreCase))
		{
			Def.NormalTexture = Child->GetAttribute(TEXT("file"));
		}
		else if (Tag.Equals(TEXT("PushedTexture"), ESearchCase::IgnoreCase))
		{
			Def.PushedTexture = Child->GetAttribute(TEXT("file"));
		}
		else if (Tag.Equals(TEXT("HighlightTexture"), ESearchCase::IgnoreCase))
		{
			Def.HighlightTexture = Child->GetAttribute(TEXT("file"));
		}
		else if (Tag.Equals(TEXT("DisabledTexture"), ESearchCase::IgnoreCase))
		{
			Def.DisabledTexture = Child->GetAttribute(TEXT("file"));
		}
	}

	return Def;
}

// ─── Public API ────────────────────────────────────────────────────────────────

TArray<FWowXmlDirective> FWowFrameXmlParser::ParseXml(const TArray<uint8>& Data, const FString& FileName)
{
	TArray<FWowXmlDirective> Directives;

	// Convert bytes to string
	FString XmlContent;
	if (Data.Num() >= 3 && Data[0] == 0xEF && Data[1] == 0xBB && Data[2] == 0xBF)
	{
		// UTF-8 BOM - skip it
		FUTF8ToTCHAR Conv((const ANSICHAR*)Data.GetData() + 3, Data.Num() - 3);
		XmlContent = FString(Conv.Length(), Conv.Get());
	}
	else
	{
		FUTF8ToTCHAR Conv((const ANSICHAR*)Data.GetData(), Data.Num());
		XmlContent = FString(Conv.Length(), Conv.Get());
	}

	FXmlFile XmlFile;
	if (!XmlFile.LoadFile(XmlContent, EConstructMethod::ConstructFromBuffer))
	{
		UE_LOG(LogWowXml, Warning, TEXT("Failed to parse XML: %s - %s"), *FileName, *XmlFile.GetLastError());
		return Directives;
	}

	const FXmlNode* Root = XmlFile.GetRootNode();
	if (!Root)
	{
		UE_LOG(LogWowXml, Warning, TEXT("No root node in XML: %s"), *FileName);
		return Directives;
	}

	// The root should be <Ui> but we'll accept anything
	for (const FXmlNode* Child = Root->GetFirstChildNode(); Child; Child = Child->GetNextNode())
	{
		FString Tag = Child->GetTag();

		if (IsFrameTypeTag(Tag))
		{
			FWowXmlDirective Dir;
			Dir.Type = FWowXmlDirective::EType::Frame;
			Dir.FrameDef = ParseFrameNode(Child);
			Directives.Add(MoveTemp(Dir));
		}
		else if (Tag.Equals(TEXT("Include"), ESearchCase::IgnoreCase))
		{
			FWowXmlDirective Dir;
			Dir.Type = FWowXmlDirective::EType::Include;
			Dir.FilePath = Child->GetAttribute(TEXT("file"));
			Directives.Add(MoveTemp(Dir));
		}
		else if (Tag.Equals(TEXT("Script"), ESearchCase::IgnoreCase))
		{
			FWowXmlDirective Dir;
			Dir.Type = FWowXmlDirective::EType::Script;
			Dir.FilePath = Child->GetAttribute(TEXT("file"));
			Directives.Add(MoveTemp(Dir));
		}
		else if (Tag.Equals(TEXT("Font"), ESearchCase::IgnoreCase))
		{
			FWowXmlDirective Dir;
			Dir.Type = FWowXmlDirective::EType::Font;
			Dir.FontName = Child->GetAttribute(TEXT("name"));
			Dir.FontInherits = Child->GetAttribute(TEXT("inherits"));
			FString FH = Child->GetAttribute(TEXT("height"));
			if (!FH.IsEmpty()) Dir.FontHeight = FCString::Atof(*FH);
			Dir.FontFlags = Child->GetAttribute(TEXT("flags"));

			// Check for FontHeight child
			const FXmlNode* FHNode = Child->FindChildNode(TEXT("FontHeight"));
			if (FHNode)
			{
				const FXmlNode* AbsVal = FHNode->FindChildNode(TEXT("AbsValue"));
				if (AbsVal)
				{
					FString Val = AbsVal->GetAttribute(TEXT("val"));
					if (!Val.IsEmpty()) Dir.FontHeight = FCString::Atof(*Val);
				}
			}

			Directives.Add(MoveTemp(Dir));
		}
	}

	UE_LOG(LogWowXml, Log, TEXT("Parsed %s: %d directives"), *FileName, Directives.Num());
	return Directives;
}

TArray<FWowXmlDirective> FWowFrameXmlParser::LoadFrameXml(FMpqManager* Mpq)
{
	TArray<FWowXmlDirective> AllDirectives;

	if (!Mpq)
	{
		UE_LOG(LogWowXml, Error, TEXT("LoadFrameXml: No MPQ manager"));
		return AllDirectives;
	}

	// Read the FrameXML.toc
	TArray<uint8> TocData;
	if (!Mpq->ReadFile(TEXT("Interface\\FrameXML\\FrameXML.toc"), TocData))
	{
		UE_LOG(LogWowXml, Error, TEXT("Failed to read Interface\\FrameXML\\FrameXML.toc"));
		return AllDirectives;
	}

	// Parse TOC lines
	FString TocContent;
	FUTF8ToTCHAR Conv((const ANSICHAR*)TocData.GetData(), TocData.Num());
	TocContent = FString(Conv.Length(), Conv.Get());

	TArray<FString> Lines;
	TocContent.ParseIntoArrayLines(Lines);

	for (const FString& Line : Lines)
	{
		FString Trimmed = Line.TrimStartAndEnd();

		// Skip empty lines and ## metadata
		if (Trimmed.IsEmpty() || Trimmed.StartsWith(TEXT("##")))
		{
			continue;
		}

		// Determine type from extension
		if (Trimmed.EndsWith(TEXT(".xml"), ESearchCase::IgnoreCase))
		{
			FString FullPath = FString::Printf(TEXT("Interface\\FrameXML\\%s"), *Trimmed);
			TArray<uint8> FileData;
			if (Mpq->ReadFile(FullPath, FileData))
			{
				TArray<FWowXmlDirective> Parsed = ParseXml(FileData, Trimmed);
				AllDirectives.Append(MoveTemp(Parsed));
			}
			else
			{
				UE_LOG(LogWowXml, Warning, TEXT("Could not read FrameXML file: %s"), *FullPath);
			}
		}
		else if (Trimmed.EndsWith(TEXT(".lua"), ESearchCase::IgnoreCase))
		{
			FWowXmlDirective Dir;
			Dir.Type = FWowXmlDirective::EType::Script;
			Dir.FilePath = FString::Printf(TEXT("Interface\\FrameXML\\%s"), *Trimmed);
			AllDirectives.Add(MoveTemp(Dir));
		}
	}

	UE_LOG(LogWowXml, Log, TEXT("LoadFrameXml: %d total directives from FrameXML.toc"), AllDirectives.Num());
	return AllDirectives;
}
