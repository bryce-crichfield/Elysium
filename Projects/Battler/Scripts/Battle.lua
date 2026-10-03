---@type SceneScript
-- Battler POC: a turn-based skirmish on the iso board. Player phase, then enemy phase.
--   Left-click a blue-ringed unit, then a blue tile to move (its own tile to stay put).
--   Then 1 Attack / 2 Spell / 3 Wait (or click the menu). Right-click / Esc backs out.
--   Space ends the player phase. WASD / middle-drag pans, wheel zooms. R restarts.
local Board = require("Scripts/Battler/Board")
local Units = require("Scripts/Battler/Units")
local Rules = require("Scripts/Battler/Rules")

local Battle = {}

local PLAYER, ENEMY = Units.PLAYER, Units.ENEMY
local SCREEN_W, SCREEN_H = 1280, 720
local STEP_TIME = 0.42

local ROSTER = {
    { "Fighter", PLAYER, 1, 2 }, { "Archer", PLAYER, 2, 1 }, { "Mage", PLAYER, 1, 1 },
    { "Grunt", ENEMY, 9, 9 }, { "Poacher", ENEMY, 10, 7 }, { "Grunt", ENEMY, 7, 10 }, { "Hexer", ENEMY, 10, 10 },
}

local COLORS = {
    move = {r = 60, g = 140, b = 255, a = 70}, moveEdge = {r = 120, g = 190, b = 255, a = 200},
    attack = {r = 255, g = 60, b = 50, a = 55}, attackEdge = {r = 255, g = 110, b = 90, a = 190},
    spell = {r = 170, g = 90, b = 255, a = 60}, spellEdge = {r = 200, g = 150, b = 255, a = 200},
    splash = {r = 255, g = 170, b = 60, a = 90},
    hover = {r = 255, g = 255, b = 255, a = 210}, path = {r = 255, g = 255, b = 255, a = 160},
    text = {r = 235, g = 235, b = 240, a = 255}, dim = {r = 160, g = 160, b = 175, a = 255},
    panel = {r = 14, g = 14, b = 22, a = 210}, gold = {r = 255, g = 210, b = 110, a = 255},
}

-- --- Coroutine helpers -------------------------------------------------------------------

local function Wait(seconds)
    local t = 0
    while t < seconds do t = t + coroutine.yield() end
end

local function Lerp(a, b, t) return a + (b - a) * t end
local function Smooth(t) return t * t * (3 - 2 * t) end

-- --- Lifecycle ---------------------------------------------------------------------------

function Battle:Initialize()
    self.time = 0
    self.board = Board.new()
    self.state = "loading"
    self.loadTimer, self.loadChecks, self.lastCount = 0, 0, -1
    self.units = {}
    self.floaters = {}
    self.banner = nil
    self.turn = 0
    self.zoomTarget = nil
    self.panAnchor = nil
    Log("Battler: building the board from the navmesh...")
end

function Battle:Camera()
    if not self.camera then self.camera = GetEntityByName("CAMERA") end
    return self.camera
end

