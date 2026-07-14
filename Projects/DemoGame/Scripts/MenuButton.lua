---@type EntityScript
---@class MenuButton
local MenuButton = {}

function MenuButton:Initialize()
    self.time = 0
    self.isHovering = false
end

function MenuButton:Update(entity, dt)
    -- local mousePos = GetMousePosition()
    -- local screenMousePos = WorldToScreen(mousePos)
    -- local pos = GetComponent(entity, "Transform")
    -- local rect = GetComponent(entity, "Rectangle")
    -- self.isHovering = screenMousePos.x >= pos.worldX and screenMousePos.x <= pos.worldX + rect.width
    --     and screenMousePos.y >= pos.worldY and screenMousePos.y <= pos.worldY + rect.height

    -- grab the rectangle and cycle its color over time
    local rect = GetComponent(entity, "Rectangle")
    self.time = self.time + dt
    local r = math.floor((math.sin(self.time) + 1) / 2 * 255)
    local g = math.floor((math.sin(self.time + 2) + 1)
        / 2 * 255)
    local b = math.floor((math.sin(self.time + 4) + 1) / 2 * 255)

    if self.isHovering then
        rect.border = string.format("#%02x%02x%02xFF", r, g, b)
    else
        rect.border = "#202020FF"
    end
end

function MenuButton:OnEvent(entity, event)
    if event.type == "MouseMoved" then
        local mouseX = event.x
        local mouseY = event.y

        local pos = GetComponent(entity, "Transform")
        local rect = GetComponent(entity, "Rectangle")

        self.isHovering = mouseX >= pos.worldX and mouseX <= pos.worldX + rect.width
            and mouseY >= pos.worldY and mouseY <= pos.worldY + rect.height
    end

    if event.type == "MouseButtonReleased" and event.button == MOUSE_LEFT then
        if self.isHovering then
            SceneReplace("MenuScene")
        end
    end
end
return MenuButton