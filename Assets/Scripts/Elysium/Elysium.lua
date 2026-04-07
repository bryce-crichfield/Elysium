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

--- Find the nearest entity (with PositionComponent) that has the given component.
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

--- Returns current mouse position in world coordinates.
---@return Vector2
function GetMousePosition() end

-- Scene =======================================================================

--- Replace the current scene.
---@param sceneName string
function SceneReplace(sceneName) end

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
---@field Initialize? fun(self: SceneScript)
---@field Update? fun(self: SceneScript, dt: number)
---@field OnEvent? fun(self: SceneScript, event: EventData)