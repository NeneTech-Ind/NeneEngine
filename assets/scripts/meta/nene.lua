---@meta nene

---@class vec3
---@field x number
---@field y number
---@field z number
---@operator add(vec3): vec3
---@operator sub(vec3): vec3
---@operator mul(number): vec3
---@operator div(number): vec3
---@operator unm: vec3
local vec3 = {}

---@return number
function vec3:length() end

---@return vec3
function vec3:normalized() end

---@param other vec3
---@return number
function vec3:dot(other) end

---@class Entity
local Entity = {}

---@return boolean
function Entity:is_valid() end

---@return string
function Entity:name() end

---Local position, relative to the parent if the entity has one.
---@return vec3
function Entity:get_position() end

---Teleports the entity. Use it for spawning and respawning; move physics bodies with set_velocity.
---@param position vec3
function Entity:set_position(position) end

---Euler angles in degrees.
---@return vec3
function Entity:get_rotation() end

---@param degrees vec3
function Entity:set_rotation(degrees) end

---Linear velocity of a rigidbody, zero for an entity without one.
---@return vec3
function Entity:get_velocity() end

---Sets the linear velocity of a rigidbody. Keep the y component to let gravity act.
---@param velocity vec3
function Entity:set_velocity(velocity) end

---True when the rigidbody stands on another collider.
---@return boolean
function Entity:is_grounded() end

---@alias nene.FieldValue boolean|integer|number|string|vec3

---@class nene.FieldMetadata
---@field type 'bool'|'int'|'float'|'string'|'vec3'|'enum'
---@field min? number
---@field max? number
---@field options? string[]

---@class nene.Behaviour
---@field entity Entity The entity this behaviour is attached to.
---@field fields table<string, nene.FieldValue> Declared fields and their default values. Their current values are copied into the instance, so `self.wave_size` reads the field `wave_size`.
---@field field_metadata? table<string, nene.FieldMetadata> Optional inspector types and constraints.
local Behaviour = {}

---Creates a behaviour class. A script file returns the class it creates.
---@generic T
---@param self T
---@return T
function Behaviour:extend() end

---Called once on the first update in play mode.
function Behaviour:on_start() end

---@param dt number Seconds since the previous frame.
function Behaviour:on_update(dt) end

---Called when the entity is destroyed and when play mode stops.
function Behaviour:on_destroy() end

---Called once when this entity starts touching another collider.
---@param other Entity
---@param normal vec3 Contact normal pointing from this entity towards `other`.
function Behaviour:on_collision(other, normal) end

nene = {}

nene.Behaviour = Behaviour

---@param x number
---@param y number
---@param z number
---@return vec3
function nene.vec3(x, y, z) end

nene.input = {}

---True while any key bound to the action in engine.json is held.
---@param action string
---@return boolean
function nene.input.is_action_down(action) end

---True on the frame the action was pressed.
---@param action string
---@return boolean
function nene.input.is_action_pressed(action) end

---@param key string Key name as written in engine.json, for example "W" or "Space".
---@return boolean
function nene.input.is_key_down(key) end

nene.world = {}

---Creates an entity from assets/prefabs/<prefab>.prefab.json.
---@param prefab string
---@param position vec3
---@return Entity?
function nene.world.spawn(prefab, position) end

---Destroys the entity and its children at the end of the frame.
---@param entity Entity
function nene.world.destroy(entity) end

---Requests that the editor leave Play mode after the current world update.
function nene.world.stop_play() end

---@param name string
---@return Entity?
function nene.world.find(name) end

---Returns all entities with the given category tag in unspecified order.
---@param category string
---@return Entity[] matches Empty when no entity matches.
function nene.world.find_by_tag(category) end

---@param position vec3
---@param radius number
---@return Entity[]
function nene.world.find_in_radius(position, radius) end

nene.log = {}

---@param message string
function nene.log.info(message) end

---@param message string
function nene.log.warn(message) end

---@param message string
function nene.log.error(message) end
