-- GameTooltip API Test Script
-- This script tests the basic tooltip functionality

print("Testing GameTooltip API...")

-- Test 1: Basic tooltip creation and text setting
if GameTooltip then
    print("GameTooltip exists")

    -- Test SetText
    GameTooltip:SetText("Test Tooltip Text")
    print("SetText: OK")

    -- Test AddLine
    GameTooltip:AddLine("Line 1: White text")
    GameTooltip:AddLine("Line 2: Red text", 1, 0, 0)
    GameTooltip:AddLine("Line 3: Green text", 0, 1, 0)
    print("AddLine: OK")

    -- Test AddDoubleLine
    GameTooltip:AddDoubleLine("Left Text", "Right Text", 1, 1, 1, 1, 1, 0)
    print("AddDoubleLine: OK")

    -- Test NumLines
    local numLines = GameTooltip:NumLines()
    print("NumLines: " .. numLines)

    -- Test AppendText
    GameTooltip:AppendText(" (appended)")
    print("AppendText: OK")

    -- Test Show/Hide
    GameTooltip:Show()
    print("Show: OK")

    -- Wait a moment then hide
    C_Timer.After(2, function()
        GameTooltip:Hide()
        print("Hide: OK")
    end)

    -- Test SetOwner (using UIParent as owner)
    if UIParent then
        GameTooltip:SetOwner(UIParent, "ANCHOR_CURSOR")
        print("SetOwner: OK")
    else
        print("SetOwner: UIParent not found")
    end

    -- Test ClearLines
    GameTooltip:ClearLines()
    local numLinesAfterClear = GameTooltip:NumLines()
    print("ClearLines: " .. numLinesAfterClear .. " lines remaining")

    -- Test context methods
    GameTooltip:SetSpell(1)
    local spellId = GameTooltip:GetSpell()
    if spellId then
        print("SetSpell/GetSpell: " .. spellId)
    else
        print("SetSpell/GetSpell: nil")
    end

    GameTooltip:SetItem("Test Item Link")
    local itemLink = GameTooltip:GetItem()
    if itemLink then
        print("SetItem/GetItem: " .. itemLink)
    else
        print("SetItem/GetItem: nil")
    end

    GameTooltip:SetUnit("player")
    local unitToken = GameTooltip:GetUnit()
    if unitToken then
        print("SetUnit/GetUnit: " .. unitToken)
    else
        print("SetUnit/GetUnit: nil")
    end

    print("All GameTooltip tests completed!")

else
    print("ERROR: GameTooltip not found!")
end