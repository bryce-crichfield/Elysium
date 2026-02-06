local Component = require("Scripts/Component")

local MenuButton = {
    STATE_NORMAL = "NORMAL",
    STATE_HOVER = "HOVER",
    STATE_PRESSED = "PRESSED",

    normalBackground = nil,
    normalBorder = nil,
    state = "NORMAL"
}

function MenuButton.Initialize(self, entity)
    Log("Initializing MenuButton for entity " .. entity)

    local rect = GetComponent(entity, "Rectangle")
    if rect then
        self.normalBackground = Color.new(rect.background.r, rect.background.g, rect.background.b, rect.background.a)
        self.normalBorder = Color.new(rect.border.r, rect.border.g, rect.border.b, rect.border.a)
    end

    self.state = MenuButton.STATE_NORMAL
end

function MenuButton.Update(self, entity, dt)
end

function MenuButton.OnEvent(self, entity, event)
    local name = GetComponent(entity, "Name")
    if not name then return end

    local buttonName = name.name
    local rect = GetComponent(entity, "Rectangle")

    if event.type == "PickPress" then
        Log("Button pressed: " .. buttonName)
        self.state = MenuButton.STATE_PRESSED

        if rect and self.normalBackground then
            rect.background = Color.new(
                math.max(0, self.normalBackground.r - 40),
                math.max(0, self.normalBackground.g - 40),
                math.max(0, self.normalBackground.b - 40),
                self.normalBackground.a
            )
        end

    elseif event.type == "PickRelease" then
        Log("Button released: " .. buttonName)
        self.state = MenuButton.STATE_NORMAL

        if rect and self.normalBackground then
            rect.background = self.normalBackground
        end

        self:OnClick(buttonName)
    end
end

function MenuButton:OnClick(buttonName)
    Log("Button clicked: " .. buttonName)

    if buttonName == "PlayButton" then
        Log("Starting game...")
        SceneReplace("ExploreScene")
    elseif buttonName == "OptionsButton" then
        Log("Options not implemented yet")
    elseif buttonName == "QuitButton" then
        Log("Quit requested")
    end
end

return MenuButton