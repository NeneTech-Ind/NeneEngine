-- Allow the editor to read the fields before the behaviour runtime is available.
local SpinCube = nene.Behaviour and nene.Behaviour:extend() or {}

-- Degrees per second.
SpinCube.fields = {
	rotation_speed = 60.0,
}
SpinCube.field_metadata = {
	rotation_speed = { type = "float", min = 0, max = 360 },
}

function SpinCube:on_start()
	self.initial_rotation = self.entity:get_rotation()
	self.angle = self.initial_rotation.z
end

function SpinCube:on_update(dt)
	self.angle = (self.angle + self.rotation_speed * dt) % 360.0
	self.entity:set_rotation(nene.vec3(self.initial_rotation.x, self.initial_rotation.y, self.angle))
end

return SpinCube
