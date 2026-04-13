---@type SceneScript
---@class MenuScene
local MenuScene = {}

SCENE_WIDTH  = 640
SCENE_HEIGHT = 480

function MenuScene:Initialize()
    Log("MenuScene initialized")
end

function MenuScene:Update(dt)
end

function MenuScene:Render()
end

-- Returns true if world-space point (mx, my) is inside the button entity's rect.
local function hitTest(entity, mx, my)
    local pos  = GetComponent(entity, "Position")
    local rect = GetComponent(entity, "Rectangle")
    if not pos or not rect then return false end
    local hw = rect.width  * 0.5
    local hh = rect.height * 0.5
    return mx >= pos.x - hw and mx <= pos.x + hw
       and my >= pos.y - hh and my <= pos.y + hh
end

local function worldToScreen(wx, wy)
    local cam = GetComponent(GetEntityByName("CAMERA"), "Camera")
    local pos = GetComponent(GetEntityByName("CAMERA"), "Position")
    if not cam then return wx, wy end
    if not pos then return wx, wy end
    local sx = wx - pos.x + cam.viewport.width  * 0.5
    local sy = wy - pos.y + cam.viewport.height * 0.5
    return sx, sy
end

function MenuScene:OnEvent(event)
    if event.type ~= "MouseButtonReleased" or event.button ~= MOUSE_LEFT then return end

    local mx, my = event.wx, event.wy
    mx, my = worldToScreen(mx, my)
    Log(string.format("Mouse released at: %.2f, %.2f", mx, my))
    local play    = GetEntityByName("PlayButton")
    local options = GetEntityByName("OptionsButton")
    local quit    = GetEntityByName("QuitButton")

    if play    ~= 0 and hitTest(play,    mx, my) then
        SceneReplace("ExploreScene")
    elseif options ~= 0 and hitTest(options, mx, my) then
        Log("Options: not yet implemented")
    elseif quit    ~= 0 and hitTest(quit,    mx, my) then
        Log("Quit: not yet implemented")
    end
end

return MenuScene
