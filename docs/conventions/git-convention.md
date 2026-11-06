# Git Conventions

NeneEngine is developed by a small team that works on trust. There are no pull requests or merge requests: each developer merges their own branch into `master` and is responsible for the result. Nothing checks these rules automatically, so they depend on the author.

## Summary

1. No pull requests or merge requests. The author of a branch merges it into `master`.
2. A branch lands in `master` through a merge commit, `git merge --no-ff`, with Git's default subject and a short body.
3. Branch names, commit messages and tags are written in English. Branch names use lower case `kebab-case`.
4. Branches, commits and tags never mention labs, assignments or their tasks. The history reads like the history of a regular engine, not a study project.
5. A commit is a small logical unit: one local change or one indivisible new feature.
6. Whoever merges resolves the conflicts, and involves other developers only in critical cases.
7. `master` is updated with `--ff-only`. Keep your own branch current with rebase, a shared one with merge.
8. Releases are marked with annotated `vMAJOR.MINOR.PATCH` tags on `master`.

## Branches

### Branch Model

| Branch           | Lives        | Purpose                                                                                                    |
| ---------------- | ------------ | ---------------------------------------------------------------------------------------------------------- |
| `master`         | Permanently  | The latest working state. Every commit on it builds in Debug and Release and runs.                         |
| `<type>/<topic>` | Until merged | One goal: a feature, a fix, a refactoring. Created from `master`, merged back into `master`, then deleted. |

- Do not commit to `master` directly. The only exception is a typo fix in documentation.
- Keep one goal per branch. Fix an unrelated bug found along the way in its own branch or its own commit.
- Branch from another topic branch only when your work depends on its unmerged commits.
- Prefer branches that live days, not weeks. The longer a branch lives, the harder its conflicts become.

### Branch Names

```text
<type>/<topic>
```

```text
feat/enkits-job-system
feat/editor-ui-consistency
fix/inspector-read-only-in-play
refactor/inspector-material-section
build/tracy-0.14
```

