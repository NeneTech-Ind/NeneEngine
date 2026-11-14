# nene stub

A plain Lua 5.4 imitation of the `nene` API from `assets/scripts/meta/nene.lua`. It runs behaviour scripts without the engine, so gameplay scripts can be written and checked before the scripting runtime is merged.

```shell
lua tools/nene-stub/run.lua tools/nene-stub/examples/chaser_scene.lua
```

A scenario file registers prefabs in `stub.prefabs`, creates entities with `stub.create(name, options)`, runs frames with `stub.run(frames, dt)` and checks the result. `stub.press(action)` and `stub.release(action)` drive `nene.input`.

The stub is not a physics engine:

- Positions move by `velocity * dt`. There is no gravity, and `is_grounded` returns the `grounded` option given to `stub.create`.
- Every entity is a sphere of `radius` (0.5 by default). `on_collision` fires once when two spheres start to overlap.
- `nene.world.spawn` reads prefabs from `stub.prefabs`, not from `assets/prefabs`.
