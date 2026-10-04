-- The cards: what each one costs and does, the decks the classes start with, and a unit's
-- piles (draw, hand, discard). Everything random goes through a seeded generator, so both sides
-- of a versus game shuffle the same way (lockstep).
--
-- A card's cost is paid from the three vitals: health (red), stamina (green), mana (blue).
-- `kind` is what playing it asks for:
--   move   - pick a spot within `move` tiles to walk to
--   attack - pick a foe within `range` tiles to hit for `power` + the user's `stat`
--   bolt   - pick a spot within `range` tiles; everyone within `splash` tiles takes the hit
local Cards = {}

Cards.HAND_START = 7
Cards.HAND_MAX = 10
Cards.DECK_SIZE = 30

-- `art` is the placeholder picture's color until there's real art.
Cards.Catalog = {
    Move = { name = "Move", class = "Basic", kind = "move", move = 3,
             cost = { stamina = 2 }, text = "Walk up to 3 tiles.", art = {r = 70, g = 150, b = 90, a = 255} },
    Attack = { name = "Strike", class = "Basic", kind = "attack", range = {1, 1}, power = 2, stat = "str",
               cost = { stamina = 1 }, text = "Hit an adjacent foe\nfor 2 + STR.", art = {r = 170, g = 70, b = 60, a = 255} },
    Bolt = { name = "Bolt", class = "Mage", kind = "bolt", range = {2, 5}, splash = 1, power = 3, stat = "int",
             cost = { mana = 2 }, text = "Blast a spot 2-5 tiles\naway for 3 + INT.", art = {r = 80, g = 110, b = 220, a = 255} },
}
for id, card in pairs(Cards.Catalog) do card.id = id end

-- The 30 cards each class starts with: mostly the basics, plus its own.
Cards.Decks = {
    Warrior = { Move = 14, Attack = 16 },
    Mage = { Move = 12, Attack = 8, Bolt = 10 },
    Rogue = { Move = 16, Attack = 14 },
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

function Cards.Cost(card, vital) return card.cost[vital] or 0 end

-- Whether `u` can pay for `card` now. Health can't be spent down to nothing.
function Cards.Affordable(u, card)
    return Cards.Cost(card, "health") < u.hp
        and Cards.Cost(card, "stamina") <= u.stamina
        and Cards.Cost(card, "mana") <= u.mana
end

function Cards.Pay(u, card)
    u.hp = u.hp - Cards.Cost(card, "health")
    u.stamina = u.stamina - Cards.Cost(card, "stamina")
    u.mana = u.mana - Cards.Cost(card, "mana")
end

-- The first card of `kind` in `u`'s hand it can pay for: slot, card.
function Cards.Find(u, kind)
    for slot, card in ipairs(u.piles.hand) do
        if card.kind == kind and Cards.Affordable(u, card) then return slot, card end
    end
    return nil
end

return Cards
