---@type EntityScript
---@class Crystal
-- Prefabs/Crystal.xml: one mana crystal. The gem is mana.png; the Glow under it is a square
-- turned 45 degrees with a Fire outline, so it burns around the diamond: the crystal's charge.
-- Whoever places it binds a view under the crystal root's full name (a nested placement's
-- id is its parent's, so the name is what's unique):
--
--   Widgets.BindView(GetComponent(crystal, "Name").name, { charged = true })
--
-- Charged, it's bright and its light pulses; spent, the light goes out and the gem dims. Coming
-- back to charge it flares; spending it pops (the Burst child, Shaders/Sdf/Material/Burst.glsl). Showing and hiding it is the placer's business.
local Widgets = require("Scripts/Components/Widgets")

local Crystal = {}

local LIT, DARK = 1.0, 0.0
local FLARE = 2.5              -- the burst when it charges back up
local POP = 0.45               -- seconds the spend pop's sparks fly
local CHARGED_TINT = {r = 255, g = 255, b = 255, a = 255}
local SPENT_TINT = {r = 70, g = 80, b = 110, a = 170}

local function Lerp(a, b, k) return a + (b - a) * k end

function Crystal:Initialize(entity)
    local name = GetComponent(entity, "Name")
    self.key = name and name.name
    self.glow = Widgets.Child(entity, "Glow")
    self.gem = Widgets.Child(entity, "Gem")
    self.burst = Widgets.Child(entity, "Burst")
    self.charge, self.flare, self.time, self.was = 1, 0, math.random() * 6, nil
    self.pop = nil  -- the pop's progress, 0 to 1, while it plays
end

function Crystal:Update(entity, dt)
    self.time = self.time + dt
    local view = self.key and Widgets.Binding(self.key)
    local charged = not view or view.charged ~= false
    if charged and self.was == false then self.flare = FLARE end
    if not charged and self.was == true then self.pop, self.seed = 0, math.random() * 100 end
    self.was = charged
    self.flare = math.max(0, self.flare - dt * 5)
    self.charge = Widgets.Ease(self.charge, charged and LIT or DARK, 10, dt)

    local mat = self.glow and GetComponent(self.glow, "Material")
    local fire = mat and mat:Layer("Fire")
    if fire then fire:Set("uIntensity", self.charge * (1.1 + 0.25 * math.sin(self.time * 3)) + self.flare) end

    if self.pop then
        self.pop = self.pop + dt / POP
        if self.pop >= 1 then self.pop = nil end
    end
    mat = self.burst and GetComponent(self.burst, "Material")
    local burst = mat and mat:Layer("Burst")
    if burst then
        burst:Set("uProgress", self.pop or 1)
        if self.seed then burst:Set("uSeed", self.seed) end
    end

    mat = self.gem and GetComponent(self.gem, "Material")
    local tex = mat and mat:Layer("Texture")
    if tex then
        local k = self.charge
        tex:Set("uTint", {
            r = math.floor(Lerp(SPENT_TINT.r, CHARGED_TINT.r, k)), g = math.floor(Lerp(SPENT_TINT.g, CHARGED_TINT.g, k)),
            b = math.floor(Lerp(SPENT_TINT.b, CHARGED_TINT.b, k)), a = math.floor(Lerp(SPENT_TINT.a, CHARGED_TINT.a, k)),
        })
    end
end

return Crystal
