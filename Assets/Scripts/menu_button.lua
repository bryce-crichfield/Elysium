local MenuButton = {}

-- Button states
MenuButton.STATE_NORMAL = "NORMAL"
MenuButton.STATE_HOVER = "HOVER"
MenuButton.STATE_PRESSED = "PRESSED"

-- Store original colors
MenuButton.normalBackground = nil
MenuButton.normalBorder = nil
MenuButton.state = MenuButton.STATE_NORMAL

function MenuButton.init(self, entity)
    Log("Initializing MenuButton for entity " .. entity)

    -- Store original colors for hover effects
    local rect = GetComponent(entity, "Rectangle")
    if rect then
        self.normalBackground = Color.new(rect.background.r, rect.background.g, rect.background.b, rect.background.a)
        self.normalBorder = Color.new(rect.border.r, rect.border.g, rect.border.b, rect.border.a)
    end

    self.state = MenuButton.STATE_NORMAL
end

function MenuButton.update(self, entity, dt)
    -- Could add animation here (pulse, etc)
end

function MenuButton.onEvent(self, entity, event)
    local name = GetComponent(entity, "Name")
    if not name then return end

    local buttonName = name.name
    local rect = GetComponent(entity, "Rectangle")

    -- Handle pick events (from PickSystem - entity was clicked)
    if event.type == "PickPress" then
        Log("Button pressed: " .. buttonName)
        self.state = MenuButton.STATE_PRESSED

        -- Darken on press
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

        -- Restore normal color
        if rect and self.normalBackground then
            rect.background = self.normalBackground
        end

        -- Trigger button action
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
        -- Could show an options panel or switch to options scene

    elseif buttonName == "QuitButton" then
        Log("Quit requested")
        -- Note: No quit function exposed yet, would need to add to Lua API
    end
end

return MenuButton
