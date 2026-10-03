---@type EntityScript
---@class HealthBar
-- A health bar over its parent: reads the parent's Health (current / max) and Team, and
-- drives the Meter material. The fill eases down after a hit; hidden once the parent is dead.
local HealthBar = {}

local TEAM_COLORS = {
    [0] = {r = 80, g = 190, b = 255, a = 255},  -- player
    [1] = {r = 235, g = 70, b = 55, a = 255},   -- enemy
}
local DRAIN = 0.8  -- of the bar per second

function HealthBar:Initialize(entity)
    self.shown = nil
end

function HealthBar:Update(entity, dt)
    local parent = HasComponent(entity, "Parent") and GetComponent(entity, "Parent").parent
    if not parent or not HasComponent(parent, "Health") then return end
    local health = GetComponent(parent, "Health")
    local fraction = health.max > 0 and math.max(0, math.min(1, health.current / health.max)) or 0

    -- Snap up (heals), drain down (hits).
    if not self.shown or fraction > self.shown then
        self.shown = fraction
    else
        self.shown = math.max(fraction, self.shown - DRAIN * dt)
    end

    local layer = GetComponent(entity, "Layer")
    if layer then layer.isVisible = health.current > 0 end

    local mat = GetComponent(entity, "Material")
    local meter = mat and mat:Layer("Meter")
    if not meter then return end
    meter:Set("uProgress", self.shown)
    local team = HasComponent(parent, "Team") and GetComponent(parent, "Team").team or 0
    meter:Set("uFillColor", TEAM_COLORS[team] or TEAM_COLORS[0])
end

return HealthBar
