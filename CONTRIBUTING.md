# Contributing

## Development workflow

1. Create a focused branch from `main`.
2. Add or update tests with every behavioral change.
3. Build with warnings treated as errors.
4. Keep protocol changes traceable to an SSC-32 or SSC-32U manual, hardware
   observation, or reproducible test.
5. Open a pull request describing compatibility and safety implications.

```sh
cmake -S . -B build -DSSC32_WARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Run `clang-format` on changed C++ files before submitting. Public API changes
must update the README and remain source-compatible within a released major
version unless the change fixes unsafe or incorrect behavior.

## Commit messages

Use an imperative subject that states the outcome, for example:

```text
fix: handle partial serial writes
```

## Hardware reports

Include controller model, firmware version, serial adapter, baud rate, operating
system, command bytes, response bytes, and the smallest safe reproduction.
Remove unrelated device paths, usernames, and host details.
