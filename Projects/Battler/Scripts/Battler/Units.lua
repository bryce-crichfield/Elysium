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

-- The classes: three stats, red Strength, blue Intellect and green Agility, and the deck the
-- unit fights with (Cards.Decks). stamina: what it has to walk and swing with each phase.
-- rig: a 3D character (Units.Rigs). The other vitals come from the stats (Units.Vitals).
Units.Classes = {
    Marsh   = { prefab = "Prefabs/Character.xml", rig = "Knight", label = "Marsh", role = "Warrior", deck = "Warrior",
                str = 6, int = 1, agi = 3, stamina = 5, portrait = "Textures/Portraits/Knight.jpg" },
    Alexa   = { prefab = "Prefabs/Character.xml", rig = "Vampire", label = "Alexa", role = "Mage", deck = "Mage",
                str = 1, int = 6, agi = 3, stamina = 4, portrait = "Textures/Portraits/Bishop.jpg", cast = "Cast1" },
    Gryphon = { prefab = "Prefabs/Character.xml", rig = "Archer", label = "Gryphon", role = "Rogue", deck = "Rogue",
                str = 3, int = 1, agi = 6, stamina = 7, portrait = "Textures/Portraits/Archer.jpg" },
    Brigand = { prefab = "Prefabs/Character.xml", rig = "Knight", label = "Brigand", role = "Warrior", deck = "Warrior",
                str = 4, int = 0, agi = 2, stamina = 5, portrait = "Textures/Portraits/Militia.jpg" },
    Hexer   = { prefab = "Prefabs/Character.xml", rig = "Vampire", label = "Hexer", role = "Mage", deck = "Mage",
                str = 1, int = 4, agi = 2, stamina = 4, portrait = "Textures/Portraits/Worker.jpg", cast = "Cast2" },
    Poacher = { prefab = "Prefabs/Character.xml", rig = "Archer", label = "Poacher", role = "Rogue", deck = "Rogue",
                str = 3, int = 0, agi = 4, stamina = 6, portrait = "Textures/Portraits/Militia.jpg" },
}

-- What a class's stats make of its vitals: health (from Strength) and how many mana crystals it grows to (from Intellect, at most 10).
-- Mana starts at one crystal and grows by one a turn, refilling, until it reaches the cap.
function Units.Vitals(c)
    return {
        hp = 12 + 3 * c.str,
        manaCap = math.min(10, 3 + c.int),
    }
end

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
    local v = Units.Vitals(c)
    local u = {
        entity = e, class = c, name = c.label, team = team,
        hp = v.hp, maxHp = v.hp, tile = tile,
        mana = 0, manaMax = 0, manaCap = v.manaCap,   -- the first turn grows the first crystal
        stamina = 0, staminaMax = c.stamina,  -- filled when its first phase begins
        piles = { draw = {}, hand = {}, discard = {} },
        x = tile.x, y = tile.y, z = tile.z,
        facing = team == Units.PLAYER and "northeast" or "southwest",
        clip = "Idle", alive = true,
        shake = 0, flash = 0, shownHp = v.hp,
        rig = c.rig and Units.Rigs[c.rig],
    }
    if u.rig then
        local model = GetComponent(e, "Model")
        if model then
            model.model = u.rig.model
            model.scale = u.rig.scale
        end
    end
    u.ring = Units.FindRing(e)
    -- The prefab's HealthBar reads these (Scripts/Components/HealthBar.lua).
    local teamComp = GetComponent(e, "Team")
    if teamComp then teamComp.team = team end
    Units.SyncHealth(u)
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

-- Publish hp to the root's Health, which the HealthBar child draws.
function Units.SyncHealth(u)
    local health = GetComponent(u.entity, "Health")
    if health then
        health.max = u.maxHp
        health.current = u.alive and u.hp or 0
    end
end

function Units.Update(u, dt)
    Units.SyncHealth(u)
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

-- The prefab's "Ring" child (a flat fire-material circle at the unit's feet), if it has one.
-- Spawned names are namespaced per placement, so match the end of the name.
function Units.FindRing(root)
    for _, e in ipairs(GetChildren(root)) do
        local n = GetComponent(e, "Name")
        if n and n.name:sub(-4) == "Ring" then return e end
    end
    Log("Battler: no Ring child on unit " .. tostring(root))
    return nil
end

local RING_COLORS = {
    player = {x = 0.27, y = 0.63, z = 1.0},
    enemy  = {x = 1.0, y = 0.27, z = 0.23},
    spent  = {x = 0.45, y = 0.45, z = 0.5},
}
Units.RING_COLORS = RING_COLORS

-- The team ring: the prefab's Ring child, its fire in the team's color (grey once a player
-- unit has nothing left it can play), flared while active, hidden once dead.
function Units.DrawRing(u, active, time)
    if not u.ring then return end
    local key = u.team == Units.PLAYER and (u.spent and "spent" or "player") or "enemy"
    local layer = GetComponent(u.ring, "Layer")
    if layer then layer.isVisible = u.alive end
    local mat = GetComponent(u.ring, "Material")
    local fire = mat and mat:Layer("Fire")
    if fire then
        fire:Set("uGlowColor", RING_COLORS[key])
        fire:Set("uIntensity", active and (1.8 + 0.4 * math.sin(time * 6)) or (key == "spent" and 0.6 or 1.1))
        fire:Set("uGlowRadius", active and 9 or 6)
    end
end

return Units
