# Naming Conventions

This document defines the permanent naming conventions for the NeneEngine codebase.

The goal is not stylistic purity. The goal is predictability. A reader should be able to infer what a file, type, or module does from its name, and where related code should live.

## Scope

These rules apply to:
- module and folder names
- header and source file names
- class, struct, enum, and function names
- include paths introduced in project code

These rules do not apply to:
- third-party code in `external/`
- upstream API names that must be preserved for interoperability
- serialized asset filenames unless a change is explicitly planned and coordinated

## Core Principles

- Prefer domain names over technical names. Name code after what it represents, not how it happens to be implemented.
- Use one term for one concept. If the project says `Graphics`, do not introduce a parallel term like `Rendering` for the same boundary.
- Make broad names earn their breadth. Names like `Resource`, `Manager`, `Utils`, or `System` should only be used when the scope is truly generic.
- Prefer explicit suffixes when they clarify responsibility.
- Avoid temporary-sounding names in permanent runtime code.

## General Rules

- Use `PascalCase` for types, files, and folders in project code.
- Use `camelCase` for local variables, parameters, and non-constant function names that follow existing project style.
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

## Module Naming

Top-level project modules should reflect ownership and responsibility.

- `App`: composition root, startup, frame loop, runtime orchestration
- `Core`: reusable engine utilities and foundational abstractions
- `ECS`: entities, components, systems, world orchestration
- `Graphics`: renderer backends, GPU-facing loaders, render runtime helpers
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

Acceptable current example:

- `ResourceManager`

Before introducing a new `*Manager`, ask whether the name should really be `*Service`, `*Registry`, `*Store`, `*Loader`, or a domain-specific noun.

## Function Naming

- Prefer verbs for actions and nouns for queries.
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

- Include paths should mirror module ownership.
- If a type lives in `Graphics/Runtime`, include it from `Graphics/Runtime/...`, not through an old alias path.
- Do not keep parallel include roots for the same concept once a module has been renamed.
- Namespace names should remain stable and domain-oriented. Prefer moving files before inventing extra namespace variants.

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

If you introduce an exception, document the reason in the relevant PR or code review.

## Review Checklist

Before adding a new file, type, or module, check:

- Does the name describe the domain responsibility clearly?
- Does it reuse an existing project term instead of inventing a synonym?
- Is the suffix accurate: `Service`, `Config`, `Loader`, `Binding`, `Manager`?
- Would a new teammate know where to find related code from the name alone?
- Does the include path reflect the real module owner?
- Is the chosen name likely to stay correct if implementation details change?
