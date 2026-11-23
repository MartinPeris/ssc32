# Contributing

## Development workflow

1. Create a focused branch from `main`.
2. Add or update tests with every behavioral change.
3. Install the versioned pre-commit hook with `./scripts/install_git_hooks.sh`.
4. Keep protocol changes traceable to an SSC-32 or SSC-32U manual, hardware
   observation, or reproducible test.
5. Open a pull request describing compatibility and safety implications.

The hook runs the complete Dockerized quality gate against the exact staged Git
tree. Run the same gate explicitly against working-tree content with
`./scripts/quality_gate.sh`. It enforces formatting, shell lint, Clang static
analysis, GCC and Clang warnings-as-errors, tests, installation, ASan/UBSan, and
an exact production-coverage ratchet. The committed baseline records covered
and total lines and branches, and every commit must equal or improve both
ratios. After a stable improvement, adopt the measured result explicitly with
`./scripts/quality_gate.sh --update-coverage-baseline`; the command refuses to
lower either metric. Public API changes must update the README and remain
source-compatible within a released major version unless the change fixes
unsafe or incorrect behavior.

## Commit messages

Use an imperative subject that states the outcome, for example:

```text
fix: handle partial serial writes
```

## Hardware reports

Include controller model, firmware version, serial adapter, baud rate, operating
system, command bytes, response bytes, and the smallest safe reproduction.
Remove unrelated device paths, usernames, and host details.
