# Git Conventions

NeneEngine is developed by a small team that works on trust. There are no pull requests or merge requests: each developer merges their own branch into the current release branch and is responsible for the result. Only the release maintainer merges a release into `main`. Apart from the protection of `main` on GitHub, nothing checks these rules automatically, so they depend on the author.

## Summary

1. `main` holds released versions only. Every commit on it is a release merge or a hotfix merge, and each of them is tagged `vMAJOR.MINOR.PATCH`.
2. Each release is assembled in its own `release/MAJOR.MINOR.PATCH` branch, created from `main` and deleted after it is merged back.
3. Work happens in `<type>/<topic>` branches created from the current release branch. The author merges their branch into that release branch with `git merge --no-ff`.
4. No pull requests or merge requests. Whoever merges a branch keeps Git's default merge subject and adds a short body.
5. A release ends with one version bump commit in `CMakeLists.txt`, a `git merge --no-ff` into `main` and an annotated tag on that merge.
6. Branch names, commit messages and tags are written in English. Branch names use lower case `kebab-case`.
7. Branches, commits and tags never mention labs, assignments or their tasks. The history reads like the history of a regular engine, not a study project.
8. A commit is a small logical unit: one local change or one indivisible new feature.
9. Whoever merges resolves the conflicts, and involves other developers only in critical cases.
10. Local `main` and release branches are updated with `--ff-only`. Keep your own branch current with rebase, a shared one with merge.

## Branches

### Branch Model

