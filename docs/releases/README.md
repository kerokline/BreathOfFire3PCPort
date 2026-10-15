# Releases: how one is cut

**Status:** IN PROGRESS (2026-10-15; the workflow and the zip script written and the zip built and run locally; no tag pushed yet)

A release is a zip a player unzips and runs: `bof3x-launcher.exe`,
`bof3x.dll`, `bof3x.exe`, the Jukebox's `music-names.tsv`, and under
`runtime\` a bundled Python with the importer, so the player installs
nothing but the game they own. **No game data is in it** (CLAUDE.md rule 1):
the player adds their own PC folder or disc images on the launcher's Sources
page ([`../install-sources.md`](../install-sources.md)), and
`tools/make_release.py` refuses to write a zip holding any file of a
game-data kind or any file whose hash `fixtures.toml` catalogues.

## Cutting one

1. Write the notes: `docs/releases/<tag>.md` (copy the last one's shape:
   what works, what has been watched but not played, how to add a source,
   the ffmpeg link, known issues). Read [`../STATUS.md`](../STATUS.md) and
   the README's status line first; a release note never claims more than
   they do. Commit it on `main` (signed off, rule 7).
2. Tag that commit and push the tag:

   ```
   git tag -a v0.1.0-alpha.1 -m "v0.1.0-alpha.1"
   git push origin v0.1.0-alpha.1
   ```

3. `.github/workflows/release.yml` runs: it builds the engine exactly as
   `build.yml` does, runs `tools/importer.py check`, lays out the zip with
   `tools/make_release.py --version <tag>`, and creates a **draft
   pre-release** with the zip attached and the notes file as its text. It
   stops if the notes file is missing.
4. The owner reads the draft on GitHub, downloads the zip, tries it on a
   clean folder, and presses **Publish**. Nothing is public before that
   click. (`workflow_dispatch` with a tag re-runs step 3 for an existing tag,
   e.g. after deleting a bad draft.)

## The latest build (2026-10-15)

Every push to `main` builds the same zip as a release does and publishes it as
the pre-release **"Latest build (main)"**, tag `latest`, always at the same
address:

```
https://github.com/kerokline/BreathOfFire3PCPort/releases/download/latest/bof3x-latest-win.zip
```

`release.yml`, when run for a branch: the `latest` tag is moved to the commit,
the zip (`make_release.py --version latest-<commit>`) replaces the one there,
and the notes name the commit and the time. It is published, not a draft -
it is the download link for current builds (the owner's ask) - and never
marked GitHub's latest release, so a published `v*` release keeps that. No
notes file is needed; a newer push cancels a latest build still running. A
`v*` tag still makes a draft as in "Cutting one" above. The repository is
public, so the latest build is too.

## Building the zip locally

```
cmake --preset i686 && cmake --build build
python tools/make_release.py --version v0.1.0-alpha.1
```

writes `build/release/bof3x-v0.1.0-alpha.1-win.zip`. Downloads (python.org's
embeddable package and three PyPI wheels) are pinned by version and sha256 in
the script and kept in `build/release-downloads/`, so a second run is
offline. To try the bundle without the system Python:

```
<unzipped>\runtime\python\python.exe <unzipped>\runtime\tools\importer.py identify --json "<a .cue>"
```

## Versions

`v<major>.<minor>.<patch>-alpha.<n>` while the game has been watched more
than played; the pre-release flag stays on until the owner says otherwise.
The bundled Python's version and the wheels' versions are pinned in
`tools/make_release.py` (`PYTHON_VERSION`, `WHEELS`) - change a pin and its
sha256 together, each measured and compared with the publisher's own digest.
