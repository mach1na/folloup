# Versioning

Followup follows [Semantic Versioning](https://semver.org/) (`MAJOR.MINOR.PATCH`).

## Source of truth

`version.txt` at the repo root holds the current version. ESP-IDF's build
system reads this file automatically (no CMake wiring needed) and bakes it
into the firmware's app description, which is logged once at boot in
`main/app_shell.cpp` (`Run()`) via `esp_app_get_description()->version`. This
is also what `esptool.py`/`idf.py partition-table`/OTA tooling reports for the
running image.

The `webserver/` setup portal is embedded into and shipped with firmware; it
does not carry an independent version and moves with the firmware version.

## When to bump

The product is pre-1.0, so:

- **MINOR** — a new or changed user-facing feature/behavior (new screen,
  changed gesture, new setting, etc).
- **PATCH** — a bug fix, or an internal/refactor change with no user-visible
  behavior change.
- **No bump** — docs-only changes, comments, CI/tooling that doesn't affect
  the built firmware.

Once the product reaches a stable 1.0, switch to strict SemVer: breaking
changes (a stored-data format change that isn't migrated, a removed setting,
etc.) bump MAJOR.

## Workflow

Every feature or bugfix branch that changes firmware or webapp behavior:

1. Bumps `version.txt` as part of that change, in the same branch/PR as the
   change itself (not as a separate follow-up).
2. Adds an entry to `CHANGELOG.md` under `## [Unreleased]`.

When cutting a release (merging to `main` and flashing/distributing a build),
move the `[Unreleased]` entries under a new `## [X.Y.Z] - YYYY-MM-DD` heading
in `CHANGELOG.md`, and tag that commit on `main` with an annotated tag
`vX.Y.Z`:

```bash
git tag -a vX.Y.Z -m "vX.Y.Z"
git push origin vX.Y.Z
```

The pre-SemVer tag `v0.01` predates this scheme and can be ignored.
