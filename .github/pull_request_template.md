<!-- Thanks for contributing. CONTRIBUTING.md has the full rules; this is the
     short version of what a reviewer will check for. -->

## What this changes

<!-- One or two sentences. -->

## Checklist

- [ ] Every commit is signed off (`git commit -s`) — see [CONTRIBUTING.md](../CONTRIBUTING.md)
- [ ] **No game data** in the diff, and none pasted into the description, commit
      messages or linked issues — not just files (`CLAUDE.md` rule 1)
- [ ] Behavioural changes have a [`docs/DIVERGENCE.md`](../docs/DIVERGENCE.md)
      entry — what the original did, what this does, why. *A port bug fix still
      counts.*
- [ ] Claims about the binary cite the measurement that produced them
- [ ] New or changed names in [`symbols.toml`](../symbols.toml) carry an honest
      tier — `hypothesis` unless something on the PC side confirms it
- [ ] No third-party source vendored (see [`docs/LICENSING.md`](../docs/LICENSING.md) §4)
- [ ] No stubs: replaced functions are fully implemented or abort loudly

## Evidence

<!-- For a finding: the command or script, and its output. For a reimplemented
     function: how equivalence was established. -->
