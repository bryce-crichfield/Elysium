-- The cards: what each one costs and does, the decks the classes start with, and a unit's
-- piles (draw, hand, discard). Everything random goes through a seeded generator, so both sides
-- of a versus game shuffle the same way (lockstep).
--
-- A card costs mana (`cost`), paid from its user's crystals. Walking and the basic attack aren't
-- cards: they cost stamina (Scripts/Battler/Rules.lua).
-- `kind` is what playing it asks for:
--   bolt   - pick a spot within `range` tiles; everyone within `splash` tiles takes the hit
local Cards = {}

Cards.HAND_START = 3
Cards.HAND_MAX = 10

-- `art` is the picture on its face (Textures/Cards), multiplied by `tint` if it has one.
Cards.Catalog = {
    Bolt = { name = "Bolt", class = "Mage", kind = "bolt", range = {2, 5}, splash = 1, power = 3, stat = "int",
             cost = 2, text = "Blast a spot 2-5 tiles\naway for 3 + INT.", art = "Textures/Cards/magic_missile.png" },
}

-- A color around the hue wheel, at `hue` 0 to 1.
local function Hue(hue)
    local function channel(n)
        local k = (n + hue * 6) % 6
        return math.floor(255 * (1 - math.max(0, math.min(k, 4 - k, 1))) * 0.6 + 102)
    end
    return { r = channel(5), g = channel(3), b = channel(1), a = 255 }
end

-- Every card the party owns, in order, for the deck builder. A placeholder until there's a
-- collection: Bolt1..Bolt20, copies of Bolt under their own ids, each with its art tinted
-- (`tint`) a different color to tell them apart.
Cards.Collection = {}
for k = 1, 20 do
    local copy = {}
    for field, value in pairs(Cards.Catalog.Bolt) do copy[field] = value end
    copy.name = "Bolt " .. k
    copy.tint = Hue((k - 1) / 20)
    Cards.Catalog["Bolt" .. k] = copy
    Cards.Collection[k] = copy
end
for id, card in pairs(Cards.Catalog) do card.id = id end

-- The cards each class starts with. For now every class fights with Bolts, for testing.
Cards.Decks = {
    Warrior = { Bolt = 10 },
    Mage = { Bolt = 10 },
    Rogue = { Bolt = 10 },
}

-- --- Random ---------------------------------------------------------------------------------

-- A small generator both sides of a versus game step identically from the same seed.
function Cards.NewRng(seed)
    local state = math.floor(seed) % 2147483647
    if state <= 0 then state = state + 2147483646 end
    return function(n)  -- an integer in [1, n]
        state = (state * 48271) % 2147483647
        return state % n + 1
    end
end

local function Shuffle(list, rng)
    for i = #list, 2, -1 do
        local j = rng(i)
        list[i], list[j] = list[j], list[i]
    end
end

-- --- Piles ----------------------------------------------------------------------------------

-- A unit's piles from its class's deck, shuffled, with the opening hand drawn.
function Cards.NewPiles(deckName, rng)
    local piles = { draw = {}, hand = {}, discard = {} }
    local ids = {}
    for id in pairs(Cards.Decks[deckName]) do ids[#ids + 1] = id end
    table.sort(ids)  -- pairs order isn't fixed; the deck must be built the same everywhere
    for _, id in ipairs(ids) do
        for _ = 1, Cards.Decks[deckName][id] do piles.draw[#piles.draw + 1] = Cards.Catalog[id] end
    end
    Shuffle(piles.draw, rng)
    Cards.Draw(piles, Cards.HAND_START, rng)
    return piles
end

-- Draws `count` cards, shuffling the discard back in when the draw pile runs out. A card drawn
-- into a full hand is burned (discarded).
function Cards.Draw(piles, count, rng)
    for _ = 1, count do
        if #piles.draw == 0 then
            if #piles.discard == 0 then return end
            piles.draw, piles.discard = piles.discard, {}
            Shuffle(piles.draw, rng)
        end
        local card = table.remove(piles.draw)
        if #piles.hand < Cards.HAND_MAX then
            piles.hand[#piles.hand + 1] = card
        else
            piles.discard[#piles.discard + 1] = card
        end
    end
end

-- Moves the card in hand slot `slot` to the discard pile and returns it.
function Cards.Discard(piles, slot)
    local card = table.remove(piles.hand, slot)
    if card then piles.discard[#piles.discard + 1] = card end
    return card
end

-- --- Costs ----------------------------------------------------------------------------------

function Cards.Cost(card) return card.cost or 0 end

-- Whether `u` has the mana for `card` now.
function Cards.Affordable(u, card) return Cards.Cost(card) <= u.mana end

function Cards.Pay(u, card) u.mana = u.mana - Cards.Cost(card) end

return Cards
