-- A run through the dungeon: the state that outlives any one scene. A run starts at the
-- Campfire, which sends the party down to the floor's Dungeon. Each floor deals FLOOR_SIZE
-- encounters from ENCOUNTERS; walking into one plays it in the Battle scene. Win, and it's
-- cleared (the party keeps its wounds); clear the floor and it's back to the Campfire, which
-- heals the party and opens the next floor. Lose any battle and the run is over. Beat all
-- FLOORS floors and the dungeon is beaten.
--
-- The state lives in a global (like HeroFrames), so it survives scene changes; Run.Active()
-- is false outside a run (Skirmish, Versus), and the Battle then plays its default encounter.
local Run = {}

Run.FLOORS = 9
Run.FLOOR_SIZE = 3
Run.PARTY = { "Marsh", "Gryphon", "Alexa" }   -- the heroes, in their HeroN spawn order

-- Every encounter there is. `prefab` lays out the room and its spawn points (see
-- Prefabs/Encounters); `intro` is the flavour shown on walking in, before the fight.
Run.ENCOUNTERS = {
    { id = "Crypt", title = "The Crypt", prefab = "Prefabs/Encounters/Crypt.xml",
      intro = "Brigands have made camp among the tombs. They reach for their blades." },
    { id = "Hall", title = "The Pillared Hall", prefab = "Prefabs/Encounters/Hall.xml",
      intro = "Bowstrings creak on the dais. A voice calls: \"No further.\"" },
}
Run.DEFAULT = Run.ENCOUNTERS[1]

RunState = RunState or nil

-- FLOOR_SIZE encounters, without repeats while the pool lasts.
local function Deal()
    local pool, picked = {}, {}
    for _, e in ipairs(Run.ENCOUNTERS) do pool[#pool + 1] = e end
    for k = 1, Run.FLOOR_SIZE do
        if #pool == 0 then for _, e in ipairs(Run.ENCOUNTERS) do pool[#pool + 1] = e end end
        picked[k] = { encounter = table.remove(pool, Random(1, #pool)), cleared = false }
    end
    return picked
end

function Run.New()
    RunState = { floor = 1, slots = Deal(), wounds = {}, fighting = nil, at = nil }
end

function Run.Clear() RunState = nil end
function Run.Active() return RunState ~= nil end
function Run.State() return RunState end
function Run.Floor() return RunState and RunState.floor or 0 end

-- The floor's encounter slots: { encounter, cleared }, in dungeon order.
function Run.Slots() return RunState and RunState.slots or {} end

function Run.Cleared()
    local n = 0
    for _, s in ipairs(Run.Slots()) do if s.cleared then n = n + 1 end end
    return n
end

-- Slot k is about to be fought; `at` is where the party stood, to return to after.
function Run.Enter(k, at)
    RunState.fighting, RunState.at = k, at
end

-- The encounter the Battle should play: the one being fought, else the default.
function Run.Encounter()
    local slot = RunState and RunState.fighting and RunState.slots[RunState.fighting]
    return slot and slot.encounter or Run.DEFAULT
end

-- A hero's carried-over health, or nil (full).
function Run.Wounds(name) return RunState and RunState.wounds[name] end

-- The fight was won; `party` is the heroes as they ended it. Returns the scene to go to.
function Run.Won(party)
    local slot = RunState.slots[RunState.fighting]
    if slot then slot.cleared = true end
    RunState.fighting = nil
    for _, u in ipairs(party) do RunState.wounds[u.name] = u.alive and u.hp or 1 end
    return Run.Cleared() >= #RunState.slots and "Campfire" or "Dungeon"
end

-- The fight was lost: the run is over.
function Run.Lost() RunState = nil end

-- At the Campfire: heal up and deal the next floor. Returns false once the dungeon is beaten.
function Run.Descend()
    if Run.Cleared() >= #RunState.slots then RunState.floor = RunState.floor + 1 end
    if RunState.floor > Run.FLOORS then return false end
    RunState.slots, RunState.wounds, RunState.fighting, RunState.at = Deal(), {}, nil, nil
    return true
end

return Run
