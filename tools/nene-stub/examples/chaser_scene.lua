stub.prefabs.enemy = {
	script = stub.directory .. "/examples/chaser.lua",
	fields = { speed = 4.0 },
}

stub.create("player", { position = nene.vec3(10, 0, 0) })
nene.world.spawn("enemy", nene.vec3(0, 0, 0))

while stub.count("player") > 0 and stub.frame < 180 do
	stub.step(1 / 60)
end

assert(stub.count("player") == 0, "the enemy should have caught the player")
print(string.format("ok: player caught after %.2f s", stub.time))
