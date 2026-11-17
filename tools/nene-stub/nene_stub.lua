local stub = {}

local Vec3 = {}
Vec3.__index = Vec3

local function vec3(x, y, z)
	return setmetatable({ x = x or 0, y = y or 0, z = z or 0 }, Vec3)
end

local function is_vec3(value)
	return getmetatable(value) == Vec3
end

local function copy(value)
	return vec3(value.x, value.y, value.z)
end

Vec3.__add = function(a, b)
	return vec3(a.x + b.x, a.y + b.y, a.z + b.z)
end

Vec3.__sub = function(a, b)
	return vec3(a.x - b.x, a.y - b.y, a.z - b.z)
end

Vec3.__mul = function(a, b)
	if type(a) == "number" then
		a, b = b, a
	end
	return vec3(a.x * b, a.y * b, a.z * b)
end

Vec3.__div = function(a, b)
	return vec3(a.x / b, a.y / b, a.z / b)
end

Vec3.__unm = function(a)
	return vec3(-a.x, -a.y, -a.z)
end

Vec3.__eq = function(a, b)
	return a.x == b.x and a.y == b.y and a.z == b.z
end

Vec3.__tostring = function(a)
	return string.format("(%.2f, %.2f, %.2f)", a.x, a.y, a.z)
end

function Vec3:dot(other)
	return self.x * other.x + self.y * other.y + self.z * other.z
end

function Vec3:length()
	return math.sqrt(self:dot(self))
end

function Vec3:normalized()
	local length = self:length()
	if length == 0 then
		return vec3()
	end
	return self / length
end

local records = {}
local order = {}
local next_id = 1
local pending_start = {}
local pending_destroy = {}
local touching = {}
local classes = {}

local Entity = {}
Entity.__index = Entity

local function handle(id)
	return setmetatable({ id = id }, Entity)
end

Entity.__eq = function(a, b)
	return a.id == b.id
end

Entity.__tostring = function(a)
	local record = records[a.id]
	return string.format("Entity(%d, %s)", a.id, record and record.name or "?")
end

local function live_record(entity)
	local record = records[entity.id]
	if record == nil or not record.alive then
		error(string.format("entity %d is not valid", entity.id), 3)
	end
	return record
end

function Entity:is_valid()
	local record = records[self.id]
	return record ~= nil and record.alive
end

function Entity:name()
	return live_record(self).name
end

function Entity:get_position()
	return copy(live_record(self).position)
end

function Entity:set_position(position)
	live_record(self).position = copy(position)
end

function Entity:get_rotation()
	return copy(live_record(self).rotation)
end

function Entity:set_rotation(degrees)
	live_record(self).rotation = copy(degrees)
end

function Entity:get_velocity()
	return copy(live_record(self).velocity)
end

function Entity:set_velocity(velocity)
	live_record(self).velocity = copy(velocity)
end

function Entity:is_grounded()
	return live_record(self).grounded
end

local Behaviour = {}
Behaviour.__index = Behaviour
Behaviour.fields = {}

function Behaviour:extend()
	local class = setmetatable({}, { __index = self })
	class.__index = class
	class.fields = {}
	return class
end

function Behaviour:on_start() end

function Behaviour:on_update(_) end

function Behaviour:on_destroy() end

function Behaviour:on_collision(_, _) end

local function load_class(path)
	if classes[path] then
		return classes[path]
	end

	local environment = setmetatable({}, { __index = _G })
	local chunk, message = loadfile(path, "t", environment)
	if not chunk then
		error(message, 0)
	end

	local class = chunk()
	if type(class) ~= "table" then
		error(path .. ": the script must return its behaviour class", 0)
	end

	classes[path] = class
	return class
end

local function call(record, method, ...)
	local behaviour = record.behaviour
	if behaviour == nil or record.broken then
		return
	end

	local ok, message = xpcall(behaviour[method], debug.traceback, behaviour, ...)
	if not ok then
		record.broken = true
		print(string.format("[error] %s.%s: %s", record.name, method, message))
	end
end

local function copy_value(value)
	if is_vec3(value) then
		return copy(value)
	end
	return value
end

