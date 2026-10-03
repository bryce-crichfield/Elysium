-- Combat rules (ranges, damage) and the enemy's planning. Nothing here touches the world; it
-- only reads the board and the units, so the AI can try every option cheaply.
local Board = require("Scripts/Battler/Board")

local Rules = {}

local LEVEL = 16   -- the height of one step, in world units

local function Levels(from, to) return math.floor((from.z - to.z) / LEVEL + 0.5) end

-- Whether an attack with `range` ({min, max}) from tile `a` reaches tile `b`. Melee reaches one
-- step up or down; ranged attacks reach a tile further for each level they look down from.
function Rules.Reaches(range, a, b)
    local d = Board.Manhattan(a, b)
    local dz = Levels(a, b)
    if range[2] <= 1 then return d == 1 and math.abs(dz) <= 1 end
    local max = range[2] + math.max(0, dz)
    return d >= range[1] and d <= max
end

-- The damage `att` standing on `from` does to `def` standing on `to` with `power`.
function Rules.Damage(att, from, def, to, power)
    local dz = Levels(from, to)
    local bonus = dz > 0 and 2 or (dz < 0 and -1 or 0)
    return math.max(1, power + bonus - def.class.def)
end

function Rules.AttackPower(u) return u.class.atk end

function Rules.UnitAt(units, tile)
    for _, u in ipairs(units) do
        if u.alive and u.tile == tile then return u end
    end
    return nil
end

-- Where `u` can end its move this turn: {key -> reach node}, without tiles others stand on.
function Rules.MoveRange(board, units, u)
    local reach = board:Reach(u.tile, u.moved and 0 or u.class.move, function(tile)
        local other = Rules.UnitAt(units, tile)
        return not other or other.team == u.team
    end)
    for k, node in pairs(reach) do
        local other = Rules.UnitAt(units, node.tile)
        if other and other ~= u then node.blocked = true end
    end
    return reach
end

-- Splash: the units within `radius` tiles of `center`.
function Rules.Splash(units, center, radius)
    local hit = {}
    for _, u in ipairs(units) do
        if u.alive and Board.Manhattan(u.tile, center) <= radius then hit[#hit + 1] = u end
    end
    return hit
end

-- The enemy's turn for `u`: where to go and what to do there.
-- Returns {tile = destination, path = tiles, action = "attack"|"spell"|nil, target = unit/tile}.
function Rules.Plan(board, units, u)
    local foes = {}
    for _, o in ipairs(units) do
        if o.alive and o.team ~= u.team then foes[#foes + 1] = o end
    end
    if #foes == 0 then return nil end

    local reach = Rules.MoveRange(board, units, u)
    local best, bestScore = nil, -1e9
    for _, node in pairs(reach) do
        if not node.blocked then
            local from = node.tile
            for _, foe in ipairs(foes) do
                local options = { { kind = "attack", range = u.class.range, power = Rules.AttackPower(u) } }
                if u.class.spell then
                    options[#options + 1] = { kind = "spell", range = u.class.spell.range, power = u.class.spell.power }
                end
                for _, opt in ipairs(options) do
                    if Rules.Reaches(opt.range, from, foe.tile) then
                        local dmg = Rules.Damage(u, from, foe, foe.tile, opt.power)
                        local score = dmg * 10 + (dmg >= foe.hp and 200 or 0) - node.cost
                        -- Casters like to keep their distance.
                        if opt.kind == "spell" then score = score + Board.Manhattan(from, foe.tile) * 3 end
                        if score > bestScore then
                            bestScore = score
                            best = { tile = from, action = opt.kind, target = foe }
                        end
                    end
                end
            end
        end
    end

    if not best then
        -- Nobody in reach: close in on the nearest foe by walking distance.
        local sources = {}
        for _, foe in ipairs(foes) do
            for _, n in ipairs(foe.tile.links) do sources[#sources + 1] = n end
        end
        local field = board:DistanceField(sources)
        local bestDist = 1e9
        for k, node in pairs(reach) do
            local d = field[k]
            if not node.blocked and d and (d < bestDist or (d == bestDist and node.cost < (best and best.cost or 99))) then
                bestDist = d
                best = { tile = node.tile, cost = node.cost }
            end
        end
        if not best then best = { tile = u.tile } end
    end
    best.path = Board.PathTo(reach, best.tile)
    return best
end

return Rules
