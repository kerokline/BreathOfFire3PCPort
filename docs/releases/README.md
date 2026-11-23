# Releases: how one is cut

**Status:** IN PROGRESS (2026-10-15; the workflow and the zip script written and the zip built and run locally; no tag pushed yet)

A release is a zip a player unzips and runs: `bof3x-launcher.exe`,
`bof3x.dll`, `bof3x.exe`, the Jukebox's `music-names.tsv`, the save
editor's `save-names.tsv`, `licenses\` (the notices of the work inside the
launcher, which is the drawn one since stage P6 of
[`../launcher-portable.md`](../launcher-portable.md), 2026-11-20: Dear
ImGui's MIT notice from the build's fetched tree, Droid Sans's Apache
License 2.0 and TeX Gyre Pagella's four text files from `third_party/`;
[`../THIRD_PARTY.md`](../THIRD_PARTY.md) sections 6 and 7), and under
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
   `build.yml` does, in both widths, runs `tools/importer.py check`, lays
   out the zips with `tools/make_release.py --version <tag>`, and creates a
   **draft pre-release** with the zips attached and the notes file as its
   text. It stops if the notes file is missing. Two zips: `bof3x-<tag>-win.zip`
   is the 32-bit build, the player's default; `bof3x-<tag>-win-x64.zip` is
   the 64-bit developer build of the same files
   ([`../x64-build-round.md`](../x64-build-round.md) 6). Both need the same
   copy of the game. The Linux tarball is built first and goes on the same
   draft ("The Linux tarball" below).
4. The owner reads the draft on GitHub, downloads the zip, tries it on a
   clean folder, and presses **Publish**. Nothing is public before that
   click. (`workflow_dispatch` with a tag re-runs step 3 for an existing tag,
   e.g. after deleting a bad draft.)

## Signing (2026-10-16)

The three binaries in the zip (`bof3x.exe`, `bof3x.dll`, `bof3x-launcher.exe`)
are Authenticode-signed by the workflow with the owner's **Azure Trusted
Signing** identity (public trust, individual validation). Why: an unsigned
build scored 11/70 on VirusTotal, every hit a machine-learning label keyed on
the executable's packer-like shape (`docs/state3-build.md` 2.1), and Windows
Defender had quarantined the launcher twice; a publisher identity is what
those models score.

How: jsign (`ebourg/jsign`, pinned by version in `release.yml`) signs the
files from the Ubuntu job - Microsoft's action needs a Windows runner - with
SHA-256 and the service's own RFC 3161 timestamp, so the signature outlives
the three-day certificate. Then `make_exe.py --verify-only` checks the signed
executable's layout again and that its certificate table is the file's
trailing blob. The workflow takes:

| Where | Name | What |
|---|---|---|
| repository **secrets** | `AZURE_TENANT_ID`, `AZURE_CLIENT_ID`, `AZURE_CLIENT_SECRET` | an App Registration in the owner's tenant, with a client secret, given the *Trusted Signing Certificate Profile Signer* role on the signing account |
| repository **variables** | `SIGNING_ENDPOINT` | the account's regional endpoint, host only, e.g. `eus.codesigning.azure.net` |
| | `SIGNING_ACCOUNT` | the Trusted Signing account name |
| | `SIGNING_PROFILE` | the certificate profile name (public trust) |

Without the secrets (a fork) the job warns and ships unsigned. There is no
thumbprint or serial to configure: the service issues a fresh certificate
every few days under the same validated identity, and reputation
(SmartScreen, Defender) accrues to the identity. A client secret expires
(two years at most): when signing starts failing with an authentication
error, make a new one and replace the secret.

## Symbols (2026-11-23)

The presets build `RelWithDebInfo`, so a binary as built holds its DWARF:
71 of `bof3x.dll`'s 93 MB and 31 of the launcher's 38 MB (the 32-bit build,
measured 2026-11-23). Until that day the downloads shipped it. A player's copy
needs none of it: the log's `CRASH` lines name an address as
`module+0xOFFSET`, the Windows minidump is `MiniDumpWriteDump`'s, and the
game's own addresses are named from `symbols.toml`. Debug information is read
on the developer's side only.

So `release.yml` runs `tools/strip_symbols.py` in both jobs, after the build
and **before the signing** (a strip after Authenticode breaks the signature):

