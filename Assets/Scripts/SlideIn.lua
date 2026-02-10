local Component = require("Scripts/Component")

local SlideIn = {}

function SlideIn.Initialize(self, entity)
    local position = GetComponent(entity, "PositionComponent")
    if not position then
        return
    end
    
    -- Store the target position (where it currently is in the scene)
    self.targetX = position.x
    self.targetY = position.y
    
    -- Configuration
    self.slideDistance = 5.0  -- How far off-screen to start
    self.slideDirection = {x = -1.0, y = 0.0}  -- Slide from left by default
    self.slideDuration = 1.0  -- Time to complete the slide
    self.timeAccumulator = 0.0
    
    -- Calculate and set the starting position
    local startX = self.targetX + (self.slideDirection.x * self.slideDistance)
    local startY = self.targetY + (self.slideDirection.y * self.slideDistance)
    
    position.x = startX
    position.y = startY
end

function SlideIn.Update(self, entity, dt)
    local position = GetComponent(entity, "PositionComponent")
    if not position then
        return
    end
    
    -- Only animate if we haven't finished yet
    if self.timeAccumulator >= self.slideDuration then
        -- Ensure we're exactly at target position
        position.x = self.targetX
        position.y = self.targetY
        return
    end
    
    self.timeAccumulator = self.timeAccumulator + dt
    
    -- Normalize time to 0-1 range
    local normalizedTime = math.min(self.timeAccumulator / self.slideDuration, 1.0)
    
    -- Apply easing (smoothstep for nice acceleration/deceleration)
    local easedTime = normalizedTime * normalizedTime * (3.0 - 2.0 * normalizedTime)
    
    -- Calculate start position
    local startX = self.targetX + (self.slideDirection.x * self.slideDistance)
    local startY = self.targetY + (self.slideDirection.y * self.slideDistance)
    
    -- Interpolate from start to target
    position.x = startX + (self.targetX - startX) * easedTime
    position.y = startY + (self.targetY - startY) * easedTime

    Component.Add(entity, "PositionComponent", position)
end

return SlideIn