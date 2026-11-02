# Commit Messages

NeneEngine uses [Conventional Commits](https://www.conventionalcommits.org/) with a required scope. Nothing checks it automatically, so it depends on the author.

## Format

```text
<type>(<scope>): <subject>

<optional body>
```

Examples from the history:

```text
fix(input): keep keyboard state when the cursor leaves the window
feat(editor): add command-based undo/redo history
refactor(ecs): reset systems on scene restore through an ISystem hook
perf(render): enable D3D12 validation only in debug builds
build(deps): add Jolt Physics via vcpkg
docs(architecture): expand naming conventions and module dependencies
```

## Type

Use exactly one type. If a commit seems to need two, split it.

| Type | Use for | Example |
| --- | --- | --- |
| `feat` | New behavior a user or engine developer can use | `feat(editor): add component inspector panel` |
| `fix` | Corrected behavior in engine code or assets | `fix(app): exit when the main window is closed` |
| `refactor` | Code changes that keep behavior, including renames, moves and dead code removal | `refactor(core): rename Resource to CachedResource` |
| `perf` | Better performance with the same behavior | `perf(render): enable D3D12 validation only in debug builds` |
| `style` | Formatting only | `style(app): sort includes in AppWindowContextFactory` |
| `docs` | Documentation, reports and lab assignments | `docs(tasks): add second semester lab assignments` |
| `build` | CMake, dependencies, submodules, developer tooling and CI, including fixes to them | `build(cmake): fix Python3 interpreter lookup on Windows` |
| `chore` | Small maintenance that fits no other type | `chore(app): update startup log version to v0.4` |

## Scope

Name the area that changed, in lower case. Prefer these scopes:

| Scope | Covers |
| --- | --- |
| `app` | `App`: bootstrap, frame loop, windows, config services |
| `core` | `Core`: logger, resources, paths, timers |
| `ecs` | `ECS`: world, components, systems |
| `editor` | `Editor`: ImGui layer, panels, gizmos, undo/redo |
| `gamestates` | `GameStates`: state machine and states |
| `graphics` | `Graphics` module structure: moving or reorganizing its files |
| `render` | Rendering behavior in `Graphics`: adapters, loaders, debug draw |
| `input` | `Input`: input manager and bindings |
| `physics` | Physics simulation, when the change is specifically about it |
| `platform` | `Platform`: `IWindow`, Win32 |
| `scene` | `Scene`: loading, serialization, demo scene |
| `assets` | Files under `assets/` |
| `cmake`, `deps`, `submodules`, `tooling`, `ci` | With `build` |
| `architecture`, `contributing`, `readme`, `tasks`, `reports` | With `docs` |

- If a change touches several modules, use the one where its main purpose lies. For example, `refactor(render): share one D3D12 device across all windows` also edits `App`.
- Add a new scope only for an area the list does not cover.
- Use one scope. Do not combine scopes with `,`, `+` or `|`.
- Do not use class names or file names as scopes. Put them in the subject.
- Leave out the scope only when a change really is repository-wide: `style: apply clang-format`.

## Subject

- Use the imperative mood in lower case: `add`, `fix`, `remove`, `move`, `rename`, `extract`.
- Identifiers keep their original case: `remove unused IGameState::IsTransparent`.
- Do not end it with a period.
- Keep the whole first line within 72 characters where possible. Do not go past 80, and do not drop a detail that matters only to reach the limit.
- Write it in English.
- Describe the result, not the activity. For `fix`, name the correct behavior or the failure that is gone:

| Avoid | Prefer |
| --- | --- |
| `fix(editor): fix inspector bug` | `fix(editor): make the inspector read-only in play mode` |
| `fix(input): mouse changes` | `fix(input): skip the mouse delta jump after the cursor leaves or focus returns` |
| `refactor(app): cleanup` | `refactor(app): extract frame loop orchestration service` |

The project does not use footers such as `BREAKING CHANGE:` or the `!` marker.

## Body

Most commits have only a subject. Add a body when the reason is not obvious from the subject and the diff:

```text
fix(assets): rename Models and Shaders folders to lower case

Code uses lower-case paths, so these assets were not found outside Windows
```

## Scope of a Commit

- Make one logical change per commit. Keep unrelated fixes and clean-ups in separate commits, as the history does when several small fixes land together.
- Keep formatting-only changes apart from behavior changes.
