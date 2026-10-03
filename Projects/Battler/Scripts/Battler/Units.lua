-- Unit classes, spawning, and the per-frame look of a unit: where it stands, which way it
-- faces, which clip plays, its health bar and ring.
local Board = require("Scripts/Battler/Board")

local Units = {}

Units.PLAYER, Units.ENEMY = 0, 1

-- range: {min, max} tiles for the basic attack. spell: an optional area attack.
Units.Classes = {
    Fighter = { prefab = "Prefabs/Fighter.xml", label = "Militia", hp = 26, atk = 8, def = 3, move = 4,
                range = {1, 1} },
    Mage    = { prefab = "Prefabs/Mage.xml", label = "Bishop", hp = 17, atk = 4, def = 1, move = 3,
                range = {1, 1}, spell = { name = "Bolt", power = 9, range = {2, 5}, splash = 1 } },
    Grunt   = { prefab = "Prefabs/Grunt.xml", label = "Brigand", hp = 20, atk = 7, def = 1, move = 4,
                range = {1, 1} },
    Hexer   = { prefab = "Prefabs/Mage.xml", label = "Hexer", hp = 14, atk = 3, def = 0, move = 3,
                range = {1, 1}, spell = { name = "Bolt", power = 7, range = {2, 4}, splash = 0 } },
}

local CLIP = { Idle = 0.12, Walk = 0.07, Attack = 0.065, Death = 0.085 }
local CLIP_FRAMES = 15

-- 8-way facing from a picture-space direction (+y is down the screen).
local function Facing(dx, dy)
    local a = math.deg(math.atan(dy, dx)) % 360
    local names = { "east", "southeast", "south", "southwest", "west", "northwest", "north", "northeast" }
    return names[math.floor((a + 22.5) / 45) % 8 + 1]
end

function Units.Spawn(className, team, tile)
    local c = Units.Classes[className]
    local e = SpawnPrefab(c.prefab, tile.x, tile.y, tile.z)
    if not e then
        Log("Battler: couldn't spawn " .. c.prefab)
        return nil
    end
    local u = {
        entity = e, class = c, name = c.label, team = team,
        hp = c.hp, maxHp = c.hp, tile = tile,
        x = tile.x, y = tile.y, z = tile.z,
        facing = team == Units.PLAYER and "northeast" or "southwest",
        clip = "Idle", moved = false, acted = false, alive = true,
        shake = 0, flash = 0, shownHp = c.hp,
    }
    Units.Play(u, "Idle")
    return u
end

-- Starts a clip from its first frame. Death holds on its last frame.
function Units.Play(u, clip)
    u.clip, u.clipTime = clip, 0
    local spr = GetComponent(u.entity, "Sprite")
    if spr then
        spr.sheet = clip
        spr.duration = CLIP[clip] or 0.08
        spr.frame = 0
    end
end

function Units.ClipLength(clip) return (CLIP[clip] or 0.08) * CLIP_FRAMES end

function Units.Face(u, tx, ty)
    local dx, dy = tx - u.x, ty - u.y
    if math.abs(dx) + math.abs(dy) > 0.01 then u.facing = Facing(dx, dy) end
end

function Units.Update(u, dt)
    u.clipTime = (u.clipTime or 0) + dt
    if u.clip == "Death" and u.clipTime >= Units.ClipLength("Death") - CLIP.Death then
        local spr = GetComponent(u.entity, "Sprite")
        if spr then spr.frame = CLIP_FRAMES - 1; spr.duration = 1e9 end
    elseif u.clip == "Attack" and u.clipTime >= Units.ClipLength("Attack") then
        Units.Play(u, "Idle")
    end
    u.shake = math.max(0, u.shake - dt)
    u.flash = math.max(0, u.flash - dt)
    -- The bar drains toward the real value.
    if u.shownHp > u.hp then u.shownHp = math.max(u.hp, u.shownHp - dt * u.maxHp * 0.8) end

    local spr = GetComponent(u.entity, "Sprite")
    if spr then spr.sequence = u.facing end
    local t = GetComponent(u.entity, "Transform")
    if t then
        local jitter = u.shake > 0 and math.sin(u.shake * 90) * 4 or 0
        t.localX, t.localY, t.localZ = u.x + jitter, u.y, u.z
    end
end

-- Ring and health bar, drawn on the ground layers.
function Units.DrawRing(u, active, time)
    if not u.alive then return end
    local x, y = Board.Lift(u.x, u.y, u.z)
    local c = u.team == Units.PLAYER and {r = 70, g = 160, b = 255, a = 150} or {r = 255, g = 70, b = 60, a = 150}
    if u.acted and u.team == Units.PLAYER then c = {r = 120, g = 120, b = 130, a = 110} end
    local pulse = active and (1 + 0.08 * math.sin(time * 6)) or 1
    DrawEllipse(x, y, 30 * pulse, 15 * pulse, c, "overlay")
end

function Units.DrawBar(u)
    if not u.alive then return end
    local x, y = Board.Lift(u.x, u.y, u.z)
    local w, h = 44, 5
    local bx, by = x - w / 2, y - 112
    FillRect(bx - 1, by - 1, w + 2, h + 2, {r = 10, g = 10, b = 14, a = 220}, "fx")
    local shown = u.shownHp / u.maxHp
    local fill = u.hp / u.maxHp
    FillRect(bx, by, w * shown, h, {r = 255, g = 240, b = 200, a = 230}, "fx")
    local c = u.team == Units.PLAYER and {r = 80, g = 190, b = 255, a = 255} or {r = 235, g = 70, b = 55, a = 255}
    FillRect(bx, by, w * fill, h, c, "fx")
end

return Units
