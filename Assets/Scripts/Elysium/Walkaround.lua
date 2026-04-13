---@type EntityScript
---@class Walkaround : Unit
local Unit = require("Scripts/Elysium/Unit")
local Walkaround = setmetatable({}, {__index = Unit})

local WANDER_RADIUS = 160   -- world-px radius around home position
local WAIT_MIN      = 1.5   -- seconds paused at each destination
local WAIT_MAX      = 3.5

local function randomInRadius(cx, cy, radius)
    local angle = math.random() * 2 * math.pi
    local dist  = math.sqrt(math.random()) * radius  -- sqrt for uniform disc distribution
    return cx + math.cos(angle) * dist,
           cy + math.sin(angle) * dist
end

function Walkaround:Initialize(entity)
    Unit.Initialize(self, entity)

    local pos = GetComponent(entity, "Position")
    self.homeX = pos and pos.x or 0
    self.homeY = pos and pos.y or 0

    self.phase    = "waiting"
    -- Stagger first move so all units don't depart at the same instant
    self.waitTime = math.random() * 2.0
end

function Walkaround:Update(entity, dt)
    Unit.Update(self, entity, dt)   -- animation (sheet + direction)

    local mv = GetComponent(entity, "Movement")
    if not mv then return end

    if self.phase == "moving" then
        -- mv.state 0 = Idle (arrived or no path)
        if mv.state == 0 then
            self.phase    = "waiting"
            self.waitTime = WAIT_MIN + math.random() * (WAIT_MAX - WAIT_MIN)
        end

    elseif self.phase == "waiting" then
        self.waitTime = self.waitTime - dt
        if self.waitTime <= 0 then
            local wx, wy = randomInRadius(self.homeX, self.homeY, WANDER_RADIUS)
            IssueMoveCommand(entity, wx, wy)
            self.phase = "moving"
        end
    end
end

return Walkaround