function Battle:StartBattle()
    for _, u in ipairs(self.units) do DestroyEntity(u.entity) end
    self.units = {}
    for _, r in ipairs(ROSTER) do
        local tile = self:NearestTile(r[3], r[4])
        if tile then
            local u = Units.Spawn(r[1], r[2], tile)
            if u then self.units[#self.units + 1] = u end
        end
    end
    self.turn = 0
    self.selected, self.mode = nil, nil
    self:Run(function() self:PlayerPhase() end)
end

-- A free tile nearest (i, j).
function Battle:NearestTile(i, j)
    local best, bestD = nil, 1e9
    for _, t in pairs(self.board.tiles) do
        local d = math.abs(t.i - i) + math.abs(t.j - j)
        if d < bestD and not Rules.UnitAt(self.units, t) then best, bestD = t, d end
    end
    return best
end

-- Runs `fn` as the current sequence; input waits until it's done.
function Battle:Run(fn)
    self.co = coroutine.create(fn)
end

function Battle:Update(dt)
    self.time = self.time + dt
    self:UpdateCamera(dt)

    if self.state == "loading" then
        self.loadTimer = self.loadTimer + dt
        if self.loadTimer >= 0.25 then
            self.loadTimer = 0
            local count = self.board:Build()
            if count > 0 and count == self.lastCount then
                self.loadChecks = self.loadChecks + 1
            else
                self.loadChecks = 0
            end
            self.lastCount = count
            if self.loadChecks >= 3 then
                Log(string.format("Battler: board has %d tiles, %d links", count, self.board.linkCount))
                self.state = "battle"
                self:StartBattle()
            end
        end
        return
    end

    if self.co then
        local ok, err = coroutine.resume(self.co, dt)
        if not ok then
            Log("Battler error: " .. tostring(err))
            self.co = nil
        elseif coroutine.status(self.co) == "dead" then
            self.co = nil
        end
    end

    for _, u in ipairs(self.units) do Units.Update(u, dt) end
    for i = #self.floaters, 1, -1 do
        local f = self.floaters[i]
        f.t = f.t + dt
        if f.t > 1.1 then table.remove(self.floaters, i) end
    end
    if self.banner then
        self.banner.t = self.banner.t + dt
        if self.banner.duration and self.banner.t > self.banner.duration then self.banner = nil end
    end

    local mouse = GetMousePosition()
    local w = ScreenToWorld(mouse)
    self.hover = self:PickTile(w.x, w.y)
    self.mouse = mouse
    self:PollInput(mouse, w)
end

-- Input is polled rather than taken from OnEvent: the same handler, fed from the input state.
function Battle:PollInput(mouse, w)
    local function Feed(event)
        event.x, event.y, event.wx, event.wy = mouse.x, mouse.y, w.x, w.y
        local ok, err = pcall(self.HandleEvent, self, event)
        if not ok then Log("Battler input error: " .. tostring(err)) end
    end
    for _, button in ipairs({ MOUSE_LEFT, MOUSE_RIGHT }) do
        if IsMouseButtonPressed(button) then Feed({ type = "MouseButtonPressed", button = button }) end
    end
    for _, key in ipairs({ KEY_R, KEY_ESCAPE, KEY_SPACE, KEY_TAB, KEY_1, KEY_2, KEY_3 }) do
        if IsKeyPressed(key) then Feed({ type = "KeyPressed", key = key }) end
    end
end

function Battle:UpdateCamera(dt)
    local cam = self:Camera()
    if not cam then return end
    local t = GetComponent(cam, "Transform")
    local c = GetComponent(cam, "Camera")
    if not t or not c then return end
    local speed = 500 / c.zoom
    if IsKeyDown(KEY_W) or IsKeyDown(KEY_UP) then t.localY = t.localY - speed * dt end
    if IsKeyDown(KEY_S) or IsKeyDown(KEY_DOWN) then t.localY = t.localY + speed * dt end
    if IsKeyDown(KEY_A) or IsKeyDown(KEY_LEFT) then t.localX = t.localX - speed * dt end
    if IsKeyDown(KEY_D) or IsKeyDown(KEY_RIGHT) then t.localX = t.localX + speed * dt end

    self.zoomTarget = self.zoomTarget or c.zoom
    local wheel = GetMouseWheelMove()
    if wheel ~= 0 then self.zoomTarget = math.max(0.5, math.min(2.2, self.zoomTarget * 1.15 ^ wheel)) end
    c.zoom = c.zoom + (self.zoomTarget - c.zoom) * (1 - math.exp(-12 * dt))

    local m = GetMousePosition()
    if IsMouseButtonDown(MOUSE_MIDDLE) then
        if self.panAnchor then
            t.localX = t.localX - (m.x - self.panAnchor.x) / c.zoom
            t.localY = t.localY - (m.y - self.panAnchor.y) / c.zoom
        end
        self.panAnchor = { x = m.x, y = m.y }
    else
        self.panAnchor = nil
    end
end

-- --- Phases -------------------------------------------------------------------------------

function Battle:Living(team)
    local list = {}
    for _, u in ipairs(self.units) do
        if u.alive and (team == nil or u.team == team) then list[#list + 1] = u end
    end
    return list
end

function Battle:ShowBanner(text, sub, duration)
    self.banner = { text = text, sub = sub, t = 0, duration = duration }
end

-- True (and shows the result) when one side is gone.
function Battle:CheckOver()
    if #self:Living(ENEMY) == 0 then
        self.state = "over"
        self:ShowBanner("VICTORY", "Press R to fight again")
        return true
    elseif #self:Living(PLAYER) == 0 then
        self.state = "over"
        self:ShowBanner("DEFEAT", "Press R to try again")
        return true
    end
    return false
end

function Battle:PlayerPhase()
    self.turn = self.turn + 1
    self.phase = PLAYER
    for _, u in ipairs(self.units) do u.moved, u.acted = false, false end
    self:ShowBanner("PLAYER PHASE", "Turn " .. self.turn, 1.2)
    Wait(0.9)
    -- The rest happens through OnEvent until every unit has acted.
end

function Battle:EndPlayerPhase()
    self.selected, self.mode = nil, nil
    self:Run(function() self:EnemyPhase() end)
end

function Battle:EnemyPhase()
    self.phase = ENEMY
    for _, u in ipairs(self.units) do u.moved, u.acted = false, false end
    self:ShowBanner("ENEMY PHASE", nil, 1.2)
    Wait(1.0)
    for _, u in ipairs(self:Living(ENEMY)) do
        if u.alive then
            local plan = Rules.Plan(self.board, self.units, u)
            if plan then
                self.focus = u
                Wait(0.25)
                if #plan.path > 0 then self:MoveAlong(u, plan.path) end
                if plan.action == "attack" and plan.target.alive then
                    self:Attack(u, plan.target)
                elseif plan.action == "spell" and plan.target.alive then
                    self:CastBolt(u, plan.target.tile)
                end
                u.acted = true
                Wait(0.2)
                if self:CheckOver() then self.focus = nil return end
            end
        end
    end
    self.focus = nil
    self:PlayerPhase()
end

function Battle:AllPlayersDone()
    for _, u in ipairs(self:Living(PLAYER)) do
        if not u.acted then return false end
    end
    return true
end

-- --- Actions ------------------------------------------------------------------------------

function Battle:MoveAlong(u, path)
    Units.Play(u, "Walk")
    for _, tile in ipairs(path) do
        local x0, y0, z0 = u.x, u.y, u.z
        Units.Face(u, tile.x, tile.y)
        local t = 0
        while t < STEP_TIME do
            t = t + coroutine.yield()
            local k = math.min(1, t / STEP_TIME)
            u.x, u.y = Lerp(x0, tile.x, k), Lerp(y0, tile.y, k)
            -- A little hop when the floor changes height.
            local hop = math.abs(tile.z - z0) > 1 and math.sin(k * math.pi) * 10 or 0
            u.z = Lerp(z0, tile.z, Smooth(k)) + hop
        end
        u.x, u.y, u.z = tile.x, tile.y, tile.z
        u.tile = tile
    end
    Units.Play(u, "Idle")
    u.moved = true
end

function Battle:Float(text, x, y, z, color)
    self.floaters[#self.floaters + 1] = { text = text, x = x, y = y, z = z, t = 0, color = color }
end

function Battle:Hurt(target, dmg)
    target.hp = math.max(0, target.hp - dmg)
    target.shake = 0.25
    self:Float(tostring(dmg), target.x, target.y, target.z, {r = 255, g = 225, b = 120, a = 255})
    if target.hp <= 0 and target.alive then
        target.alive = false
        Units.Play(target, "Death")
        self:Float("DEFEATED", target.x, target.y, target.z + 20, {r = 255, g = 90, b = 80, a = 255})
    elseif target.alive then
        Units.Play(target, "Hurt")
    end
end

function Battle:Attack(att, def)
    Units.Face(att, def.x, def.y)
    Units.Play(att, "Attack")
    local length = Units.ClipLength("Attack", att)
    Wait(length * 0.55)
    self:Hurt(def, Rules.Damage(att, att.tile, def, def.tile, Rules.AttackPower(att)))
    Wait(length * 0.45 + 0.15)
end

function Battle:CastBolt(caster, tile)
    local spell = caster.class.spell
    Units.Face(caster, tile.x, tile.y)
    Units.Play(caster, spell.cast or "Attack")
    self:Float(spell.name .. "!", caster.x, caster.y, caster.z + 20, {r = 170, g = 210, b = 255, a = 255})
    Wait(0.35)

    local sx, sy, sz = caster.x, caster.y, caster.z + 55
    local ex, ey, ez = tile.x, tile.y, tile.z + 30
    local bolt = SpawnPrefab("Prefabs/Bolt.xml", sx, sy, sz)
    local flight = 0.35 + Board.Manhattan(caster.tile, tile) * 0.08
    local t = 0
    while t < flight do
        t = t + coroutine.yield()
        local k = math.min(1, t / flight)
        local tr = bolt and GetComponent(bolt, "Transform")
        if tr then
            tr.localX, tr.localY = Lerp(sx, ex, k), Lerp(sy, ey, k)
            tr.localZ = Lerp(sz, ez, k) + math.sin(k * math.pi) * 70
        end
    end
    if bolt then DestroyEntity(bolt) end

    for _, v in ipairs(Rules.Splash(self.units, tile, spell.splash)) do
        if v.team ~= caster.team then
            local power = v.tile == tile and spell.power or math.floor(spell.power / 2)
            self:Hurt(v, Rules.Damage(caster, caster.tile, v, v.tile, power))
        end
    end
    Wait(0.6)
end

-- Runs a player unit's action, then hands control back (and ends the phase if all are done).
function Battle:PlayerAction(fn)
    local u = self.selected
    self.mode = nil
    self:Run(function()
        fn()
        u.acted, u.moved = true, true
        self.selected = nil
        if self:CheckOver() then return end
        if self:AllPlayersDone() then
            Wait(0.3)
            self:EnemyPhase()
        end
    end)
end

-- --- Input --------------------------------------------------------------------------------

function Battle:Busy() return self.co ~= nil or self.state ~= "battle" or self.phase ~= PLAYER end

function Battle:Select(u)
    self.selected = u
    self.origin = u.tile
    self.mode = "move"
    self.reach = Rules.MoveRange(self.board, self.units, u)
end

function Battle:MenuItems()
    local u = self.selected
    if not u then return {} end
    local items = { { key = "1", label = "Attack", mode = "attack" } }
    if u.class.spell then items[#items + 1] = { key = "2", label = u.class.spell.name, mode = "spell" } end
    items[#items + 1] = { key = "3", label = "Wait", mode = "wait" }
    for k, item in ipairs(items) do
        item.x, item.y, item.w, item.h = SCREEN_W - 230, SCREEN_H - 60 - (#items - k) * 42, 200, 36
    end
    return items
end

function Battle:Choose(mode)
    local u = self.selected
    if mode == "wait" then
        self:PlayerAction(function() end)
    elseif mode == "spell" and not u.class.spell then
        return
    else
        self.mode = mode
    end
end

function Battle:Back()
    local u = self.selected
    if not u then return end
    if self.mode == "attack" or self.mode == "spell" then
        self.mode = "menu"
    elseif self.mode == "menu" then
        -- Undo the move.
        u.tile, u.x, u.y, u.z, u.moved = self.origin, self.origin.x, self.origin.y, self.origin.z, false
        self:Select(u)
    else
        self.selected, self.mode = nil, nil
    end
end

function Battle:SpellTiles(u)
    local list = {}
    for _, t in pairs(self.board.tiles) do
        if Rules.Reaches(u.class.spell.range, u.tile, t) then list[#list + 1] = t end
    end
    return list
end

-- The tile under a picture point. The floor always wins: a unit's sprite stands tall enough to
-- cover the tiles behind it, and those must stay reachable. A unit's body only counts where no
-- floor is drawn at all (its head sticking out past the board's edge).
function Battle:PickTile(wx, wy)
    local floor = self.board:Pick(wx, wy)
    if floor then return floor end
    for _, u in ipairs(self.units) do
        if u.alive then
            local x, y = Board.Lift(u.x, u.y, u.z)
            if math.abs(wx - x) <= 22 and wy <= y and wy >= y - 90 then return u.tile end
        end
    end
    return nil
end

function Battle:Click(tile)
    local u = self.selected
    local there = tile and Rules.UnitAt(self.units, tile)

    if self.mode == nil or self.mode == "move" then
        if there and there.team == PLAYER and not there.acted and there ~= u then
            self:Select(there)
            return
        end
        if not u then return end
        if not tile then return end
        local node = self.reach[Board.Key(tile.i, tile.j)]
        if tile == u.tile then
            self.mode = "menu"
        elseif node and not node.blocked then
            local path = Board.PathTo(self.reach, tile)
            self.mode = nil
            self:Run(function()
                self:MoveAlong(u, path)
                self.mode = "menu"
            end)
        end
    elseif self.mode == "attack" then
        if there and there.team ~= u.team and Rules.Reaches(u.class.range, u.tile, tile) then
            self:PlayerAction(function() self:Attack(u, there) end)
        end
    elseif self.mode == "spell" then
        if tile and Rules.Reaches(u.class.spell.range, u.tile, tile) then
            self:PlayerAction(function() self:CastBolt(u, tile) end)
        end
    end
end

function Battle:HandleEvent(event)
    if event.type == "KeyPressed" then
        if event.key == KEY_R and self.state ~= "loading" then
            self.co = nil
            self.state = "battle"
            self.banner = nil
            self:StartBattle()
            return true
        end
        if self:Busy() then return false end
        if event.key == KEY_ESCAPE then self:Back() return true end
        if event.key == KEY_SPACE then self:EndPlayerPhase() return true end
        if event.key == KEY_TAB then
            for _, u in ipairs(self:Living(PLAYER)) do
                if not u.acted and u ~= self.selected then self:Select(u) break end
            end
            return true
        end
        if self.mode == "menu" then
            for _, item in ipairs(self:MenuItems()) do
                if (item.key == "1" and event.key == KEY_1) or (item.key == "2" and event.key == KEY_2)
                    or (item.key == "3" and event.key == KEY_3) then
                    self:Choose(item.mode)
                    return true
                end
            end
        end
    elseif event.type == "MouseButtonPressed" then
        if self:Busy() then return false end
        if event.button == MOUSE_RIGHT then self:Back() return true end
        if event.button ~= MOUSE_LEFT then return false end
        if self.mode == "menu" or self.mode == "attack" or self.mode == "spell" then
            for _, item in ipairs(self:MenuItems()) do
                if event.x >= item.x and event.x <= item.x + item.w and event.y >= item.y and event.y <= item.y + item.h then
                    self:Choose(item.mode)
                    return true
                end
            end
        end
        if self.mode == "menu" then return true end
        local tile = self:PickTile(event.wx, event.wy)
        self:Click(tile)
        return true
    end
    return false
end

-- --- Drawing ------------------------------------------------------------------------------

function Battle:Render()
    if self.state == "loading" then
        DrawText("Reading the battlefield...", SCREEN_W / 2 - 150, SCREEN_H / 2, 24, COLORS.text, "ui")
        return
    end

    local u = self.selected
    if u and self.mode == "move" and self.reach then
        for _, node in pairs(self.reach) do
            if not node.blocked or node.tile == u.tile then
                Board.Fill(node.tile, COLORS.move)
                Board.Outline(node.tile, COLORS.moveEdge, 5)
            end
        end
        -- The path to the hovered tile.
        local node = self.hover and self.reach[Board.Key(self.hover.i, self.hover.j)]
        if node and not node.blocked then
            local prev = u.tile
            for _, t in ipairs(Board.PathTo(self.reach, self.hover)) do
                local x1, y1 = Board.Lift(prev.x, prev.y, prev.z)
                local x2, y2 = Board.Lift(t.x, t.y, t.z)
                DrawLine(x1, y1, x2, y2, COLORS.path, "overlay")
                prev = t
            end
        end
    elseif u and self.mode == "attack" then
        for _, t in pairs(self.board.tiles) do
            if Rules.Reaches(u.class.range, u.tile, t) then
                Board.Fill(t, COLORS.attack)
                Board.Outline(t, COLORS.attackEdge, 5)
            end
        end
    elseif u and self.mode == "spell" then
        for _, t in ipairs(self:SpellTiles(u)) do
            Board.Fill(t, COLORS.spell)
            Board.Outline(t, COLORS.spellEdge, 5)
        end
        if self.hover and Rules.Reaches(u.class.spell.range, u.tile, self.hover) then
            for _, t in pairs(self.board.tiles) do
                if Board.Manhattan(t, self.hover) <= u.class.spell.splash then Board.Fill(t, COLORS.splash, 8) end
            end
        end
    end

    if self.hover then Board.Outline(self.hover, COLORS.hover, 2) end

    for _, v in ipairs(self.units) do
        Units.DrawRing(v, v == self.selected or v == self.focus, self.time)
        Units.DrawBar(v)
    end

    for _, f in ipairs(self.floaters) do
        local x, y = Board.Lift(f.x, f.y, f.z)
        local a = math.floor(255 * math.max(0, 1 - f.t / 1.1))
        local c = { r = f.color.r, g = f.color.g, b = f.color.b, a = a }
        DrawText(f.text, x - 10, y - 130 - f.t * 40, 22, c, "fx")
    end

    self:DrawHud()
end

function Battle:Forecast()
    local u, h = self.selected, self.hover
    if not u or not h then return nil end
    local target = Rules.UnitAt(self.units, h)
    if self.mode == "attack" and target and target.team ~= u.team and Rules.Reaches(u.class.range, u.tile, h) then
        return string.format("%s -> %s: %d damage", u.name, target.name,
            Rules.Damage(u, u.tile, target, target.tile, Rules.AttackPower(u)))
    elseif self.mode == "spell" and Rules.Reaches(u.class.spell.range, u.tile, h) then
        local total, n = 0, 0
        for _, v in ipairs(Rules.Splash(self.units, h, u.class.spell.splash)) do
            if v.team ~= u.team then
                local power = v.tile == h and u.class.spell.power or math.floor(u.class.spell.power / 2)
                total, n = total + Rules.Damage(u, u.tile, v, v.tile, power), n + 1
            end
        end
        return n > 0 and string.format("%s hits %d for %d total", u.class.spell.name, n, total) or "No targets"
    end
    return nil
end

function Battle:DrawHud()
    -- Top bar.
    FillRect(0, 0, SCREEN_W, 36, COLORS.panel, "ui")
    local phase = self.phase == ENEMY and "Enemy phase" or "Player phase"
    DrawText(string.format("Turn %d  -  %s", math.max(1, self.turn), phase), 16, 8, 20, COLORS.gold, "ui")
    DrawText(string.format("Allies %d   Foes %d", #self:Living(PLAYER), #self:Living(ENEMY)),
        SCREEN_W - 230, 8, 20, COLORS.text, "ui")

    -- The unit under the cursor, else the selected one.
    local shown = (self.hover and Rules.UnitAt(self.units, self.hover)) or self.selected or self.focus
    if shown then
        FillRect(16, SCREEN_H - 120, 300, 104, COLORS.panel, "ui")
        local c = shown.team == PLAYER and {r = 120, g = 190, b = 255, a = 255} or {r = 255, g = 120, b = 100, a = 255}
        DrawText(shown.name, 30, SCREEN_H - 110, 24, c, "ui")
        DrawText(string.format("HP %d / %d", shown.hp, shown.maxHp), 30, SCREEN_H - 80, 20, COLORS.text, "ui")
        local cl = shown.class
        local line = string.format("ATK %d  DEF %d  MOV %d", cl.atk, cl.def, cl.move)
        DrawText(line, 30, SCREEN_H - 56, 18, COLORS.dim, "ui")
        if cl.spell then
            DrawText(string.format("%s: %d power, range %d-%d", cl.spell.name, cl.spell.power, cl.spell.range[1],
                cl.spell.range[2]), 30, SCREEN_H - 34, 16, COLORS.dim, "ui")
        end
    end

    -- Action menu.
    if self.selected and (self.mode == "menu" or self.mode == "attack" or self.mode == "spell") then
        for _, item in ipairs(self:MenuItems()) do
            local active = item.mode == self.mode
            local over = self.mouse and self.mouse.x >= item.x and self.mouse.x <= item.x + item.w
                and self.mouse.y >= item.y and self.mouse.y <= item.y + item.h
            local bg = active and {r = 70, g = 60, b = 30, a = 230} or (over and {r = 40, g = 40, b = 60, a = 230} or COLORS.panel)
            FillRect(item.x, item.y, item.w, item.h, bg, "ui")
            DrawText(item.key .. "  " .. item.label, item.x + 14, item.y + 8, 20, active and COLORS.gold or COLORS.text, "ui")
        end
    end

    local forecast = self:Forecast()
    if forecast then
        FillRect(SCREEN_W / 2 - 180, SCREEN_H - 56, 360, 36, COLORS.panel, "ui")
        DrawText(forecast, SCREEN_W / 2 - 165, SCREEN_H - 48, 20, COLORS.gold, "ui")
    end

    -- Hints.
    local hint
    if self.phase == PLAYER and not self.co and self.state == "battle" then
        if not self.selected then hint = "Click a unit (Tab cycles)   Space: end phase"
        elseif self.mode == "move" then hint = "Click a blue tile to move, the unit's own tile to stay   Right-click: cancel"
        elseif self.mode == "menu" then hint = "Choose an action   Right-click: undo move"
        else hint = "Click a target   Right-click: back" end
    end
    if hint then DrawText(hint, 16, 44, 16, COLORS.dim, "ui") end

    if self.banner then
        local b = self.banner
        local a = b.duration and math.floor(255 * math.min(1, (b.duration - b.t) * 3, b.t * 4)) or 255
        a = math.max(0, math.min(255, a))
        FillRect(0, SCREEN_H / 2 - 50, SCREEN_W, 100, {r = 8, g = 8, b = 14, a = math.floor(a * 0.8)}, "ui")
        DrawText(b.text, SCREEN_W / 2 - #b.text * 13, SCREEN_H / 2 - 36, 44, {r = 255, g = 215, b = 120, a = a}, "ui")
        if b.sub then
            DrawText(b.sub, SCREEN_W / 2 - #b.sub * 5, SCREEN_H / 2 + 14, 20, {r = 220, g = 220, b = 230, a = a}, "ui")
        end
    end
end

return Battle
