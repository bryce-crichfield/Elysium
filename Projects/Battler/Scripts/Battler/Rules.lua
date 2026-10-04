-- Combat rules for free movement: positions are ground points (x, y, z), distances are ground
-- distances (NavGroundDistance: a picture y counts twice an x, so a radius is a circle on the
-- ground). Moving is a NavReach budget; ranges and splashes are radii.
local Rules = {}

local LEVEL = 16          -- the height of one step, in world units
Rules.TILE = 90           -- about one old tile, in ground units
Rules.UNIT_RADIUS = 24    -- how close two units may stand is twice this
Rules.MELEE = 80          -- centre to centre
Rules.HEIGHT_REACH = 40   -- extra range per level a ranged attacker looks down from

function Rules.Distance(a, b) return NavGroundDistance(a.x, a.y, b.x, b.y) end

local function Levels(from, to) return math.floor((from.z - to.z) / LEVEL + 0.5) end

-- Everything below reads the card being played (Scripts/Battler/Cards.lua): its reach in
-- tiles, its power, and the stat of the user's that adds to it.

-- How far a move card walks, in ground units.
function Rules.MoveBudget(card) return card.move * Rules.TILE end

-- A card's reach, in ground units: {min, max}. A reach of one tile is melee.
function Rules.Range(card)
    if card.range[2] <= 1 then return { 0, Rules.MELEE } end
    return { card.range[1] * Rules.TILE - 30, card.range[2] * Rules.TILE }
end

function Rules.SplashRadius(card) return card.splash * Rules.TILE * 0.8 + 30 end

function Rules.Power(u, card) return card.power + (card.stat and u.class[card.stat] or 0) end

Rules.EYE = 44      -- where an attacker looks from, above its floor
Rules.AIM = 24      -- where it aims at, above the target's floor

-- Whether a sight line runs from `a` to `b` (floor points), clear of terrain and pillars.
function Rules.Sees(a, b)
    return NavCanSee(a.x, a.y, a.z + Rules.EYE, b.x, b.y, b.z + Rules.AIM)
end

-- Whether an attack with `range` from point `a` reaches point `b`: in range and in sight.
-- Melee reaches about one step up or down; ranged reaches further for each level it looks
-- down from.
function Rules.Reaches(range, a, b)
    local d = Rules.Distance(a, b)
    local dz = Levels(a, b)
    if range[1] == 0 then
        if d > range[2] or math.abs(dz) > 1 then return false end
    elseif d < range[1] or d > range[2] + math.max(0, dz) * Rules.HEIGHT_REACH then
        return false
    end
    return Rules.Sees(a, b)
end

-- The longest an attack with `range` can reach from a unit at `a` (height included).
function Rules.MaxReach(range, a)
    if range[1] == 0 then return range[2] end
    return range[2] + 4 * Rules.HEIGHT_REACH
end

-- A hit of `power` from `from` on someone at `to`: harder from above, softer from below.
function Rules.Damage(from, to, power)
    local dz = Levels(from, to)
    local bonus = dz > 0 and 2 or (dz < 0 and -1 or 0)
    return math.max(1, power + bonus)
end

