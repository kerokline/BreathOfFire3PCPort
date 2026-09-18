# Contributing

Three rules govern this repository, and a first-time contributor should meet all
three before writing anything. They are cheap to follow and very expensive to
retrofit.

1. **Never commit game data.** `BOF3.exe`, `DAT/`, `BGM/`, `SND/`, disc images,
   the AVIs, and anything extracted or dumped out of them. `.gitignore` covers
   the files; it does not cover a paste into a doc, an issue, a PR description or
   a commit message. This is not only hygiene — the engine/data split is what
   keeps a commercial handoff possible ([`docs/LICENSING.md`](docs/LICENSING.md)
   §3), so it is not relaxable for convenience.
2. **Unledgered divergence is a bug.** Every intentional change to game
   behaviour gets an entry in [`docs/DIVERGENCE.md`](docs/DIVERGENCE.md): what
   the original did, what this does, why. A PR that changes behaviour without a
   ledger entry is incomplete, and will be asked for one.
3. **Evidence over assertion.** A claim about the binary cites the measurement
   that produced it — the command, the script, the address, the trace. Not "the
   game dispatches through function-pointer tables" but the sweep that counted
   them. See [`docs/README.md`](docs/README.md) for the evidence rule and the
   `evidence` / `hypothesis` / `unnamed` tiers.

[`CLAUDE.md`](CLAUDE.md) carries the full rule set, including why addresses are
load-bearing constants and why `bof3ext` is a peer project rather than a base.
Read it.

---

## Sign your commits off (DCO)

Every commit must carry a `Signed-off-by:` line. Git adds it for you:

```bash
git commit -s -m "your message"
```

which appends, using your `user.name` and `user.email`:

```
Signed-off-by: Your Name <you@example.com>
```

Use a real name and a working address. To fix a branch that is missing sign-offs,
rebase with `git rebase --signoff <base>` and force-push your own branch.

A CI check enforces this on every pull request.

### Signing off automatically

Git has no `commit.signoff` setting — deliberately, since the sign-off is meant
to be an affirmative act. (`format.signOff` exists but only affects
`git format-patch`, and `commit.gpgSign` is cryptographic signing, a different
thing.) The usual answer is a hook, and this repository ships one:

```bash
git config core.hooksPath .githooks
```

`.githooks/prepare-commit-msg` then adds the trailer to every commit, using
`git interpret-trailers` so it lands in the trailer block alongside
`Co-Authored-By:` rather than being appended blindly. It skips merge commits,
does not duplicate a trailer you added with `-s`, and fails loudly if
`user.name` / `user.email` are unset.

Two things to know before enabling it. It **replaces `.git/hooks` entirely** for
this repository, so merge in any hooks you already keep there rather than
switching blind. And it makes the certification automatic — which is fine for
commits whose provenance you know, and is worth a thought in a repository where
some commits are tool-assisted. The sign-off is still your assertion that you
reviewed and understood the change; a hook adds the line, not the review.

### What you are certifying

The sign-off is your agreement to the
[Developer Certificate of Origin 1.1](https://developercertificate.org/),
reproduced verbatim:

```
Developer Certificate of Origin
Version 1.1

Copyright (C) 2004, 2006 The Linux Foundation and its contributors.

Everyone is permitted to copy and distribute verbatim copies of this
license document, but changing it is not allowed.


Developer's Certificate of Origin 1.1

By making a contribution to this project, I certify that:

(a) The contribution was created in whole or in part by me and I
    have the right to submit it under the open source license
    indicated in the file; or

(b) The contribution is based upon previous work that, to the best
    of my knowledge, is covered under an appropriate open source
    license and I have the right under that license to submit that
    work with modifications, whether created in whole or in part
    by me, under the same license (unless I am permitted to submit
    under a different license), as indicated in the file; or

(c) The contribution was provided directly to me by some other
    person who certified (a), (b) or (c) and I have not modified
    it.

(d) I understand and agree that this project and the contribution
    are public and that a record of the contribution (including all
    personal information I submit with it, including my sign-off) is
    maintained indefinitely and may be redistributed consistent with
    this project or the open source license(s) involved.
```

The DCO text is reproduced unchanged on purpose. Its value is that it is the
*same* document thousands of projects use, so a contributor does not have to read
a bespoke variant. Note that it says "open source license", while this project is
licensed under [PolyForm Noncommercial](LICENSE), which is source-available
rather than OSI open source. Projects outside OSI licensing commonly use the DCO
anyway, since what it actually certifies is provenance; the wording mismatch is
noted here rather than papered over, and is one of the things a legal review
should confirm ([`docs/LICENSING.md`](docs/LICENSING.md) §5).

### Third-party code

Don't vendor any. Read other projects freely, cite them, and reimplement from a
description — techniques are not copyrightable. But **copying source in would end
the commercial path**, because the project cannot grant terms it does not hold,
and for GPL code there is no workaround at all
([`docs/LICENSING.md`](docs/LICENSING.md) §4). If you believe something genuinely
must be vendored, open an issue first; it needs a licence decision, not a PR.

### AI-assisted contributions

Fine, and used in this repository — commits carry a `Co-Authored-By:` trailer
where that applies. The DCO sign-off is yours regardless: you are the person
certifying you have the right to submit the contribution, and you are responsible
for it having been reviewed and understood. Tool-assisted does not mean
unreviewed.

---

## Licensing of your contribution — read this before your first PR

Contributions are made under [PolyForm Noncommercial 1.0.0](LICENSE), the
repository's licence.

**This project intends to remain relicensable.** The stated goal
([`docs/LICENSING.md`](docs/LICENSING.md)) is to keep the work open and viewable
while being able to hand a finished product to a distributor who would settle IP
rights with Capcom. That requires being able to grant terms other than PolyForm
Noncommercial — including, potentially, proprietary terms.

A sign-off alone does **not** grant that right. A Contributor Licence Agreement
is what does, and **this project does not have one yet.** Until it does:

- Contributions are accepted under the repository licence as it stands.
- The intent above is disclosed up front so that nobody is surprised by it
  later. Contribute knowing that you may be asked to agree to a CLA, or to
  confirm a relicensing grant, before any commercial handoff.
- If you are not comfortable with that, say so on the PR. It is a reasonable
  position and much better raised early than discovered at the worst moment.

This section will be replaced when a real CLA exists. It is deliberately not
written as legal text, because inventing legal text is worse than admitting the
gap ([`docs/LICENSING.md`](docs/LICENSING.md) §7).

---

## Practical notes

- **Findings go in `docs/`.** Naming, status headers and the evidence rule are
  in [`docs/README.md`](docs/README.md). `SCREAMING_CASE.md` for durable
  subsystem documents, `kebab-case.md` for a single investigation.
- **Names go in [`symbols.toml`](symbols.toml)**, tiered. A name transferred
  from the archival sibling, or proposed by a matcher, is a `hypothesis` until
  something on the PC side confirms it. It never inherits `confirmed` from the
  PSX side.
- **No stubs.** A reimplemented function is fully implemented or it aborts
  loudly. `return 0;`, `// TODO` and `// for now` inside a function that has
  replaced original code are silent behavioural forks.
- **Tooling output belongs in `analysis/`**, which is gitignored because it is
  derived from copyrighted game code.
- Environment quirks specific to this machine — the Anaconda `python`, the
  MSYS2 path, where Ghidra lives — are in [`CLAUDE.md`](CLAUDE.md).
