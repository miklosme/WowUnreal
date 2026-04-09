-- Test ScrollFrame implementation
print("=== ScrollFrame Test ===")

-- Create a simple scroll frame
local scrollFrame = CreateFrame("ScrollFrame", "TestScrollFrame", UIParent)
if not scrollFrame then
    print("ERROR: Failed to create ScrollFrame")
    return
end

print("✓ ScrollFrame created successfully")

-- Set its size and position
scrollFrame:SetSize(200, 100)
scrollFrame:SetPoint("CENTER", UIParent, "CENTER", 0, 0)

-- Create a child frame (content to scroll)
local contentFrame = CreateFrame("Frame", "TestScrollContent", scrollFrame)
if not contentFrame then
    print("ERROR: Failed to create content frame")
    return
end

print("✓ Content frame created successfully")

-- Make content larger than scroll frame to enable scrolling
contentFrame:SetSize(200, 300)
contentFrame:SetPoint("TOPLEFT", scrollFrame, "TOPLEFT", 0, 0)

-- Set the scroll child
scrollFrame:SetScrollChild(contentFrame)
print("✓ SetScrollChild called")

-- Test GetScrollChild
local retrievedChild = scrollFrame:GetScrollChild()
if retrievedChild == contentFrame then
    print("✓ GetScrollChild works correctly")
else
    print("ERROR: GetScrollChild returned wrong value")
    print("  Expected:", contentFrame)
    print("  Got:", retrievedChild)
end

-- Test vertical scrolling
local initialScroll = scrollFrame:GetVerticalScroll()
print("Initial vertical scroll:", initialScroll)

scrollFrame:SetVerticalScroll(50)
local newScroll = scrollFrame:GetVerticalScroll()
print("After SetVerticalScroll(50):", newScroll)

if newScroll == 50 then
    print("✓ Vertical scrolling works correctly")
else
    print("ERROR: Vertical scrolling failed")
    print("  Expected: 50")
    print("  Got:", newScroll)
end

-- Test scroll range
local scrollRange = scrollFrame:GetVerticalScrollRange()
print("Vertical scroll range:", scrollRange)

-- Expected: contentFrame height (300) - scrollFrame height (100) = 200
if scrollRange == 200 then
    print("✓ Vertical scroll range is correct")
else
    print("ERROR: Vertical scroll range is wrong")
    print("  Expected: 200")
    print("  Got:", scrollRange)
end

-- Test horizontal scrolling
scrollFrame:SetHorizontalScroll(25)
local horizontalScroll = scrollFrame:GetHorizontalScroll()
print("Horizontal scroll:", horizontalScroll)

if horizontalScroll == 25 then
    print("✓ Horizontal scrolling works correctly")
else
    print("ERROR: Horizontal scrolling failed")
    print("  Expected: 25")
    print("  Got:", horizontalScroll)
end

-- Test horizontal scroll range
local horizontalRange = scrollFrame:GetHorizontalScrollRange()
print("Horizontal scroll range:", horizontalRange)

-- Expected: contentFrame width (200) - scrollFrame width (200) = 0
if horizontalRange == 0 then
    print("✓ Horizontal scroll range is correct")
else
    print("ERROR: Horizontal scroll range is wrong")
    print("  Expected: 0")
    print("  Got:", horizontalRange)
end

-- Test scroll bounds
scrollFrame:SetVerticalScroll(-10) -- Should clamp to 0
local clampedScroll = scrollFrame:GetVerticalScroll()
if clampedScroll == 0 then
    print("✓ Negative scroll properly clamped to 0")
else
    print("ERROR: Negative scroll not clamped")
    print("  Expected: 0")
    print("  Got:", clampedScroll)
end

scrollFrame:SetVerticalScroll(250) -- Should clamp to max range (200)
local maxClampedScroll = scrollFrame:GetVerticalScroll()
if maxClampedScroll == 200 then
    print("✓ Excessive scroll properly clamped to max range")
else
    print("ERROR: Excessive scroll not clamped")
    print("  Expected: 200")
    print("  Got:", maxClampedScroll)
end

print("=== ScrollFrame Test Complete ===")