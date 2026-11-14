local Chaser = nene.Behaviour:extend()

Chaser.fields = {
	speed = 3.0,
	target = "player",
}

function Chaser:on_start()
	nene.log.info(self.entity:name() .. " hunts " .. self.target)
end

function Chaser:on_update(_)
	local target = nene.world.find(self.target)
	if target == nil then
		self.entity:set_velocity(nene.vec3(0, 0, 0))
		return
	end

	local direction = (target:get_position() - self.entity:get_position()):normalized()
	local velocity = self.entity:get_velocity()
	self.entity:set_velocity(nene.vec3(direction.x * self.speed, velocity.y, direction.z * self.speed))
end

function Chaser:on_collision(other, _)
	if other:name() == self.target then
		nene.log.info(self.entity:name() .. " caught " .. other:name())
		nene.world.destroy(other)
	end
end

return Chaser
