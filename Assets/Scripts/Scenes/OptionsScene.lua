---@type SceneScript
---@class OptionsScene
local OptionsScene = {}

-- Button definitions — name matches entity, action is what fires on click.
-- Placeholders have no action (nil = do nothing for now).
local BUTTONS = {
    { name = "RESUME_BTN",   label = "Resume",            action = function() ScenePop() end },
    { name = "AUDIO_BTN",    label = "Audio Settings",    action = nil },
    { name = "VIDEO_BTN",    label = "Video Settings",    action = nil },
    { name = "CONTROLS_BTN", label = "Controls",          action = nil },
    { name = "BACK_BTN",     label = "Back to Main Menu", action = function() 
        SceneClear()
        ScenePush("MenuScene")
        end },
}

local COLOR_DEFAULT  = "#2A2A2EFF"
local COLOR_HOVER    = "#3C3C46FF"
local COLOR_DISABLED = "#2A2A2EFF"   -- same as default; placeholders stay flat

-- ─────────────────────────────────────────────────────────────────────────────

function OptionsScene:Initialize()
    -- Cache entity handles for each button
    self.buttons = {}
    for _, def in ipairs(BUTTONS) do
        local e = GetEntityByName(def.name)
        if e ~= 0 then
            table.insert(self.buttons, {
                entity = e,
                action = def.action,
                hovered = false,
            })
        end
    end
    Log("OptionsScene opened")
end

function OptionsScene:Update(dt)
end

function OptionsScene:Render()
end

-- Returns true if screen-space point (px, py) is inside an entity's rect.
local function hitTest(entity, px, py)
    local pos  = GetComponent(entity, "Transform")
    local rect = GetComponent(entity, "Rectangle")
    if not pos or not rect then return false end
    return px >= pos.worldX and px <= pos.worldX + rect.width
       and py >= pos.worldY and py <= pos.worldY + rect.height
end

function OptionsScene:OnEvent(event)
    if event.type == "KeyPressed" and event.key == KEY_ESCAPE then
        ScenePop()
        return true
    end

    -- Mouse hover: update button backgrounds
    if event.type == "MouseMoved" then
        for _, btn in ipairs(self.buttons) do
            local over = hitTest(btn.entity, event.x, event.y)
            if over ~= btn.hovered then
                btn.hovered = over
                local rect = GetComponent(btn.entity, "Rectangle")
                if rect and btn.action then   -- only interactive buttons highlight
                    rect.background = over and COLOR_HOVER or COLOR_DEFAULT
                end
            end
        end
    end

    -- Mouse click: fire action
    if event.type == "MouseButtonReleased" and event.button == MOUSE_LEFT then
        for _, btn in ipairs(self.buttons) do
            if btn.hovered and btn.action then
                btn.action()
                return true
            end
        end
    end
end

return OptionsScene
