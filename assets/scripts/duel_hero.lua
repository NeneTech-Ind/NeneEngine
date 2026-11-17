local fighters = require("duel_fighters")
local Fighter = nene.Behaviour:extend()

Fighter.fields = {
	max_hp = 30,
	max_mana = 6,
	attack_damage = 5,
	fire_damage = 9,
	fire_mana_cost = 3,
}

function Fighter:on_start()
	fighters.hero = self
end

function Fighter:on_destroy()
	if fighters.hero == self then
		fighters.hero = nil
	end
end

return Fighter
