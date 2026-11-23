# Release Convention

The release maintainer builds the Windows x64 package, checks it, and publishes it on GitHub.

## Prepare the release

Follow the [Git conventions for releases](git-convention.md#releases) to check the release branch, bump the version in the root `CMakeLists.txt`, merge into `main`, and create the annotated `vX.Y.Z` tag. For a hotfix, follow the [hotfix steps](git-convention.md#hotfixes) to prepare the merge and tag.

Build and test the ZIP from the local tag before pushing `main` and the tag. The CMake version `X.Y.Z`, tag `vX.Y.Z`, and archive `NeneEngine-X.Y.Z-win64.zip` must refer to the same version.

## Build and test the package

Use an x64 Developer PowerShell for Visual Studio with MSVC, CMake, Ninja, and `VCPKG_ROOT` set. Run the commands from the repository root. Substitute the release version for `X.Y.Z` and choose an unused worktree path.

```powershell
git worktree add --detach ../NeneEngine-package-X.Y.Z vX.Y.Z
Set-Location ../NeneEngine-package-X.Y.Z
git submodule update --init --recursive

cmake --preset x64-release
cmake --build out/build/x64-release --config Release
cpack --config out/build/x64-release/CPackConfig.cmake

.\tools\Test-ReleasePackage.ps1 -PackagePath .\out\package\NeneEngine-X.Y.Z-win64.zip
```

Run the commands in order and stop at the first failure. CPack writes the ZIP to `out/package/`. The [package check](../../tools/Test-ReleasePackage.ps1) extracts the archive to a temporary directory, checks the required files, and runs `NeneEngine.exe --smoke-test` with a fresh profile.

Extract the same ZIP into a fresh folder and run `NeneEngine.exe` from there. Check that the editor opens the demo scene, then exercise the main change in the release. Keep this ZIP for upload after both checks pass. Return to the original checkout to push `main` and the tag.

## Draft the GitHub release

Once the tag is on GitHub, open **Releases → Draft a new release**. Select the existing `vX.Y.Z` tag and enter `NeneEngine vX.Y.Z` as the title. Upload the tested `NeneEngine-X.Y.Z-win64.zip` to **Assets**.

Write the description in English. Open with the main user-visible change, then list the features and fixes that readers will notice. Use the Git history between tags to check what shipped. If users need to take action before upgrading, add **Upgrade notes** after **What's changed**. Include **Known issues** when readers need to know when a problem occurs and how to work around it, if a workaround is available.

In **Download and run**, name the Windows ZIP from **Assets** so readers can find it alongside GitHub's source archives. Take the system requirements from the [packaged README](../../packaging/README.txt.in). End with a link comparing the previous release tag to the current one when a previous tag exists.

In this example for `NeneEngine vX.Y.Z`, `vA.B.C` is the previous release tag.

```markdown
You can now create prefabs from the Hierarchy and attach Lua behaviours
from the Inspector.

## What's changed

- Save an entity and its children as a prefab from the Hierarchy.
- Attach Lua behaviours from the Inspector. The picker hides helper modules.
- Script fields refresh in the Inspector after a Lua reload.
- Prefab names with Unicode characters are preserved on Windows.

## Download and run

Download `NeneEngine-X.Y.Z-win64.zip` from Assets, extract it, and run
`NeneEngine.exe`. Requires 64-bit Windows 10 or 11 and a DirectX 12 GPU.

[Full changelog](https://github.com/NeneTech-Ind/NeneEngine/compare/vA.B.C...vX.Y.Z)
```

## Publish the release

Before choosing **Publish release**, check that the tag, title, and uploaded ZIP show the same version. Review the description and open its links. For the newest stable version, check that GitHub will mark it as **Latest**. [GitHub release settings](https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository)

After publishing, open the release page and download the ZIP to verify that the asset is available. If a shipped build needs a code or package fix, follow the [hotfix process](git-convention.md#hotfixes) and publish a new patch version.