- each binary **as built** goes into a symbols zip, an asset beside the
  downloads: `bof3x-<tag>-win-symbols.zip`, `-win-x64-symbols.zip` and
  `-linux-x86_64-symbols.zip`;
- the build's own file is then stripped of its `.debug_*` sections
  (`strip --strip-debug`: the symbol table stays).

`make_release.py` refuses a binary that still holds debug sections
(`--keep-debug` takes it anyway, for a download made by hand).

**How a report finds its symbols:** the log's `build:` line, its second line,
names the tag (`v0.1.0-beta.2`, or `latest-<commit>`), and on Linux the ELF's
GNU build id, which the strip keeps. Take that tag's symbols zip; for a
Linux log, `python tools/crash_report.py --log bof3x.log --elf <the zip's
bof3x>` names the engine's addresses by `addr2line`; for a Windows log or
minidump, `--dll <the zip's bof3x.dll>` names the addresses inside
`bof3x.dll` the same way. Both were tried from the downloads of "latest" at
3398f7bb (2026-11-23, a fault made with `BOF3X_CRASH_TEST=0`): the symbols
zip gave the function, the file and the line (`crash_win32.cpp:175`,
`crash_posix.cpp:296`); the player's stripped file gave the function alone.
The report of that fault had no stack line inside the DLL, so the naming of
stack words by `--dll` has not been seen on a real report. A `latest` build's
symbols are replaced with the build, so a report against an older `latest`
needs a rebuild of that commit (same toolchain, same tag: the layout is the
same).

By hand, the same two steps before the zip:

```
python tools/strip_symbols.py --build build --out build/release/bof3x-<tag>-win-symbols.zip
python tools/make_release.py --version <tag>
```

## The latest build (2026-10-15)

Every push to `main` builds the same zips as a release does and publishes
them as the pre-release **"Latest build (main)"**, tag `latest`, always at the
same address:

```
https://github.com/kerokline/BreathOfFire3PCPort/releases/download/latest/bof3x-latest-win.zip
```

That is the 32-bit build, the default. The 64-bit developer build sits
beside it as `bof3x-latest-win-x64.zip`.

