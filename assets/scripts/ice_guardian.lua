local fighters = require("duel_fighters")
local Fighter = nene.Behaviour:extend()

Fighter.fields = {
	max_hp = 28,
	attack_damage = 4,
	charged_damage = 12,
}

function Fighter:on_start()
	fighters.guardian = self
end

function Fighter:on_destroy()
	if fighters.guardian == self then
		fighters.guardian = nil
	end
end

return Fighter
