# Naming Conventions

This document defines the permanent naming conventions for the NeneEngine codebase.

The goal is not stylistic purity. The goal is predictability. A reader should be able to infer what a file, type, or module does from its name, and where related code should live.

## Scope

These rules apply to:
- module and folder names
- header and source file names
- type, function, variable, member, constant, and macro names
- include paths introduced in project code
- asset paths under `assets/`

These rules do not apply to:
- third-party code in `external/`
- upstream API names that must be preserved for interoperability
- file names inside third-party asset packs (for example `PaRappa The Rapper/`), which keep their original names

## Core Principles

- Prefer domain names over technical names. Name code after what it represents, not how it happens to be implemented.
- Use one term for one concept. If the project says `Graphics`, do not introduce a parallel term like `Rendering` for the same boundary.
- Make broad names earn their breadth. Names like `Utils`, `Helpers`, `Misc`, `Manager`, or a bare `Resource` should only be used when the scope is truly generic.
- Prefer explicit suffixes when they clarify responsibility.
- Avoid temporary-sounding names in permanent runtime code.

## General Rules

- Use `PascalCase` for namespaces, types, enum types and enumerators, functions and methods, and for files and module folders under `include/` and `src/`.
- Keep acronyms upper case inside `PascalCase` names: `GPUMesh`, `DiligentDX12Adapter`, `ECS`, `EASTLStdHash`.
- Use `camelCase` for local variables, parameters, and public data members of plain structs.
- Use `m_camelCase` for private and protected data members of classes.
- Use `kPascalCase` for constants that are local to a source file (anonymous namespace in a `.cpp`).
- Use plain `PascalCase` for constants exposed through headers (`NullEntity`, `InputActions::Pause`).
- Use `UPPER_SNAKE_CASE` for preprocessor macros and resource ids.
- Use the platform's canonical term when one exists.
  - Prefer: `Win32`
  - Avoid: `Windows32`
- Use `Demo` or `Sample` for shipped default runtime content.
  - Prefer: `DemoScene`
  - Avoid: `TestScene`
- Do not use bare ambiguous names if the same word already exists in another domain.
  - Prefer: `Win32ResourceIds.h`
  - Avoid: `Resource.h`
- Use lower case `snake_case` for folders and files under `assets/`, and reference them in lower case from code. Windows
  hides case mismatches, but the paths must also resolve on case-sensitive file systems.
  - Prefer: `assets/models/spawn_manifest.json`, `assets/scenes/demo_scene.json`
  - Avoid: `assets/Models/`, `assets/Shaders/textured_mesh.shader`

## Module Naming

Top-level project modules should reflect ownership and responsibility.

- `App`: composition root, startup, frame loop, runtime orchestration
- `Core`: reusable engine utilities and foundational abstractions
- `ECS`: entities, components, systems (including physics and rendering systems), world orchestration
- `Graphics`: the render adapter interface and its backends, loaders that turn asset files into render resources, and
  render data types
- `Input`: input abstractions and action binding support
- `Platform`: OS-specific integration such as windowing
- `Scene`: scene data, serialization, demo scene bootstrap, scene instantiation
- `GameStates`: gameplay state flow and concrete game-state implementations

Do not create a new top-level module when the code clearly belongs to one of these existing boundaries.

## File Naming

File names should match the primary type or responsibility inside the file.

- One primary type per file when practical.
- Header/source pairs should use the same stem.
- Name files after the abstraction they expose, not the caller that uses them.

Prefer:
- `AppWindowRuntimeService.h`
- `MeshRenderBinding.h`
- `TextureLoader.h`
- `ModelSpawnManifest.h`

Avoid:
- `WindowStuff.h`
- `RuntimeHelpers.h`
- `MiscRendering.h`
- `TempScene.h`

## Type Naming

### Interfaces

- Use `I` prefix for pure abstract interfaces.
- The rest of the name should describe the role, not the implementation.

Prefer:
- `IRenderAdapter`
- `IWindow`
- `IGameState`

### Services

Use `*Service` for app-owned orchestration objects with a clear lifecycle and coordination responsibility.

Prefer:
- `AppBootstrapService`
- `AppRuntimeConfigService`
- `AppWindowRuntimeService`

Avoid using `Service` for passive data containers or utility namespaces.

### Config Types

Use `*Config` for structured configuration data loaded, stored, or passed around as configuration.

Prefer:
- `AppConfig`
- `SceneConfig`
- `ModelInstanceConfig`

Use more specific names when the file contains several related config records:
- `ModelSpawnManifestConfig`
- `SceneEntityMaterialOverrideConfig`

### Loaders

Use `*Loader` for code that converts external assets or files into engine-readable resources.

Prefer:
- `MeshLoader`
- `ShaderLoader`
- `TextureLoader`

### Bindings

Use `*Binding` for objects or helpers that connect runtime resources to another system boundary.

Prefer:
- `MeshRenderBinding`
- `MeshRenderRuntimeBinding`

Avoid `Binder` unless the code is an actual long-lived object with binder semantics.

### Managers

Use `*Manager` sparingly. It should represent a broad coordination or ownership surface, not a vague "place for logic".

