local TileMap = {}

TileMap.TILE_W = 64
TileMap.TILE_H = 32
TileMap.HALF_W = 32
TileMap.HALF_H = 16

TileMap.SPREAD_OFFSETS = {
    { 0,  0},
    { 1,  0}, { 0,  1}, {-1,  0}, { 0, -1},
    { 1,  1}, {-1,  1}, { 1, -1}, {-1, -1},
    { 2,  0}, { 0,  2}, {-2,  0}, { 0, -2},
    { 2,  1}, { 1,  2}, {-1,  2}, {-2,  1},
    {-2, -1}, {-1, -2}, { 1, -2}, { 2, -1},
}

function TileMap.worldToTile(wx, wy)
    local a = wx / TileMap.HALF_W
    local b = wy / TileMap.HALF_H
    return math.floor((a + b) / 2 + 0.5), math.floor((b - a) / 2 + 0.5)
end

function TileMap.tileToWorld(tx, ty)
    return (tx - ty) * TileMap.HALF_W, (tx + ty) * TileMap.HALF_H
end

function TileMap.drawTileOutline(cx, cy, w, h, color, layer)
    DrawLine(cx,     cy - h, cx + w, cy,     color, layer)
    DrawLine(cx + w, cy,     cx,     cy + h, color, layer)
    DrawLine(cx,     cy + h, cx - w, cy,     color, layer)
    DrawLine(cx - w, cy,     cx,     cy - h, color, layer)
end

return TileMap