-- The circles everyone but `u` takes up, for NavReach.
function Rules.Blockers(units, u)
    local list = {}
    for _, o in ipairs(units) do
        if o.alive and o ~= u then
            list[#list + 1] = { x = o.x, y = o.y, r = Rules.UNIT_RADIUS * 2 }
        end
    end
    return list
end

function Rules.Reach(units, u, budget)
    return NavReach(u.x, u.y, u.z, budget, Rules.Blockers(units, u))
end

-- The units within `radius` of `center`.
function Rules.Splash(units, center, radius)
    local hit = {}
    for _, v in ipairs(units) do
        if v.alive and Rules.Distance(v, center) <= radius + Rules.UNIT_RADIUS * 0.5 then hit[#hit + 1] = v end
    end
    return hit
end

-- A blast's damage to `v` from `center`: full near the middle, half at the edge.
function Rules.SplashPower(caster, card, v, center)
    local power = Rules.Power(caster, card)
    return Rules.Distance(v, center) <= 30 and power or math.floor(power / 2)
end

-- Cuts a waypoint path to the first `budget` ground units of it.
function Rules.Truncate(from, path, budget)
    local out, prev, left = {}, from, budget
    for _, p in ipairs(path) do
        local d = Rules.Distance(prev, p)
        if d <= left then
            out[#out + 1] = p
            left = left - d
            prev = p
        else
            local k = left / d
            out[#out + 1] = { x = prev.x + (p.x - prev.x) * k, y = prev.y + (p.y - prev.y) * k,
                              z = prev.z + (p.z - prev.z) * k }
            break
        end
    end
    return out
end

-- Points around `foe` at `distance`, on the floor (nil where there's none).
local function Ring(foe, distance, count)
    local list = {}
    for k = 0, count - 1 do
        local a = k / count * math.pi * 2
        -- A ground circle: picture y is half as long.
        local x, y = foe.x + math.cos(a) * distance, foe.y + math.sin(a) * distance * 0.5
        local z = NavFloorHeight(x, y, foe.z)
        if z then list[#list + 1] = { x = x, y = y, z = z } end
    end
    return list
end

-- The AI's turn for `u`, from what it can play: `moveBudget` (ground units, 0 without a move
-- card) and `options`, its attack cards ({card = ...}). Returns {path, option, target, at}:
-- where to walk, then which option to play on whom (option nil: just walk).
function Rules.Plan(units, u, moveBudget, options)
    local foes = {}
    for _, o in ipairs(units) do
        if o.alive and o.team ~= u.team then foes[#foes + 1] = o end
    end
    if #foes == 0 then return nil end

    local here = { x = u.x, y = u.y, z = u.z }
    local reach = moveBudget > 0 and Rules.Reach(units, u, moveBudget) or nil

    local best, bestScore = nil, -1e9
    for _, foe in ipairs(foes) do
        for _, opt in ipairs(options) do
            local range = Rules.Range(opt.card)
            local power = Rules.Power(u, opt.card)
            -- Where to stand: here, or around the foe at a range that works.
            local spots = { here }
            if reach then
                local distances = range[1] == 0 and { Rules.MELEE - 22 }
                    or { range[2] - 20, (range[1] + range[2]) * 0.5 }
                for _, d in ipairs(distances) do
                    for _, p in ipairs(Ring(foe, d, 16)) do spots[#spots + 1] = p end
                end
            end
            for _, p in ipairs(spots) do
                local cost = p == here and 0 or reach:Cost(p.x, p.y, p.z)
                if cost and Rules.Reaches(range, p, foe) then
                    local dmg = Rules.Damage(p, foe, power)
                    local score = dmg * 10 + (dmg >= foe.hp and 200 or 0) - cost * 0.02
                    if range[1] > 0 then score = score + Rules.Distance(p, foe) * 0.03 end
                    if score > bestScore then
                        bestScore = score
                        best = { at = p, option = opt, target = foe }
                    end
                end
            end
        end
    end

    if best then
        best.path = best.at == here and {} or reach:PathTo(best.at.x, best.at.y, best.at.z)
        return best
    end
    if moveBudget <= 0 then return { path = {} } end

    -- Nobody in reach: walk as far as the budget allows toward the closest foe, by walking
    -- distance (a reach big enough to cover the field).
    local far = Rules.Reach(units, u, 5000)
    if not far then return { path = {} } end
    local goal, goalCost = nil, 1e9
    for _, foe in ipairs(foes) do
        for _, p in ipairs(Ring(foe, Rules.MELEE - 22, 16)) do
            local c = far:Cost(p.x, p.y, p.z)
            if c and c < goalCost then goal, goalCost = p, c end
        end
    end
    if not goal then return { path = {} } end
    return { path = Rules.Truncate(here, far:PathTo(goal.x, goal.y, goal.z), moveBudget) }
end

return Rules
