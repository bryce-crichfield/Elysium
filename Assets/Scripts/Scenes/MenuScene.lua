---@type SceneScript
---@class MenuScene
local MenuScene = {}

local BUTTONS = {
    { name = "PlayButton",    action = function() SceneReplace("ExploreScene") end },
    { name = "OptionsButton", action = nil },  -- placeholder
    { name = "QuitButton",    action = nil },  -- placeholder
}

local COLOR_DEFAULT_PLAY    = "#2266AA"
local COLOR_HOVER_PLAY      = "#3388CC"
local COLOR_DEFAULT_OPTIONS = "#226644"
local COLOR_HOVER_OPTIONS   = "#33886A"
local COLOR_DEFAULT_QUIT    = "#662222"
local COLOR_HOVER_QUIT      = "#883333"

local HOVER_COLORS = {
    PlayButton    = { default = COLOR_DEFAULT_PLAY,    hover = COLOR_HOVER_PLAY    },
    OptionsButton = { default = COLOR_DEFAULT_OPTIONS, hover = COLOR_HOVER_OPTIONS },
    QuitButton    = { default = COLOR_DEFAULT_QUIT,    hover = COLOR_HOVER_QUIT    },
}

-- Returns true if screen-space point (px, py) is inside an entity's rect.
local function hitTest(entity, px, py)
    local pos  = GetComponent(entity, "Transform")
    local rect = GetComponent(entity, "Rectangle")
    if not pos or not rect then return false end
    return px >= pos.worldX and px <= pos.worldX + rect.width
       and py >= pos.worldY and py <= pos.worldY + rect.height
end

function MenuScene:Initialize()
    self.buttons = {}
    for _, def in ipairs(BUTTONS) do
        local e = GetEntityByName(def.name)
        if e ~= 0 then
            table.insert(self.buttons, {
                entity  = e,
                name    = def.name,
                action  = def.action,
                hovered = false,
            })
        end
    end
    Log("MenuScene initialized")
end

function MenuScene:Update(dt)
end

function MenuScene:Render()
end

function MenuScene:OnEvent(event)
    if event.type == "MouseMoved" then
        for _, btn in ipairs(self.buttons) do
            local over = hitTest(btn.entity, event.x, event.y)
            if over ~= btn.hovered then
                btn.hovered = over
                local colors = HOVER_COLORS[btn.name]
                if colors then
                    local rect = GetComponent(btn.entity, "Rectangle")
                    if rect then
                        rect.background = over and colors.hover or colors.default
                    end
                end
            end
        end
    end

    if event.type == "MouseButtonReleased" and event.button == MOUSE_LEFT then
        for _, btn in ipairs(self.buttons) do
            if btn.hovered and btn.action then
                btn.action()
                return true
            end
        end
    end
end

return MenuScene
