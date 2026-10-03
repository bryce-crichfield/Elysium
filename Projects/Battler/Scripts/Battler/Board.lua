-- The battle board: a 12x12 iso tile grid laid over the navmesh. The scene only places floor
-- models; which tiles exist, how high each stands and which neighbours you can step between
-- all come from the NavMeshSystem (NavFloorHeight / NavCanWalk), so the board can't drift from
-- what the world actually is. Tile (i, j) is centred at ((i - j) * 64, (i + j) * 32 + OY) on
-- the ground; it must match Battle.xml's layout.
local Board = {}
Board.__index = Board

Board.N = 12
Board.HALF_W = 64
Board.HALF_H = 32
Board.OY = -(Board.N - 1) * 32
Board.PITCH_COS = 0.8660254   -- a height z is drawn z * cos(pitch) higher on screen

local DIRS = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} }

local function Key(i, j) return i * 1000 + j end
Board.Key = Key

function Board.Center(i, j)
    return (i - j) * Board.HALF_W, (i + j) * Board.HALF_H + Board.OY
end

-- The tile under a ground point (rounded to the nearest centre).
function Board.ToTile(x, y)
    local u = x / Board.HALF_W
    local v = (y - Board.OY) / Board.HALF_H
    return math.floor((u + v) / 2 + 0.5), math.floor((v - u) / 2 + 0.5)
end

-- Where a point on the ground at height z appears in the (flat, ground-layer) picture.
function Board.Lift(x, y, z) return x, y - z * Board.PITCH_COS end

function Board.new()
    return setmetatable({ tiles = {}, count = 0, linkCount = 0 }, Board)
end

-- Samples the navmesh. Returns the number of tiles found; call again until it settles (the
-- navmesh rebakes as floor models finish loading).
function Board:Build()
    self.tiles, self.count, self.linkCount = {}, 0, 0
    for i = 0, Board.N - 1 do
        for j = 0, Board.N - 1 do
            local x, y = Board.Center(i, j)
            local z = NavFloorHeight(x, y, 0)
            if z then
                self.tiles[Key(i, j)] = { i = i, j = j, x = x, y = y, z = z, links = {} }
                self.count = self.count + 1
            end
        end
    end
    for _, t in pairs(self.tiles) do
        for _, d in ipairs(DIRS) do
            local n = self:Get(t.i + d[1], t.j + d[2])
            if n and NavCanWalk(t.x, t.y, n.x, n.y, t.z) then
                t.links[#t.links + 1] = n
                self.linkCount = self.linkCount + 1
            end
        end
    end
    return self.count
end

function Board:Get(i, j) return self.tiles[Key(i, j)] end

-- The tile drawn under a picture point. Every tile is tested as the block it's drawn as: its
-- diamond lifted to its height, plus the side faces down to the ground. Of the blocks under the
-- point, the one nearest the camera wins (further down the screen, then higher). This uses the
-- board's own heights, so it doesn't miss where the navmesh is eroded at edges and pillars.
function Board:Pick(wx, wy)
    local best, bestDepth = nil, -1e9
    for _, t in pairs(self.tiles) do
        local cx, cy = Board.Lift(t.x, t.y, t.z)
        local dx = math.abs(wx - cx)
        if dx <= Board.HALF_W then
            local half = Board.HALF_H * (1 - dx / Board.HALF_W)
            local dy = wy - cy
            local side = math.max(0, t.z) * Board.PITCH_COS
            if dy >= -half and dy <= half + side then
                local depth = (t.i + t.j) * 1000 + t.z
                if depth > bestDepth then best, bestDepth = t, depth end
            end
        end
    end
    return best
end

-- Tile distance ignoring walls.
function Board.Manhattan(a, b) return math.abs(a.i - b.i) + math.abs(a.j - b.j) end

-- Breadth-first reach from `from` within `steps`. `canPass(tile)` says whether a tile can be
-- walked through (allies yes, enemies no). Returns {key -> {tile, cost, parent}}.
function Board:Reach(from, steps, canPass)
    local reach = { [Key(from.i, from.j)] = { tile = from, cost = 0 } }
    local frontier = { from }
    for cost = 1, steps do
        local nextFrontier = {}
        for _, t in ipairs(frontier) do
            for _, n in ipairs(t.links) do
                local k = Key(n.i, n.j)
                if not reach[k] and (not canPass or canPass(n)) then
                    reach[k] = { tile = n, cost = cost, parent = t }
                    nextFrontier[#nextFrontier + 1] = n
                end
            end
        end
        frontier = nextFrontier
        if #frontier == 0 then break end
    end
    return reach
end

-- The tiles from the reach's origin to `to`, origin excluded.
function Board.PathTo(reach, to)
    local path = {}
    local node = reach[Key(to.i, to.j)]
    while node and node.parent do
        table.insert(path, 1, node.tile)
        node = reach[Key(node.parent.i, node.parent.j)]
    end
    return path
end

-- Steps from every tile to the nearest of `sources` (walking the links, through anything).
function Board:DistanceField(sources)
    local dist, frontier = {}, {}
    for _, s in ipairs(sources) do
        dist[Key(s.i, s.j)] = 0
        frontier[#frontier + 1] = s
    end
    local d = 0
    while #frontier > 0 do
        d = d + 1
        local nextFrontier = {}
        for _, t in ipairs(frontier) do
            for _, n in ipairs(t.links) do
                local k = Key(n.i, n.j)
                if not dist[k] then
                    dist[k] = d
                    nextFrontier[#nextFrontier + 1] = n
                end
            end
        end
        frontier = nextFrontier
    end
    return dist
end

-- A tile's diamond, lifted to its height, shrunk by `inset` pixels.
function Board.Diamond(t, inset)
    inset = inset or 4
    local x, y = Board.Lift(t.x, t.y, t.z)
    local hw, hh = Board.HALF_W - inset, Board.HALF_H - inset * 0.5
    return { {x = x, y = y - hh}, {x = x + hw, y = y}, {x = x, y = y + hh}, {x = x - hw, y = y} }
end

function Board.Fill(t, color, inset)
    DrawPolygon(Board.Diamond(t, inset), color, "overlay")
end

function Board.Outline(t, color, inset)
    local d = Board.Diamond(t, inset)
    for k = 1, 4 do
        local a, b = d[k], d[k % 4 + 1]
        DrawLine(a.x, a.y, b.x, b.y, color, "overlay")
    end
end

return Board