| Branch                    | Lives          | Created from | Merged into            | Purpose                                                                     |
| ------------------------- | -------------- | ------------ | ---------------------- | --------------------------------------------------------------------------- |
| `main`                    | Permanently    | -            | -                      | Released versions. Every commit on it builds in Debug and Release and runs. |
| `release/<version>`       | Until released | `main`       | `main`                 | Collects the work of one release. Every merge into it builds and runs.      |
| `<type>/<topic>`          | Until merged   | `release/*`  | The same `release/*`   | One goal: a feature, a fix, a refactoring.                                  |
| `fix/<topic>` from `main` | Until merged   | `main`       | `main`, then `release/*` | A hotfix for a released version, see [Hotfixes](#hotfixes).               |

```text
main            ●──────────────────────────────────●  v1.1.0
                 \                                /
release/1.1.0     ●───────●───────────●──────●───●  chore(release): bump version to 1.1.0
                   \     /   \       /      /
feat/lua-runtime    ●──●      \     /      /
feat/prefab-api                ●──●───●   /
fix/...                                ●─●
```

- Nobody commits to `main` directly, and only the release maintainer merges into it.
- Do not commit to a release branch directly. The exceptions are the version bump commit and a typo fix in documentation.
- There is one open release branch at a time. Start new work from it, not from `main`.
- Keep one goal per branch. Fix an unrelated bug found along the way in its own branch or its own commit.
- Branch from another topic branch only when your work depends on its unmerged commits.
- Prefer branches that live days, not weeks. The longer a branch lives, the harder its conflicts become.

### Release Maintainer

One developer is the release maintainer for each release. They create the release branch, announce it to the team, make the version bump commit, merge the release into `main`, tag it and delete the release branch. Everyone else only merges their own topic branches into the release branch.

### Branch Names

```text
<type>/<topic>
release/<MAJOR>.<MINOR>.<PATCH>
```

```text
feat/enkits-job-system
feat/editor-ui-consistency
fix/inspector-read-only-in-play
refactor/inspector-material-section
build/tracy-0.14
release/1.1.0
```

- `<type>` is a commit type from [Type](#type), chosen by the main purpose of the branch. `release` is reserved for release branches.
- `<topic>` names what the branch delivers in two to five English words: `enkits-job-system`, not `my-changes`.
- A release branch is named after the version it will produce, without the `v`: `release/1.1.0`.
- Use only `a-z`, `0-9`, `-` and one `/` after the type, plus `.` in release versions. No upper case, `_`, spaces or nested folders.
- Do not add the author's name, a date or suffixes such as `-v2`, `-final` or `-new`.

| Avoid                      | Prefer                        |
| -------------------------- | ----------------------------- |
| `Feature/JobSystem`        | `feat/job-system`             |
| `feat/editor_ui`           | `feat/editor-ui`              |
| `danii/fixes`              | `fix/mouse-delta-after-focus` |
| `feat/ui-v2-final`         | `feat/editor-ui-consistency`  |
| `feat/lab-5`, `lab5-task3` | `feat/scene-hierarchy-panel`  |
| `release-candidate`, `rc`  | `release/1.1.0`               |
| `release/lab-2`            | `release/1.1.0`               |

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
| `chore`    | Small maintenance that fits no other type, including the version bump             | `chore(release): bump version to 1.1.0`                      |

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
| `release`                                                    | With `chore`: the version bump that closes a release            |
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
| `release/lab-2`, `v1.1.0-lab2`                               | `release/1.1.0`, `v1.1.0`                      |

A release may match a lab in practice, but only its version number appears in Git.

The only exception is commits that change nothing but the files in `docs/tasks/` and `docs/reports/`, which hold the assignments and reports themselves. They use the `tasks` and `reports` scopes and should still be worded neutrally: `docs(tasks): add second semester assignments`.

Existing history is not rewritten.

## Daily Workflow

### One-Time Setup

```shell
git config --global pull.ff only
git config --global rerere.enabled true
```

`pull.ff only` makes `git pull` refuse to create merge commits, and `rerere` makes Git remember how you resolved a conflict and reapply it on the next rebase.

### Starting a Branch

Branch from the current release branch. The release maintainer announces its name, and `git branch -r --list "origin/release/*"` shows it.

```shell
git fetch origin
git switch release/1.1.0
git pull --ff-only
git switch -c feat/lua-runtime
git push -u origin feat/lua-runtime
```

Push the branch right away and then at least at the end of each working day. A branch that exists only on one computer is lost with it, and without pull requests the remote is the only place where the team sees who works on what.

This applies to every branch meant to reach a release. A short experiment that nobody will merge may stay local.

### Updating `main` and the Release Branch

Local `main` and release branches are updated only by fast-forward:

```shell
git switch release/1.1.0
git pull --ff-only
```

If `--ff-only` fails, the local branch has commits of its own. Move them to a branch and reset the local branch to the remote:

```shell
git switch -c fix/whatever-it-was
git switch release/1.1.0
git reset --hard origin/release/1.1.0
```

### Syncing a Branch with the Release Branch

Sync when you need changes from the release branch and right before merging, not after every change to it.

| Branch                                 | Method                                                                                    |
| -------------------------------------- | ----------------------------------------------------------------------------------------- |
| Own: only you commit to it             | `git rebase origin/release/1.1.0`, then `git push --force-with-lease --force-if-includes` |
| Shared: someone else commits to it too | `git merge origin/release/1.1.0`, then `git push`                                         |

```shell
git fetch origin
git rebase origin/release/1.1.0
git push --force-with-lease --force-if-includes
```

- Never force push to `main` or a release branch.
- Do not force push to a shared branch without the agreement of everyone working on it.
- Use `--force-with-lease --force-if-includes`, never plain `--force`. `--force-with-lease` alone stops protecting you once a Git client such as Fork or an IDE has fetched in the background; `--force-if-includes` (Git 2.30+) closes that gap.

#### When an Own Branch Becomes Shared

Every branch is pushed right away, so "own" is an agreement, not something Git knows. Before committing to someone else's branch, tell its author. From that moment the branch is shared and is synced only with merge.

#### Working on One Branch from Two Computers

After a rebase and force push from one computer, `git pull` on the other fails because of `pull.ff only`. Do not merge there. Reset the local branch to the remote one instead:

```shell
git fetch origin
git reset --hard origin/feat/lua-runtime
```

Commit and push everything on the second computer before rebasing on the first, or that work is lost.

#### Branch on Top of Another Branch

If `feat/b` was created from `feat/a` and `feat/a` was rebased or merged into the release branch, move only the commits of `feat/b`:

```shell
git fetch origin
git rebase --onto origin/release/1.1.0 <old-base> feat/b
```

`<old-base>` is the commit of `feat/a` that `feat/b` started from. Find it with `git log --oneline --graph feat/b`.

## Merging into the Release Branch

The author merges the branch when the work is ready.

### Before Merging

- The branch is synced with the current release branch.
- The project builds in Debug and Release and starts without new errors in the log.
- Commits follow this convention, and `wip` commits are squashed.
- For a risky change (architecture, build system, shared interfaces), consider asking a teammate to look at the branch. It is not required, but welcome.

### Merging

```shell
git fetch origin
git switch release/1.1.0
git pull --ff-only
git merge --no-ff feat/lua-runtime
git push origin release/1.1.0
```

- `--no-ff` is required. After a rebase the branch is ahead of the release branch, and a plain `git merge` would fast-forward it without a merge commit.
- Write the merge commit message as described in [Merge Commit Message](#merge-commit-message).

If `git push` is rejected, someone has updated the release branch in the meantime. Do not run `git pull` on top of your merge commit. Redo the merge on the fresh release branch:

```shell
git fetch origin
git reset --hard origin/release/1.1.0
git switch feat/lua-runtime
git rebase origin/release/1.1.0
git switch release/1.1.0
git merge --no-ff feat/lua-runtime
git push origin release/1.1.0
```

For a shared branch, use `git merge origin/release/1.1.0` instead of `git rebase origin/release/1.1.0`. The reset drops your merge commit, so write its message again.

### Merge Commit Message

```text
<subject Git generates>

<body>
```

```text
Merge branch 'feat/enkits-job-system' into release/1.1.0

Add a job system on top of enkiTS and move asset loading and physics
stepping to worker threads
```

Subject:

- Keep the subject Git generates. A topic branch merged into a release branch gets `Merge branch 'feat/lua-runtime' into release/1.1.0`, and a release merged into `main` gets `Merge branch 'release/1.1.0'`. Do not translate it, reword it or add a type and scope. The branch name already tells the type and topic, which is why branch names matter.
- `main` and release branches never get `Merge branch 'release/1.1.0' of https://github.com/...` or `Merge remote-tracking branch ...`. Such commits come from a pull without `--ff-only` and are removed before pushing, see [Updating `main` and the Release Branch](#updating-main-and-the-release-branch).

Body:

- Required when the branch has more than one commit. Optional for a branch with a single commit, whose subject already says everything.
- One to three lines in English that describe what the branch delivers as a whole, the way a release note would. Follow the [Subject](#subject) rules: imperative mood, the result rather than the activity, lines within 72 characters.
- Do not list the branch's commits. `git log <merge>^1..<merge>^2` shows them.
- If you resolved a non-obvious conflict, add one line on how: `Keep the new material inspector layout from the release branch`.
- No study context, see [No Study Context](#no-study-context).

`git merge --no-ff feat/lua-runtime` opens an editor with the subject already filled in: leave it and add the body after an empty line. Without an editor:

```shell
git merge --no-ff feat/lua-runtime -m "Merge branch 'feat/lua-runtime' into release/1.1.0" -m "Embed Lua through sol2 and run behaviour scripts as a world system"
```

In Fork, choose **Create a merge commit** in the merge dialog and edit the message there.

### After Merging

```shell
git branch -d feat/lua-runtime
git push origin --delete feat/lua-runtime
git fetch --prune
```

`git branch -d` refuses to delete an unmerged branch, which is a useful check. Do not reuse a merged branch: new work starts in a new branch from the release branch.

## Releases

The release maintainer runs every step in this section. Follow the [release convention](release-convention.md) to build and test the package from the local tag and publish it on GitHub.

### Starting a Release

Right after the previous release is tagged, create the next release branch from `main` and tell the team its name:

```shell
git fetch origin
git switch main
git pull --ff-only
git switch -c release/1.2.0
git push -u origin release/1.2.0
```

Choose the version by the rules in [Tags](#tags). The branch name is a plan: if the scope changes, finish the release under the planned version anyway.

### Finishing a Release

1. Every topic branch planned for the release is merged, and the release branch builds in Debug and Release and passes `--smoke-test`.
2. Bump the version in the `project()` call of the top-level `CMakeLists.txt`. It is the only place that holds the version: the startup log line and the About window read it from `NENEENGINE_VERSION`.

   ```cmake
   project(
       NeneEngine
       VERSION 1.1.0
   ```

3. Commit the bump on the release branch, then merge the release into `main` and tag the merge:

   ```shell
   git switch release/1.1.0
   git pull --ff-only
   git commit -am "chore(release): bump version to 1.1.0"
   git push origin release/1.1.0

   git switch main
   git pull --ff-only
   git merge --no-ff release/1.1.0
   git tag -a v1.1.0 -m "NeneEngine v1.1.0"
   ```

   The body of the release merge is a short release note: one to three lines on what the release delivers as a whole.

4. Build and test the ZIP from the local tag. Once it passes, push `main` and the tag, then publish the GitHub release.

   ```shell
   git push origin main v1.1.0
   ```

5. Delete the release branch and start the next one:

   ```shell
   git branch -d release/1.1.0
   git push origin --delete release/1.1.0
   git fetch --prune
   ```

`main` only ever receives merges, so `git merge --no-ff release/1.1.0` cannot conflict unless a hotfix landed on `main` and was not merged into the release branch.

### Hotfixes

A fix that cannot wait for the next release goes straight to `main`:

```shell
git switch main
git pull --ff-only
git switch -c fix/crash-on-empty-scene
```

The fix branch ends with its own bump commit that raises `PATCH`: `chore(release): bump version to 1.1.1`. The release maintainer merges it into `main` with `--no-ff` and tags the merge `v1.1.1`. Build and test the ZIP from that tag, push `main` and the tag, and publish the GitHub release. Merge `main` into the open release branch to carry the fix forward:

```shell
git switch release/1.2.0
git pull --ff-only
git merge --no-ff main
git push origin release/1.2.0
```

## Conflicts

Whoever merges or syncs a branch resolves its conflicts at their own discretion.

1. Find out why the other side changed the code: `git log -p origin/release/1.1.0 -- <file>`.
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
git merge-tree --write-tree origin/release/1.1.0 HEAD
```

## Tags

A tag marks a released version of the engine on `main`.

- Use only annotated tags named `vMAJOR.MINOR.PATCH`: `v1.1.0`.
- Put a tag only on a release or hotfix merge commit in `main`.
- Raise `MINOR` for a release with new features and `PATCH` for a hotfix. Raise `MAJOR` only when the team agrees on an incompatible change, such as a new scene or asset format.
- The version in `CMakeLists.txt` on `main` matches the latest tag.
- Never move or delete a tag after it is pushed.
- `v1.0.0` marks the commit that adopted this branch model and is the current state of the engine. Earlier history has no tags.

Push a tag by name, never with `git push --tags`: it also pushes stray local tags.

## Protection on GitHub

The repository settings back the rules that matter most:

- `main` is the default branch.
- A ruleset for `main` blocks deletion and force pushes and restricts updates, so only the release maintainer can push to it.
- A ruleset for `release/*` blocks deletion and force pushes. Everyone on the team can push merges to it.
- A tag ruleset for `v*` blocks updates and deletion of pushed tags.
