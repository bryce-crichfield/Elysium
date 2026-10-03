-- Unit classes, spawning, and the per-frame look of a unit: where it stands, which way it
-- faces, which clip plays, its health bar and ring.
local Board = require("Scripts/Battler/Board")

local Units = {}

Units.HEAD = 130   -- the height of the health bar over a unit's feet (a unit is ~105 tall)

Units.PLAYER, Units.ENEMY = 0, 1

-- The 3D characters: the model, and the file for each clip the battle plays (Idle, Walk,
-- Attack, Hurt, Death, Cast1..3). A clip a rig doesn't have keeps whatever is playing.
-- scale: world units per model unit (the models are in meters). runRate: the Walk clip's
-- playback rate, to match the feet to the walking speed.
local function Clips(folder, names)
    local clips = {}
    for clip, file in pairs(names) do clips[clip] = folder .. file .. ".anim" end
    return clips
end

Units.Rigs = {
    Knight = {
        model = "Models/Characters/Knight/Knight.mesh", scale = 25, runRate = 1,
        -- No Attack clip baked: the swing is Cast1.
        clips = Clips("Animations/Knight/Knight_", {
            Idle = "Idle", Walk = "Run", Attack = "Cast1", Hurt = "Hurt", Death = "Death",
            Cast1 = "Cast1", Cast2 = "Cast2", Cast3 = "Cast3" }),
    },
    Vampire = {
        model = "Models/Characters/Vampire/Vampire.mesh", scale = 25, runRate = 1,
        clips = Clips("Animations/Vampire/Vampire_", {
            Idle = "Idle", Walk = "Running", Attack = "Attack", Hurt = "Hurt", Death = "Death",
            Cast1 = "Cast_1", Cast2 = "Cast_2", Cast3 = "Cast_3" }),
    },
    Archer = {
        model = "Models/Characters/Archer/Archer.mesh", scale = 25, runRate = 1,
        clips = Clips("Animations/Archer/Archer_", {
            Idle = "Idle", Walk = "Run", Attack = "Attack", Hurt = "Hurt", Death = "Death" }),
    },
}

-- range: {min, max} tiles for the basic attack. spell: an optional area attack, played with
-- the rig's `cast` clip. rig: a 3D character (Units.Rigs); without one the prefab's sprite plays.
Units.Classes = {
    Fighter = { prefab = "Prefabs/Character.xml", rig = "Knight", label = "Knight", hp = 26, atk = 8, def = 3, move = 4,
                range = {1, 1} },
    Mage    = { prefab = "Prefabs/Character.xml", rig = "Vampire", label = "Vampire", hp = 17, atk = 4, def = 1, move = 3,
                range = {1, 1}, spell = { name = "Bolt", power = 9, range = {2, 5}, splash = 1, cast = "Cast1" } },
    Archer  = { prefab = "Prefabs/Character.xml", rig = "Archer", label = "Archer", hp = 18, atk = 6, def = 1, move = 4,
                range = {2, 5} },
    Grunt   = { prefab = "Prefabs/Character.xml", rig = "Knight", label = "Brigand", hp = 20, atk = 7, def = 1, move = 4,
                range = {1, 1} },
    Hexer   = { prefab = "Prefabs/Character.xml", rig = "Vampire", label = "Hexer", hp = 14, atk = 3, def = 0, move = 3,
                range = {1, 1}, spell = { name = "Bolt", power = 7, range = {2, 4}, splash = 0, cast = "Cast2" } },
    Poacher = { prefab = "Prefabs/Character.xml", rig = "Archer", label = "Poacher", hp = 15, atk = 5, def = 0, move = 4,
                range = {2, 4} },
}

local CLIP = { Idle = 0.12, Walk = 0.07, Attack = 0.065, Death = 0.085 }

-- Clips that play once, then back to Idle. Death plays once and holds.
local ONE_SHOT = { Attack = true, Hurt = true, Cast1 = true, Cast2 = true, Cast3 = true }
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
        rig = c.rig and Units.Rigs[c.rig],
    }
    if u.rig then
        local model = GetComponent(e, "Model")
        if model then
            model.model = u.rig.model
            model.scale = u.rig.scale
        end
    end
    -- Start facing the other side.
    if team == Units.PLAYER then Units.Face(u, u.x + 1, u.y - 1) else Units.Face(u, u.x - 1, u.y + 1) end
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
    local anim = u.rig and u.rig.clips[clip] and GetComponent(u.entity, "Animation")
    if anim then
        anim.clip = u.rig.clips[clip]  -- crossfades from the current pose
        anim.loop = not ONE_SHOT[clip] and clip ~= "Death"
        anim.speed = clip == "Walk" and u.rig.runRate or 1
        anim.playing = true
        anim.time = 0
    end
end

-- How long `clip` lasts. For a rig, the clip `u` just started: inside a coroutine this waits
-- (a frame or two) for it to load; outside one, or if it never does, the sprite length.
function Units.ClipLength(clip, u)
    local anim = u and u.rig and GetComponent(u.entity, "Animation")
    if anim and coroutine.isyieldable() then
        local waited = 0
        while (anim.current ~= anim.clip or anim.duration <= 0) and waited < 1 do
            waited = waited + coroutine.yield()
        end
        if anim.duration > 0 then return anim.duration / math.max(anim.speed, 0.01) end
    end
    return (CLIP[clip] or 0.08) * CLIP_FRAMES
end

function Units.Face(u, tx, ty)
    local dx, dy = tx - u.x, ty - u.y
    if math.abs(dx) + math.abs(dy) > 0.01 then
        u.facing = Facing(dx, dy)
        -- A 3D model turns smoothly: degrees about up, with +z (the model's front) toward
        -- (dx, dy) on the ground (a picture y is twice as long in GL).
        u.yaw = math.deg(math.atan(-dx, 2 * dy))
    end
end

function Units.Update(u, dt)
    u.clipTime = (u.clipTime or 0) + dt
    local anim = u.rig and GetComponent(u.entity, "Animation")
    if anim then
        if ONE_SHOT[u.clip] and u.alive and u.clipTime > 0.1 and anim.current == anim.clip and anim.finished then Units.Play(u, "Idle") end
    elseif u.clip == "Death" and u.clipTime >= Units.ClipLength("Death") - CLIP.Death then
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
    local model = GetComponent(u.entity, "Model")
    if model and u.yaw then model.yaw = u.yaw end
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

-- Over the unit's head, on the screen ("ui"), so it stays upright however the camera turns.
function Units.DrawBar(u)
    if not u.alive then return end
    local x, y = ViewProject(u.x, u.y, u.z + Units.HEAD)
    local w, h = 44, 5
    local bx, by = x - w / 2, y
    FillRect(bx - 1, by - 1, w + 2, h + 2, {r = 10, g = 10, b = 14, a = 220}, "ui")
    local shown = u.shownHp / u.maxHp
    local fill = u.hp / u.maxHp
    FillRect(bx, by, w * shown, h, {r = 255, g = 240, b = 200, a = 230}, "ui")
    local c = u.team == Units.PLAYER and {r = 80, g = 190, b = 255, a = 255} or {r = 235, g = 70, b = 55, a = 255}
    FillRect(bx, by, w * fill, h, c, "ui")
end

return Units
