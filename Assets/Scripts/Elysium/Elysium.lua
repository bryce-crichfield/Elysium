---@meta Elysium

-- ============================================================================
-- Elysium Scripting API
-- Generated from ScriptAPI.h — do not edit manually
-- ============================================================================

-- Types ======================================================================

---@alias Entity integer

---@class Vector2
---@field x number
---@field y number
---@overload fun(): Vector2
---@overload fun(x: number, y: number): Vector2
Vector2 = {}

---@class Color
---@field r number
---@field g number
---@field b number
---@field a number
---@overload fun(): Color
---@overload fun(r: number, g: number, b: number, a: number): Color
Color = {}

---@class Rectangle
---@field x number
---@field y number
---@field width number
---@field height number
---@overload fun(): Rectangle
Rectangle = {}

---@class EventData
---@field type "KeyPressed"|"KeyReleased"|"MouseButtonPressed"|"MouseButtonReleased"|"MouseMoved"
---@field key? integer
---@field button? integer
---@field x? number
---@field y? number
---@field dx? number
---@field dy? number
---@field wx? number  World-space x (mouse events only)
---@field wy? number  World-space y (mouse events only)

-- Entity Lifecycle ============================================================

--- Create a new empty entity in the active world.
---@return Entity
function CreateEntity() end

--- Destroy an entity and all its components.
---@param entity Entity
function DestroyEntity(entity) end

--- Deep-copy an entity and all its components.
---@param entity Entity
---@return Entity clone
function CloneEntity(entity) end

--- Return all living entities in the active world.
---@return Entity[]
function GetEntities() end

--- Find an entity by its name. Returns 0 if not found.
---@param name string
---@return Entity
function GetEntityByName(name) end

-- Component Access ============================================================

--- Get a component's data table from an entity.
---@param entity Entity
---@param componentName string
---@return table|nil
function GetComponent(entity, componentName) end

--- Overwrite a component's data on an entity.
---@param entity Entity
---@param componentName string
---@param value table
function SetComponent(entity, componentName, value) end

--- Add a default-constructed component to an entity.
---@param entity Entity
---@param componentName string
function AddComponent(entity, componentName) end

--- Check whether an entity has a component.
---@param entity Entity
---@param componentName string
---@return boolean
function HasComponent(entity, componentName) end

--- Remove a component from an entity.
---@param entity Entity
---@param componentName string
function RemoveComponent(entity, componentName) end

-- Queries =====================================================================

--- Return all entities that have the given component.
---@param componentName string
---@return Entity[]
function FindEntitiesWithComponent(componentName) end

--- Find the nearest entity (with TransformComponent) that has the given component.
---@param x number
---@param y number
---@param componentName string
---@return Entity  Returns 0 if none found
function FindNearestEntity(x, y, componentName) end

--- Euclidean distance between two points.
---@param x1 number
---@param y1 number
---@param x2 number
---@param y2 number
---@return number
function Distance(x1, y1, x2, y2) end

-- Collision ===================================================================

--- Check whether two entities are currently colliding.
---@param a Entity
---@param b Entity
---@return boolean
function AreColliding(a, b) end

--- Get all entities currently colliding with the given entity.
---@param entity Entity
---@return Entity[]
function GetCollisions(entity) end

-- Input =======================================================================

---@param key integer
---@return boolean
function IsKeyDown(key) end

---@param key integer
---@return boolean
function IsKeyPressed(key) end

---@param button integer
---@return boolean
function IsMouseButtonDown(button) end

---@param button integer
---@return boolean
function IsMouseButtonPressed(button) end

---@param button integer
---@return boolean
function IsMouseButtonReleased(button) end

--- Returns current mouse position in world coordinates.
---@return Vector2
function GetMousePosition() end

--- The game screen's size, in the pixels Screen2D ("ui") layers, GetMousePosition and
--- ViewProject use. It's the project's configured screen size (Config/ApplicationConfig.xml
--- <Screen>), extended along whichever axis the window has room to spare: a wider window gives
--- a wider screen at the same height. It changes when the window does, so lay out against it
--- each frame rather than once.
---@return number width
---@return number height
function GetScreenSize() end

--- The size Screen2D layouts are authored for: the project's configured screen size
--- (Config/ApplicationConfig.xml <Screen>). GetScreenSize is never smaller; the difference is
--- how far a widget anchored to the right or bottom edge moves from where it was placed.
---@return number width
---@return number height
function GetLayoutSize() end

--- Transform world coordinates to screen coordinates. 
--- @requires "CAMERA" 
--- @param worldPos Vector2
--- @return Vector2
function WorldToScreen(worldPos) end

--- Transform screen coordinates to world coordinates.
--- @requires "CAMERA"
--- @param screenPos Vector2
--- @return Vector2
function ScreenToWorld(screenPos) end

-- Scene =======================================================================

--- Replace the current scene.
---@param sceneName string
function SceneReplace(sceneName) end

---@class SceneLoading
---@field scene string     the scene being opened
---@field asset string     a preload still loading ("" before they start)
---@field loaded integer   preloads in
---@field total integer
---@field progress number  loaded / total, 0 to 1

-- The scene change the loading scene (Project.xml <LoadingScene>) is up for, or nil when
-- nothing is loading. It waits on the scene's <Preload> list and its script's Preload().
---@return SceneLoading?
function SceneLoading() end