Acceptable current examples:

- `ResourceManager`
- `InputManager`

Before introducing a new `*Manager`, ask whether the name should really be `*Service`, `*Registry`, `*Store`, `*Loader`, or a domain-specific noun.

### Other Suffixes

- `*Component` is a plain data struct attached to an entity. It holds no behaviour beyond trivial accessors.
- `*System` is an `ECS::ISystem` implementation that operates on components.
- `*State` is an `IGameState` implementation. Do not use it for other kinds of state.
- `*Adapter` wraps a third-party backend behind an engine interface (`DiligentDX12Adapter`).
- `*Factory` creates and initializes objects but does not own them afterwards (`AppWindowContextFactory`).
- `*Policy` is a pure decision with no side effects (`EvaluateAppConfigHotReload` in `AppRuntimeConfigPolicy`).
- `*Event` is a plain struct published through `EventBus` or a delegate (`CollisionEvent`, `KeyEvent`).
- `*Settings` holds options changed at runtime, while `*Config` holds data loaded from files (`DebugDrawSettings`).
- `*Impl` is reserved for a pimpl struct or a file-local implementation of a third-party interface.

## Function Naming

- Start actions with a verb: `Load`, `Create`, `Spawn`, `Apply`, `Update`.
- Name boolean queries as yes/no questions (`IsValid`, `HasComponent`, `ShouldClose`, `AreAllWindowsClosed`).
- Prefer `Get` for accessors and computed values (`GetComponent`, `GetDefaultScenePath`). Use `Find` or `Try` when the
  result may be absent (`FindPrimaryCameraEntity`, `TryParseKeyCode`).
- Use `To*` for conversions between representations (`ToString`, `ToJolt`).
- Use names that describe the effect at the call site.

Prefer:
- `LoadOrCreate`
- `CreateTexturedMeshShader`
- `SpawnModelsFromManifest`
- `ApplyRuntimeConfig`

Avoid:
- `DoStuff`
- `HandleThing`
- `ProcessData` when the data/domain is not obvious

## Namespace And Include Rules

- All project code lives in the `NeneEngine` namespace. The global namespace is shared with Windows headers and every
  third-party library, so a generic project name such as `GameTimer` placed there can collide with theirs.
- Keep the global namespace only for what the language or a library requires: the `wWinMain` entry point (with its
  file-local helpers in `WinMain.cpp`), the `operator new[]` overloads EASTL links against, and specializations of
  third-party templates, which must live in that library's namespace (`eastl::hash<std::string>`).
- Use nested namespaces sparingly: for a distinct subdomain (`NeneEngine::ECS`) or to group related free functions and
  constants (`DemoScene`, `InputActions`). Do not mirror every folder with a namespace.
- Put file-local helpers in an anonymous namespace inside `NeneEngine` rather than marking them `static`.
- Include paths should mirror module ownership.
- If a type lives in `Graphics/Runtime`, include it from `Graphics/Runtime/...`, not through an old alias path.
- Do not keep parallel include roots for the same concept once a module has been renamed.
- Namespace names should remain stable and domain-oriented. Prefer moving files before inventing extra namespace variants.

## Module Dependencies

Names say where code lives; dependencies say what it may use. The intended direction is from the top down, and a module
may include only modules below it:

```
App
GameStates   Scene
ECS
Graphics   Platform
Input
Core
```

Rules:

- Modules on the same row must not include each other.
- `Core` must not depend on any other module.
- `Graphics/Backend` is the only place that may include Diligent headers. Other modules depend on `IRenderAdapter` and
  `Graphics/Runtime/RenderTypes.h`, and only `App` constructs a concrete backend.
- `ECS` must not include `Scene`, `GameStates`, or `App`.

## Prefer / Avoid

Prefer:
- `Platform/Win32/Win32Window.h`
- `Graphics/Backend/IRenderAdapter.h`
- `Graphics/Loaders/TextureLoader.h`
- `Graphics/Runtime/RenderTypes.h`
- `Scene/Instantiation/ModelSpawner.h`
- `GameStates/PlayState.h`

Avoid:
- `Platform/Windows32/Windows32Window.h`
- `RenderAdapters/IRenderAdapter.h`
- `Rendering/TextureLoader.h`
- `Rendering/RenderTypes.h`
- `Rendering/ModelSpawner.h`
- `States/PlayState.h`

## Exceptions

The following exceptions are acceptable when there is a concrete reason:

- upstream library names required by external APIs
- asset names that are already referenced by manifests, serialized data, or documentation
- compatibility shims during short-lived migrations

If you introduce an exception, document the reason in a comment next to the name, where future readers will see it.

## Review Checklist

Before adding a new file, type, or module, check:

- Does the name describe the domain responsibility clearly?
- Does it reuse an existing project term instead of inventing a synonym?
- Is the suffix accurate (`Service`, `Config`, `Settings`, `Loader`, `Binding`, `Manager`, `Factory`, `Policy`, and the
  other suffixes above)?
- Does it fit the module dependency direction?
- Would a new teammate know where to find related code from the name alone?
- Does the include path reflect the real module owner?
- Is the chosen name likely to stay correct if implementation details change?
