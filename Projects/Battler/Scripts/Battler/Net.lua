-- Script-level networking: Lua tables over the engine's NetSend/NetPoll string channel.
-- Two players: the host (NetHost) is the server, the other side joins as its client. Either
-- side's Net.Send reaches the other, so a turn-based game just sends its commands across and
-- both sides apply them with the same rules.
--
-- Values are numbers, strings, booleans and tables of them (no functions, userdata or
-- cycles). Numbers round-trip exactly, which the lockstep relies on.
local Net = {}

-- --- Encoding -----------------------------------------------------------------------------

local function Encode(v, out)
    local t = type(v)
    if t == "number" then
        if math.type and math.type(v) == "integer" then
            out[#out + 1] = "i" .. string.format("%d", v) .. ";"
        else
            out[#out + 1] = "n" .. string.format("%.17g", v) .. ";"
        end
    elseif t == "string" then
        out[#out + 1] = "s" .. #v .. ":" .. v
    elseif t == "boolean" then
        out[#out + 1] = v and "t" or "f"
    elseif t == "table" then
        out[#out + 1] = "{"
        for k, x in pairs(v) do
            Encode(k, out)
            Encode(x, out)
        end
        out[#out + 1] = "}"
    else
        error("Net: can't send a " .. t)
    end
end

local function Decode(s, i)
    local tag = s:sub(i, i)
    if tag == "i" or tag == "n" then
        local stop = s:find(";", i, true)
        local n = tonumber(s:sub(i + 1, stop - 1))
        if tag == "i" and math.tointeger then n = math.tointeger(n) end
        return n, stop + 1
    elseif tag == "s" then
        local colon = s:find(":", i, true)
        local len = tonumber(s:sub(i + 1, colon - 1))
        return s:sub(colon + 1, colon + len), colon + len + 1
    elseif tag == "t" then
        return true, i + 1
    elseif tag == "f" then
        return false, i + 1
    elseif tag == "{" then
        local t = {}
        i = i + 1
        while s:sub(i, i) ~= "}" do
            local k, v
            k, i = Decode(s, i)
            v, i = Decode(s, i)
            t[k] = v
        end
        return t, i + 1
    end
    error("Net: bad message at byte " .. i)
end

function Net.Encode(v)
    local out = {}
    Encode(v, out)
    return table.concat(out)
end

function Net.Decode(s)
    local ok, v = pcall(Decode, s, 1)
    if ok then return v end
    Log("Net: dropped a malformed message: " .. tostring(v))
    return nil
end

-- --- Session ------------------------------------------------------------------------------

Net.PORT = 7777

function Net.Host(port) return NetHost(port or Net.PORT) end
function Net.Join(address, port) return NetJoin(address, port or Net.PORT) end
function Net.Stop() NetStop() end
function Net.IsHost() return NetMode() == "server" end
function Net.Active() return NetMode() ~= "none" end
function Net.Send(msg) NetSend(Net.Encode(msg)) end

-- The network's news since the last call, in order: { type = "connected" | "disconnected" |
-- "stopped" } or { type = "message", msg = <decoded table> }.
function Net.Poll()
    local events = {}
    for _, e in ipairs(NetPoll()) do
        if e.type == "message" then
            local msg = Net.Decode(e.data)
            if msg then events[#events + 1] = { type = "message", msg = msg } end
        else
            events[#events + 1] = e
        end
    end
    return events
end

return Net