- `<type>` is a commit type from [Type](#type), chosen by the main purpose of the branch. 
- `<topic>` names what the branch delivers in two to five English words: `enkits-job-system`, not `my-changes`.
- Use only `a-z`, `0-9`, `-` and one `/` after the type. No upper case, `_`, spaces or nested folders.
- Do not add the author's name, a date or suffixes such as `-v2`, `-final` or `-new`.

| Avoid                      | Prefer                        |
| -------------------------- | ----------------------------- |
| `Feature/JobSystem`        | `feat/job-system`             |
| `feat/editor_ui`           | `feat/editor-ui`              |
| `danii/fixes`              | `fix/mouse-delta-after-focus` |
| `feat/ui-v2-final`         | `feat/editor-ui-consistency`  |
| `feat/lab-5`, `lab5-task3` | `feat/scene-hierarchy-panel`  |

## Commits

NeneEngine uses [Conventional Commits](https://www.conventionalcommits.org/) with a required scope.

### Format

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

### Type

Use exactly one type. If a commit seems to need two, split it.

| Type       | Use for                                                                            | Example                                                      |
| ---------- | ---------------------------------------------------------------------------------- | ------------------------------------------------------------ |
| `feat`     | New behavior a user or engine developer can use                                    | `feat(editor): add component inspector panel`                |
| `fix`      | Corrected behavior in engine code or assets                                        | `fix(app): exit when the main window is closed`              |
| `refactor` | Code changes that keep behavior, including renames, moves and dead code removal    | `refactor(core): rename Resource to CachedResource`          |
| `perf`     | Better performance with the same behavior                                          | `perf(render): enable D3D12 validation only in debug builds` |
| `style`    | Formatting only                                                                    | `style(app): sort includes in AppWindowContextFactory`       |
| `docs`     | Documentation                                                                      | `docs(architecture): describe module dependencies`           |
| `build`    | CMake, dependencies, submodules, developer tooling and CI, including fixes to them | `build(cmake): fix Python3 interpreter lookup on Windows`    |
| `chore`    | Small maintenance that fits no other type                                          | `chore(app): update startup log version to v0.4`             |

### Scope

Name the area that changed, in lower case. Prefer these scopes:

| Scope                                                        | Covers                                                          |
| ------------------------------------------------------------ | --------------------------------------------------------------- |
| `app`                                                        | `App`: bootstrap, frame loop, windows, config services          |
| `core`                                                       | `Core`: logger, resources, paths, timers                        |
| `ecs`                                                        | `ECS`: world, components, systems                               |
| `editor`                                                     | `Editor`: ImGui layer, panels, gizmos, undo/redo                |
| `gamestates`                                                 | `GameStates`: state machine and states                          |
| `graphics`                                                   | `Graphics` module structure: moving or reorganizing its files   |
| `render`                                                     | Rendering behavior in `Graphics`: adapters, loaders, debug draw |
| `input`                                                      | `Input`: input manager and bindings                             |
| `physics`                                                    | Physics simulation, when the change is specifically about it    |
| `platform`                                                   | `Platform`: `IWindow`, Win32                                    |
| `scene`                                                      | `Scene`: loading, serialization, demo scene                     |
| `assets`                                                     | Files under `assets/`                                           |
| `cmake`, `deps`, `submodules`, `tooling`, `ci`               | With `build`                                                    |
| `architecture`, `contributing`, `readme`, `tasks`, `reports` | With `docs`                                                     |

- If a change touches several modules, use the one where its main purpose lies. For example, `refactor(render): share one D3D12 device across all windows` also edits `App`.
- Add a new scope only for an area the list does not cover.
- Use one scope. Do not combine scopes with `,`, `+` or `|`.
- Do not use class names or file names as scopes. Put them in the subject.
- Leave out the scope only when a change really is repository-wide: `style: apply clang-format`.

### Subject

- Use the imperative mood in lower case: `add`, `fix`, `remove`, `move`, `rename`, `extract`.
- Identifiers keep their original case: `remove unused IGameState::IsTransparent`.
- Do not end it with a period.
- Keep the whole first line within 72 characters where possible. Do not go past 80, and do not drop a detail that matters only to reach the limit.
- Describe the result, not the activity. For `fix`, name the correct behavior or the failure that is gone:

| Avoid                            | Prefer                                                                           |
| -------------------------------- | -------------------------------------------------------------------------------- |
| `fix(editor): fix inspector bug` | `fix(editor): make the inspector read-only in play mode`                         |
| `fix(input): mouse changes`      | `fix(input): skip the mouse delta jump after the cursor leaves or focus returns` |
| `refactor(app): cleanup`         | `refactor(app): extract frame loop orchestration service`                        |

The project does not use footers such as `BREAKING CHANGE:` or the `!` marker.

### Body

Most commits have only a subject. Add a body when the reason is not obvious from the subject and the diff:

```text
fix(assets): rename Models and Shaders folders to lower case

Code uses lower-case paths, so these assets were not found outside Windows
```

### Size of a Commit

A commit is a small logical unit: one local change or one indivisible new feature.

- Each commit builds on its own.
- Make one logical change per commit. Keep unrelated fixes and clean-ups in separate commits.
- Keep formatting-only changes apart from behavior changes.
- Split a large feature into steps that each work. If a feature cannot be split without breaking the build or behavior, it goes in one commit.
- Squash commits such as `wip`, `fix typo` or `fix previous commit` with `git rebase -i` before merging, while the branch is still only yours.

## No Study Context

The history must read like the history of a regular engine. Branch names, commit messages, merge commits and tags never contain:

- `lab`, lab or semester numbers;
- numbers or names of assignment tasks: `T5`, `task 3`, `step 8`;
- phrases such as "as required by the assignment" or "for the lab".

Describe what changed in the engine instead:

| Avoid                                                        | Prefer                                         |
| ------------------------------------------------------------ | ---------------------------------------------- |
| `feat(core): add job system for lab 1`                       | `feat(core): add enkiTS job system`            |
| `fix(editor): fix task 3 from lab 5`                         | `fix(editor): keep gizmo selection after undo` |
| `build(cmake): enable Tracy in Release for lab requirements` | `build(cmake): enable Tracy in Release builds` |

The only exception is commits that change nothing but the files in `docs/tasks/` and `docs/reports/`, which hold the assignments and reports themselves. They use the `tasks` and `reports` scopes and should still be worded neutrally: `docs(tasks): add second semester assignments`.

Existing history is not rewritten.

## Daily Workflow

### Starting a Branch

```shell
git switch master
git pull --ff-only
git switch -c feat/job-system
git push -u origin feat/job-system
```

Push the branch right away and then at least at the end of each working day. A branch that exists only on one computer is lost with it, and without pull requests the remote is the only place where the team sees who works on what.

This applies to every branch meant to reach `master`. A short experiment that nobody will merge may stay local.

### Updating `master`

Local `master` is updated only by fast-forward:

```shell
git switch master
git pull --ff-only
```

Run once to make this the default:

```shell
git config --global pull.ff only
```

If `--ff-only` fails, local `master` has commits of its own. Move them to a branch and reset `master` to the remote:

```shell
git switch -c fix/whatever-it-was
git switch master
git reset --hard origin/master
```

### Syncing a Branch with `master`

Sync when you need changes from `master` and right before merging, not after every change to `master`.

| Branch                                 | Method                                                                             |
| -------------------------------------- | ---------------------------------------------------------------------------------- |
| Own: only you commit to it             | `git rebase origin/master`, then `git push --force-with-lease --force-if-includes` |
| Shared: someone else commits to it too | `git merge origin/master`, then `git push`                                         |

```shell
git fetch origin
git rebase origin/master
git push --force-with-lease --force-if-includes
```

- Never force push to `master`.
- Do not force push to a shared branch without the agreement of everyone working on it.
- Use `--force-with-lease --force-if-includes`, never plain `--force`. `--force-with-lease` alone stops protecting you once a Git client such as Fork or an IDE has fetched in the background; `--force-if-includes` (Git 2.30+) closes that gap.
- Enable `git config --global rerere.enabled true` so Git remembers how you resolved a conflict and reapplies it on the next rebase.

#### When an Own Branch Becomes Shared

Every branch is pushed right away, so "own" is an agreement, not something Git knows. Before committing to someone else's branch, tell its author. From that moment the branch is shared and is synced only with merge.

#### Working on One Branch from Two Computers

After a rebase and force push from one computer, `git pull` on the other fails because of `pull.ff only`. Do not merge there. Reset the local branch to the remote one instead:

```shell
git fetch origin
git reset --hard origin/feat/job-system
```

Commit and push everything on the second computer before rebasing on the first, or that work is lost.

#### Branch on Top of Another Branch

If `feat/b` was created from `feat/a` and `feat/a` was rebased or merged into `master`, move only the commits of `feat/b`:

```shell
git fetch origin
git rebase --onto origin/master <old-base> feat/b
```

`<old-base>` is the commit of `feat/a` that `feat/b` started from. Find it with `git log --oneline --graph feat/b`.

## Merging into `master`

The author merges the branch when the work is ready.

### Before Merging

- The branch is synced with the current `master`.
- The project builds in Debug and Release and starts without new errors in the log.
- Commits follow this convention, and `wip` commits are squashed.
- For a risky change (architecture, build system, shared interfaces), consider asking a teammate to look at the branch. It is not required, but welcome.

### Merging

```shell
git fetch origin
git switch master
git pull --ff-only
git merge --no-ff feat/job-system
git push origin master
```

- `--no-ff` is required. After a rebase the branch is ahead of `master`, and a plain `git merge` would fast-forward `master` without a merge commit.
- Write the merge commit message as described in [Merge Commit Message](#merge-commit-message).

If `git push` is rejected, someone has updated `master` in the meantime. Do not run `git pull` on top of your merge commit. Redo the merge on the fresh `master`:

```shell
git fetch origin
git reset --hard origin/master
git switch feat/job-system
git rebase origin/master
git switch master
git merge --no-ff feat/job-system
git push origin master
```

For a shared branch, use `git merge origin/master` instead of `git rebase origin/master`. The reset drops your merge commit, so write its message again.

### Merge Commit Message

```text
Merge branch '<branch>'

<body>
```

```text
Merge branch 'feat/enkits-job-system'

Add a job system on top of enkiTS and move asset loading and physics
stepping to worker threads
```

Subject:

- Keep the subject Git generates: `Merge branch 'feat/job-system'`. Do not translate it, reword it or add a type and scope. The branch name already tells the type and topic, which is why branch names matter.
- `master` never gets `Merge branch 'master' of https://github.com/...` or `Merge remote-tracking branch ...`. Such commits come from a pull without `--ff-only` and are removed before pushing, see [Updating `master`](#updating-master).

Body:

- Required when the branch has more than one commit. Optional for a branch with a single commit, whose subject already says everything.
- One to three lines in English that describe what the branch delivers as a whole, the way a release note would. Follow the [Subject](#subject) rules: imperative mood, the result rather than the activity, lines within 72 characters.
- Do not list the branch's commits. `git log <merge>^1..<merge>^2` shows them.
- If you resolved a non-obvious conflict, add one line on how: `Keep the new material inspector layout from master`.
- No study context, see [No Study Context](#no-study-context).

`git merge --no-ff feat/job-system` opens an editor with the subject already filled in: leave it and add the body after an empty line. Without an editor:

```shell
git merge --no-ff feat/job-system -m "Merge branch 'feat/job-system'" -m "Add a job system on top of enkiTS"
```

In Fork, choose **Create a merge commit** in the merge dialog and edit the message there.

### After Merging

```shell
git branch -d feat/job-system
git push origin --delete feat/job-system
git fetch --prune
```

`git branch -d` refuses to delete an unmerged branch, which is a useful check. Do not reuse a merged branch: new work starts in a new branch from `master`.

## Conflicts

Whoever merges or syncs a branch resolves its conflicts at their own discretion.

1. Find out why the other side changed the code: `git log -p origin/master -- <file>`.
2. Keep both intents where possible. Take one side for a whole file only when the other is clearly obsolete.
3. Build and run the project after resolving. Code that compiles can still be merged wrong.
4. When rebasing, resolve each commit so that it builds. If it goes wrong, `git rebase --abort` or `git merge --abort` returns to the previous state.

Involve other developers only in critical cases:

- you do not understand what the other change does, and a wrong resolution could break it;
- the conflict is in public module interfaces or architecture that someone's current work depends on;
- both sides changed the behavior of the same function in different ways;
- binary files, such as fonts, images or models under `assets/`, cannot be merged by hand, so agree which version stays;
- something broke after the merge and you cannot find the cause.

To check in advance whether a branch will conflict:

```shell
git fetch origin
git merge-tree --write-tree origin/master HEAD
```

## Tags

A tag marks a version of the engine on `master`.

- Use only annotated tags named `vMAJOR.MINOR.PATCH`: `v0.5.0`.
- Put a tag on a merge commit in `master` that builds and runs.
- Raise `MINOR` for notable new features and `PATCH` for a release with fixes only. `MAJOR` stays `0` until the engine is stable.
- The version in code, the startup line in `AppBootstrapService`, matches the latest tag.
- Never move or delete a tag after it is pushed.

```shell
git switch master
git pull --ff-only
git tag -a v0.5.0 -m "NeneEngine v0.5.0"
git push origin v0.5.0
```

Do not use `git push --tags`: it also pushes stray local tags.