`release.yml`, when run for a branch: the `latest` tag is moved to the commit,
the zips (`make_release.py --version latest-<commit>`) replace the ones there,
and the notes name the commit and the time. It is published, not a draft -
it is the download link for current builds (the owner's ask) - and never
marked GitHub's latest release, so a published `v*` release keeps that. No
notes file is needed; a newer push cancels a latest build still running,
but not one that is publishing: job `publish` (the draft, or the files, the
tag and the notes of "latest") is outside the group a push cancels, so
"latest" is never left with one build's tag and another's files
(2026-11-23). A run whose commit is older than what "latest" holds by then
does not publish. A
`v*` tag still makes a draft as in "Cutting one" above. The repository is
public, so the latest build is too.

## Building the zip locally

```
cmake --preset i686 && cmake --build build
python tools/strip_symbols.py --build build --out build/release/bof3x-v0.1.0-alpha.1-win-symbols.zip
python tools/make_release.py --version v0.1.0-alpha.1
```

writes `build/release/bof3x-v0.1.0-alpha.1-win.zip`. For the 64-bit developer
build, `cmake --preset x86_64 && cmake --build build-x86_64` and then
`make_release.py --version v0.1.0-alpha.1 --build build-x86_64` writes
`bof3x-v0.1.0-alpha.1-win-x64.zip`: the script reads the width from the
build's `bof3x.exe` and adds one paragraph to that zip's `README.txt`.
Downloads (python.org's embeddable package and three PyPI wheels) are pinned
by version and sha256 in the script and kept in `build/release-downloads/`,
so a second run is offline. To try the bundle without the system Python:

```
<unzipped>\runtime\python\python.exe <unzipped>\runtime\tools\importer.py identify --json "<a .cue>"
```

## The Linux tarball (2026-11-22, the Linux round's group D2)

```
cmake --preset linux-x86_64 -DBOF3X_VERSION_TAG=<tag> && cmake --build build-linux-x86_64     # on Linux
python3 tools/strip_symbols.py --linux --build build-linux-x86_64 --out build/release/bof3x-<tag>-linux-x86_64-symbols.zip
python3 tools/make_release.py --linux --version <tag>
```

writes `build/release/bof3x-<tag>-linux-x86_64.tar.gz`: one folder
of that name holding `bof3x`, `bof3x-launcher`, `libSDL3.so.0`,
`libdxvk_d3d11.so.0`, `libdxvk_dxgi.so.0`, the two names files, `README.txt`
(the Linux one), `LICENSE`, `THIRD_PARTY.md`, `licenses/` (the zip's three
and the notices of SDL3, DXVK and what DXVK carries,
[`../THIRD_PARTY.md`](../THIRD_PARTY.md) section 8) and `runtime/` (the
zip's reached tools and data, and `tools/requirements.txt`). **No Python is
in it** ([`../linux-round.md`](../linux-round.md) section 6, call 3): the
player makes a venv in `runtime/python`, or uses `python3` from `PATH`, and
the launcher takes the first it finds. Nothing is downloaded. The game-data
scan is the zip's. The archive sets the modes (0755 for the two programs and
the three libraries) whatever the host's filesystem keeps, and the script
reads each binary's `RUNPATH` and `NEEDED` names from the file and stops
unless the programs find the three libraries beside them (`$ORIGIN`) and
need nothing else but glibc and the C++ runtime. It prints the newest glibc
and libstdc++ versions the binaries need and writes them into `README.txt`:
**that is the oldest system the tarball runs on**, and it follows the build
machine (a build on Ubuntu 26.04 needs glibc 2.43, one on 24.04, as the
release job's, 2.38). The tarball is signed by a build attestation, not by
Authenticode (below).

**The tarball's signature (2026-11-22, the owner; not yet run on GitHub).**
Authenticode signs Windows files, and Linux has no check like SmartScreen
that a signature would satisfy. What a Linux download can have is proof of
where it was built. Job `linux` of `release.yml` runs `actions/attest` on the
tarball: a SLSA build provenance attestation over the file's sha256, signed
with a Sigstore certificate issued to that workflow run, and stored by GitHub
for the repository. It uses no secret and no key of the owner's. To check a
download (GitHub's CLI):

```
gh attestation verify bof3x-<tag>-linux-x86_64.tar.gz --repo kerokline/BreathOfFire3PCPort
```

It passes only for a file this repository's workflow built, byte for byte.
The Windows zips are not attested; their binaries carry Authenticode. If the
attestation step fails, the Linux job fails and there is no release.

**In `release.yml`** (since 2026-11-22, not yet run on GitHub): job `linux`
builds on `ubuntu-24.04` with `.github/scripts/linux-build.sh` (`deps`,
`configure`, `build`, `check`; the same steps as `build.yml`'s Linux job,
but with no DXVK cache, so DXVK-native is built from its pinned commit and
its notices come from that tree) and runs the line above with the tag. The
`release` job needs `linux`: it starts when the tarball is made, fetches it,
and puts `bof3x-<tag>-linux-x86_64.tar.gz` on the release with the zips: the
draft for a tag, or "latest" for a push to `main`. If the Linux build fails,
there is no release, as when a Windows build fails (the owner, 2026-11-22):
no draft is made, and "latest" keeps the files of the last good run. The
zips are built after the Linux build, not beside it, so a release run takes
longer. A build on `ubuntu-24.04` needs glibc 2.38
and `GLIBCXX_3.4.31` or newer (measured in a 24.04 root, `../linux-round.md`
"D2 as run"). The notes of "latest" name the tarball.

**For the release notes** of the first release that carries the tarball, a
paragraph to adapt:

> **Linux (new, not yet played).** `bof3x-<tag>-linux-x86_64.tar.gz` is a
> native build for 64-bit Linux. It needs a Vulkan 1.3 driver, X11 or
> Wayland, and Python 3.11 or newer with the packages in
> `runtime/tools/requirements.txt`; its `README.txt` says how to make the
> Python environment and gives the lowest glibc it runs on. It ran on a
> computer with no screen and a software graphics driver only: the setup
> from the PC game folder, the install check and the game's self-tests
> passed. Nobody has played it on a graphics card, heard its sound or used
> a pad with it yet. Reports are welcome. The Windows zips under Proton or
> Wine stay the other way to play on Linux.

## Versions

`v<major>.<minor>.<patch>-alpha.<n>` while the game has been watched more
than played; the pre-release flag stays on until the owner says otherwise.
The bundled Python's version and the wheels' versions are pinned in
`tools/make_release.py` (`PYTHON_VERSION`, `WHEELS`) - change a pin and its
sha256 together, each measured and compared with the publisher's own digest.