-- Network =====================================================================

---@class NetEvent
---@field type "connected"|"disconnected"|"stopped"|"message"
---@field data? string  The payload, for "message"

--- Start hosting on `port` (default 7777). Restarts the network if it was running.
---@param port? integer
---@return boolean
function NetHost(port) end

--- Connect to a host. Restarts the network if it was running.
---@param address string
---@param port? integer
---@return boolean
function NetJoin(address, port) end

function NetStop() end

---@return "none"|"server"|"client"
function NetMode() end

---@return integer
function NetPeers() end

--- Send a string reliably: a client to the server, the server to every client.
---@param data string
function NetSend(data) end

--- Everything the network did since the last call, in order. The queue is shared by every script.
---@return NetEvent[]
function NetPoll() end

-- Graphics ====================================================================

--- Turn antialiasing (MSAA on World3D models) on or off for every scene.
---@param enabled boolean
function SetMsaaEnabled(enabled) end

---@return boolean
function IsMsaaEnabled() end

-- Audio =======================================================================

--- Play a sound asset (project-relative, e.g. "Sounds/Hit.wav"; .wav .mp3 .ogg .flac),
--- loading it first if needed, on a mixer channel. Returns an id for StopSound.
---@param asset string
---@param volume? number 0..1, default 1
---@param loop? boolean default false
---@param channel? integer a CHANNEL_* constant, default CHANNEL_MASTER
---@return integer
function PlaySound(asset, volume, loop, channel) end

--- Stop a playing sound early (a loop). Ids that already finished are ignored.
---@param id integer
function StopSound(id) end

--- Stop every sound.
function StopAllSounds() end

--- Whether a sound is still playing: false once it finished or was stopped (switching between
--- Play and the editor stops every sound).
---@param id integer
---@return boolean
function IsSoundPlaying(id) end

-- Mixer channels. Every channel mixes into Master, whose volume scales them all.
CHANNEL_MASTER = 0
CHANNEL_MUSIC = 1
CHANNEL_EFFECTS = 2
CHANNEL_AMBIENT = 3
CHANNEL_DIALOGUE = 4

--- Set a channel's volume, 0..1.
---@param channel integer
---@param volume number
function SetChannelVolume(channel, volume) end

---@param channel integer
---@return number
function GetChannelVolume(channel) end

--- Close the game after this frame.
function Quit() end

-- Utility =====================================================================

--- Print a message to the engine log.
---@param message string
function Log(message) end

--- Random integer in [min, max] inclusive.
---@param min integer
---@param max integer
---@return integer
function Random(min, max) end

-- Key Constants ===============================================================

---@type integer
KEY_SPACE = 32
---@type integer
KEY_ENTER = 257
---@type integer
KEY_TAB = 258
---@type integer
KEY_ESCAPE = 256
---@type integer
KEY_BACKSPACE = 259
---@type integer
KEY_LEFT = 263
---@type integer
KEY_RIGHT = 262
---@type integer
KEY_UP = 265
---@type integer
KEY_DOWN = 264

---@type integer
KEY_A = 65
---@type integer
KEY_B = 66
---@type integer
KEY_C = 67
---@type integer
KEY_D = 68
---@type integer
KEY_E = 69
---@type integer
KEY_F = 70
---@type integer
KEY_G = 71
---@type integer
KEY_H = 72
---@type integer
KEY_I = 73
---@type integer
KEY_J = 74
---@type integer
KEY_K = 75
---@type integer
KEY_L = 76
---@type integer
KEY_M = 77
---@type integer
KEY_N = 78
---@type integer
KEY_O = 79
---@type integer
KEY_P = 80
---@type integer
KEY_Q = 81
---@type integer
KEY_R = 82
---@type integer
KEY_S = 83
---@type integer
KEY_T = 84
---@type integer
KEY_U = 85
---@type integer
KEY_V = 86
---@type integer
KEY_W = 87
---@type integer
KEY_X = 88
---@type integer
KEY_Y = 89
---@type integer
KEY_Z = 90

---@type integer
KEY_0 = 48
---@type integer
KEY_1 = 49
---@type integer
KEY_2 = 50
---@type integer
KEY_3 = 51
---@type integer
KEY_4 = 52
---@type integer
KEY_5 = 53
---@type integer
KEY_6 = 54
---@type integer
KEY_7 = 55
---@type integer
KEY_8 = 56
---@type integer
KEY_9 = 57

-- Mouse Constants =============================================================

---@type integer
MOUSE_LEFT = 0
---@type integer
MOUSE_RIGHT = 1
---@type integer
MOUSE_MIDDLE = 2

-- Script Lifecycle (implement these in your script tables) ====================

---@class EntityScript
---@field Initialize? fun(self: EntityScript, entity: Entity)
---@field Update? fun(self: EntityScript, entity: Entity, dt: number)
---@field OnEvent? fun(self: EntityScript, entity: Entity, event: EventData)

---@class SceneScript
---@field Preload? fun(self: SceneScript): string[]  more assets to load before the scene opens (files or folders, like <Preload>); runs before the scene exists
---@field Initialize? fun(self: SceneScript)
---@field Update? fun(self: SceneScript, dt: number)
---@field OnEvent? fun(self: SceneScript, event: EventData)