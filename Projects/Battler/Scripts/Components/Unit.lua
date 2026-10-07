---@type EntityScript
---@class Unit
-- Prefabs/Character.xml: a unit's legs. Units.Spawn binds the unit (Scripts/Battler/Units.lua)
-- under its root entity (Widgets.BindView(entity, u)); whatever moves it only changes u.x and
-- u.y, and this plays Walk while it moves and Idle once it stops. One-shots (attacks, casts, a
-- hurt, death) are still started with Units.Play, and left to finish.
local Widgets = require("Scripts/Components/Widgets")
local Units = require("Scripts/Battler/Units")

local Unit = {}

local MOVING = 20     -- ground units a second that count as walking
local SETTLE = 0.12   -- seconds it stays still before it idles (no flicker between path legs)

function Unit:Initialize(entity)
    self.x, self.y, self.still = nil, nil, SETTLE
end

function Unit:Update(entity, dt)
    local u = Widgets.Binding(entity)
    if not u or dt <= 0 then return end
    local speed = self.x and NavGroundDistance(self.x, self.y, u.x, u.y) / dt or 0
    self.x, self.y = u.x, u.y
    self.still = speed > MOVING and 0 or self.still + dt
    if not u.alive or not Units.Locomotion(u.clip) then return end
    local clip = self.still < SETTLE and "Walk" or "Idle"
    if u.clip ~= clip then Units.Play(u, clip) end
end

return Unit
