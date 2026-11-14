local directory = arg[0]:match("^(.*)[/\\]") or "."
package.path = directory .. "/?.lua;" .. package.path

local scenario = arg[1]
if scenario == nil then
	print("usage: lua tools/nene-stub/run.lua <scenario.lua>")
	os.exit(1)
end

stub = require("nene_stub")
stub.directory = directory

dofile(scenario)