function stub.create(name, options)
	options = options or {}

	local tags = {}
	for _, tag in ipairs(options.tags or {}) do
		tags[tag] = true
	end

	local id = next_id
	next_id = next_id + 1

	local record = {
		id = id,
		name = name,
		tags = tags,
		position = copy(options.position or vec3()),
		rotation = copy(options.rotation or vec3()),
		velocity = copy(options.velocity or vec3()),
		radius = options.radius or 0.5,
		grounded = options.grounded or false,
		alive = true,
	}
	records[id] = record
	order[#order + 1] = id

	if options.script then
		local class = load_class(options.script)
		local instance = setmetatable({}, class)
		for field, default in pairs(class.fields or {}) do
			instance[field] = copy_value(default)
		end
		for field, value in pairs(options.fields or {}) do
			if class.fields and class.fields[field] ~= nil then
				instance[field] = copy_value(value)
			else
				print(string.format("[warn] %s: field '%s' is not declared by %s", name, field, options.script))
			end
		end
		instance.entity = handle(id)
		record.behaviour = instance
		pending_start[#pending_start + 1] = record
	end

	return handle(id)
end

stub.prefabs = {}
stub.actions = {}
stub.keys = {}
stub.frame = 0
stub.time = 0

local pressed_actions = {}

function stub.press(action)
	if not stub.actions[action] then
		pressed_actions[action] = true
	end
	stub.actions[action] = true
end

function stub.release(action)
	stub.actions[action] = nil
end

local function alive_ids()
	local ids = {}
	for _, id in ipairs(order) do
		if records[id].alive then
			ids[#ids + 1] = id
		end
	end
	return ids
end

local function pair_key(a, b)
	return a .. ":" .. b
end

local function detect_collisions()
	local ids = alive_ids()
	local now = {}
	local started = {}

	for i = 1, #ids do
		for j = i + 1, #ids do
			local a = records[ids[i]]
			local b = records[ids[j]]
			local offset = b.position - a.position
			if offset:length() < a.radius + b.radius then
				local key = pair_key(a.id, b.id)
				now[key] = true
				if not touching[key] then
					started[#started + 1] = { a = a, b = b, normal = offset:normalized() }
				end
			end
		end
	end

	touching = now
	for _, contact in ipairs(started) do
		if contact.a.alive and contact.b.alive then
			call(contact.a, "on_collision", handle(contact.b.id), contact.normal)
			call(contact.b, "on_collision", handle(contact.a.id), -contact.normal)
		end
	end
end

local function flush_destroyed()
	local queue = pending_destroy
	pending_destroy = {}
	for _, id in ipairs(queue) do
		local record = records[id]
		if record.alive then
			call(record, "on_destroy")
			record.alive = false
		end
	end
end

function stub.step(dt)
	local starting = pending_start
	pending_start = {}
	for _, record in ipairs(starting) do
		if record.alive then
			call(record, "on_start")
		end
	end

	for _, id in ipairs(alive_ids()) do
		call(records[id], "on_update", dt)
	end

	for _, id in ipairs(alive_ids()) do
		local record = records[id]
		record.position = record.position + record.velocity * dt
	end

	detect_collisions()
	flush_destroyed()

	pressed_actions = {}
	stub.frame = stub.frame + 1
	stub.time = stub.time + dt
end

function stub.run(frames, dt)
	dt = dt or 1 / 60
	for _ = 1, frames do
		stub.step(dt)
	end
end

function stub.stop()
	for _, id in ipairs(alive_ids()) do
		call(records[id], "on_destroy")
	end
end

function stub.count(name)
	local count = 0
	for _, id in ipairs(alive_ids()) do
		if records[id].name == name then
			count = count + 1
		end
	end
	return count
end

nene = {
	Behaviour = Behaviour,
	vec3 = vec3,
	input = {},
	world = {},
	log = {},
}

function nene.input.is_action_down(action)
	return stub.actions[action] == true
end

function nene.input.is_action_pressed(action)
	return pressed_actions[action] == true
end

function nene.input.is_key_down(key)
	return stub.keys[key] == true
end

function nene.world.spawn(prefab, position)
	local definition = stub.prefabs[prefab]
	if definition == nil then
		print(string.format("[warn] prefab '%s' is not registered in stub.prefabs", prefab))
		return nil
	end

	local options = {}
	for key, value in pairs(definition) do
		options[key] = value
	end
	options.position = position
	return stub.create(prefab, options)
end

function nene.world.destroy(entity)
	if entity:is_valid() then
		pending_destroy[#pending_destroy + 1] = entity.id
	end
end

function nene.world.find(name)
	for _, id in ipairs(alive_ids()) do
		if records[id].name == name then
			return handle(id)
		end
	end
	return nil
end

function nene.world.find_by_tag(category)
	local found = {}
	for _, id in ipairs(alive_ids()) do
		if records[id].tags[category] then
			found[#found + 1] = handle(id)
		end
	end
	return found
end

function nene.world.find_in_radius(position, radius)
	local found = {}
	for _, id in ipairs(alive_ids()) do
		if (records[id].position - position):length() <= radius then
			found[#found + 1] = handle(id)
		end
	end
	return found
end

function nene.log.info(message)
	print("[info] " .. tostring(message))
end

function nene.log.warn(message)
	print("[warn] " .. tostring(message))
end

function nene.log.error(message)
	print("[error] " .. tostring(message))
end

return stub
