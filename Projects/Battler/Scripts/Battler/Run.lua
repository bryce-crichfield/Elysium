-- An adventure: the state that outlives any one scene. It starts in the Town, whose Portal
-- leads to the next encounter from ENCOUNTERS; walking through plays it in the Battle scene.
-- Win or lose, the party comes back to the Town (rested); a win deals a new encounter, a loss
-- leaves the same one waiting. Contracts will choose the encounter later.
--
-- The state lives in a global (like HeroFrames), so it survives scene changes; Run.Active()
-- is false outside one (Skirmish, Versus), and the Battle then plays its default encounter.
local Cards = require("Scripts/Battler/Cards")

local Run = {}

Run.PARTY = { "Knight", "Archer", "Vampire" }   -- the heroes, in their HeroN spawn order

-- Every encounter there is. `prefab` lays out the room and its spawn points (see
-- Prefabs/Encounters); `intro` is the flavour shown on stepping into the Portal.
Run.ENCOUNTERS = {
    { id = "Crypt", title = "The Crypt", prefab = "Prefabs/Encounters/Crypt.xml",
      intro = "Knights have made camp among the tombs. They reach for their blades." },
    { id = "Hall", title = "The Pillared Hall", prefab = "Prefabs/Encounters/Hall.xml",
      intro = "Bowstrings creak on the dais. A voice calls: \"No further.\"" },
}
Run.DEFAULT = Run.ENCOUNTERS[1]

RunState = RunState or nil

-- A random encounter, other than `last` while there's a choice.
local function Deal(last)
    local pool = {}
    for _, e in ipairs(Run.ENCOUNTERS) do if e ~= last then pool[#pool + 1] = e end end
    if #pool == 0 then return last end
    return pool[Random(1, #pool)]
end

function Run.New()
    RunState = { next = Deal(), wins = 0, fighting = false, at = nil }
end

-- A party member's deck in the deck builder (Scenes/CharacterSheet.xml): a list of card ids
-- the sheet edits in place. A placeholder until battles deal from it: it starts as the
-- collection's first DECK_START cards.
local DECK_START = 5
function Run.Deck(name)
    RunState.decks = RunState.decks or {}
    if not RunState.decks[name] then
        local deck = {}
        for k = 1, DECK_START do deck[k] = Cards.Collection[k].id end
        RunState.decks[name] = deck
    end
    return RunState.decks[name]
end

function Run.Clear() RunState = nil end
function Run.Active() return RunState ~= nil end
function Run.State() return RunState end
function Run.Next() return RunState and RunState.next end
function Run.Wins() return RunState and RunState.wins or 0 end

-- Into the Portal; `at` is where the party stood, to come back to after.
function Run.Enter(at)
    RunState.fighting, RunState.at = true, at
end

-- The encounter the Battle should play: the one being fought, else the default.
function Run.Encounter()
    return RunState and RunState.fighting and RunState.next or Run.DEFAULT
end

-- The fight was won. Returns the scene to go to.
function Run.Won()
    RunState.fighting, RunState.wins = false, RunState.wins + 1
    RunState.next = Deal(RunState.next)
    return "Town"
end

-- The fight was lost: back to the Town, the same encounter still waiting.
function Run.Lost()
    RunState.fighting = false
    return "Town"
end

return Run
