# UI System — FrameXML & Widgets

## Goal
Implement the WoW FrameXML XML parser and widget system that maps WoW UI frames to UE5 UMG widgets, enabling the default Blizzard UI and addons to render.

## Context
- Depends on: `lua-api.md` (Lua bindings must exist for scripts to run)
- XML parser skeleton exists in `Source/WowUI/`
- TOC parser skeleton exists
- The default WoW UI is ~200 Lua/XML files in `Interface/FrameXML/` inside MPQ
- ~40 Blizzard addons in `Interface/AddOns/Blizzard_*`
- Reference: WoW 3.3.5 FrameXML source (extractable from MPQ)

## Requirements

### XML Parser
1. Parse WoW FrameXML XML format (not standard XML — has custom elements)
2. Handle `<Include file="..."/>` directives (load another XML file inline)
3. Handle `inherits="TemplateName"` — copy attributes/children from a virtual template
4. Handle `virtual="true"` — define templates without creating visible frames
5. Process `<Scripts>` blocks — extract OnLoad, OnEvent, OnClick etc. as Lua code strings

### Widget Type Mapping
Each WoW frame type maps to a UMG widget tree:

| WoW Type | UMG Implementation |
|----------|-------------------|
| Frame | UCanvasPanel (container with anchored children) |
| Button | UButton with overlaid Texture + FontString |
| CheckButton | UCheckBox variant |
| EditBox | UEditableTextBox |
| Slider | USlider |
| StatusBar | UProgressBar |
| ScrollFrame | UScrollBox |
| ScrollingMessageFrame | Custom — scrollable text log (chat) |
| GameTooltip | Floating UCanvasPanel |
| Minimap | Custom circular masked widget |
| Cooldown | Custom — radial sweep overlay |
| Model/PlayerModel | Viewport widget with 3D scene capture |
| Texture | UImage |
| FontString | UTextBlock |

### Anchor System
1. WoW uses a point-based anchor system: `SetPoint("TOPLEFT", relativeTo, "BOTTOMLEFT", xOff, yOff)`
2. Anchor points: TOPLEFT, TOP, TOPRIGHT, LEFT, CENTER, RIGHT, BOTTOMLEFT, BOTTOM, BOTTOMRIGHT
3. Map to UMG Canvas Panel slot anchors + offsets
4. Multiple anchors (e.g., TOPLEFT + BOTTOMRIGHT) = stretch fill

### Strata & Layering
1. Frame strata (back to front): BACKGROUND, LOW, MEDIUM, HIGH, DIALOG, FULLSCREEN, FULLSCREEN_DIALOG, TOOLTIP
2. Frame level within strata (integer)
3. Layer sublevel for textures/fontstrings within a frame: BACKGROUND, BORDER, ARTWORK, OVERLAY, HIGHLIGHT
4. Map to UMG Z-order

### Event System
1. C++ fires events (PLAYER_LOGIN, UNIT_HEALTH, BAG_UPDATE, etc.)
2. Frames that called `RegisterEvent("EVENT_NAME")` get their OnEvent script called
3. Event dispatch: iterate registered frames, call `frame:GetScript("OnEvent")(self, event, ...)`
4. OnUpdate called every frame for frames with OnUpdate scripts
5. Mouse events: OnEnter, OnLeave, OnClick, OnMouseDown, OnMouseUp, OnDragStart, OnDragStop

### Addon Loading
1. Scan `Interface/AddOns/` in MPQ for TOC files
2. Parse TOC: `## Title`, `## Dependencies`, `## OptionalDeps`, `## SavedVariables`
3. Resolve load order based on dependencies
4. For each addon: load files listed in TOC (Lua files executed, XML files parsed)
5. Fire `ADDON_LOADED` event after each addon

### Font System
1. WoW uses TrueType fonts from MPQ (`Fonts/*.ttf`)
2. Load fonts and create UFont assets at runtime
3. Default fonts: `FRIZQT__.TTF` (main UI font), `ARIALN.TTF` (small text)

### Default UI Boot Sequence
```
1. Load fonts from MPQ Fonts/
2. Parse Interface/FrameXML/FrameXML.toc
3. Load files in TOC order (creates UIParent, core frames)
4. Fire PLAYER_LOGIN, VARIABLES_LOADED events
5. Load Blizzard_* addons from Interface/AddOns/
6. Load user addons from Interface/AddOns/
7. Fire ADDON_LOADED for each
8. Fire PLAYER_ENTERING_WORLD
```

## Architecture

### New/Modified Files
- `WowUI/WowFrameXmlParser.h/.cpp` — XML parser + template system
- `WowUI/WowWidgetFactory.h/.cpp` — Creates UMG widgets from XML elements
- `WowUI/WowFrameManager.h/.cpp` — Frame registry, strata management, event dispatch
- `WowUI/WowAddonLoader.h/.cpp` — TOC parsing, dependency resolution, file loading
- `WowUI/Widgets/WowFrame.h` — Base frame widget
- `WowUI/Widgets/WowButton.h` — Button widget
- `WowUI/Widgets/WowEditBox.h` — Text input widget
- `WowUI/Widgets/WowStatusBar.h` — Health/mana bars
- `WowUI/Widgets/WowScrollingMessageFrame.h` — Chat window
- `WowUI/Widgets/WowCooldown.h` — Cooldown sweep

## Acceptance Criteria
- [ ] Builds without errors
- [ ] XML files from FrameXML/ parse without crashes
- [ ] `<Frame>` elements create visible UMG containers
- [ ] `<Texture>` elements display BLP textures from MPQ
- [ ] `<FontString>` elements display text with correct fonts
- [ ] Anchor system positions frames correctly
- [ ] OnLoad scripts fire when frames are created
- [ ] Event dispatch works (RegisterEvent + fire → OnEvent called)
- [ ] At least `UIParent` frame loads from FrameXML
- [ ] Screenshot shows some UI elements rendered on screen

## Verification Steps
1. Build the editor target using [development setup](../setup/development.md)
2. Launch the relevant map using [development setup](../setup/development.md)
3. Check log for XML parsing progress ("Loaded FrameXML/UIParent.xml", etc.)
4. Check log for Lua errors (expected, but should not crash)
5. Screenshot — any visible UI elements indicate success
