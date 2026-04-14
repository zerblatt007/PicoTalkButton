---
when: 2026-04-14T14:06:55Z
why: provide a frictionless one-liner install path for new users
what: add quick-install script, bump-version utility, and README quick-install section
model: github-copilot/claude-sonnet-4.6
tags: [installer, scripts, docs, tooling]
---

Added `scripts/install.sh` — a self-contained bash installer that fetches the latest `.deb`
from the GitHub repository via the Contents API, extracts it with `dpkg -x`, runs the embedded
`install.sh`, and finishes with a 4-point health check. Also added `scripts/bump-version.sh` for
repeatable semantic version bumps to `host/ptt-listen/DEBIAN/control`. README updated with a
prominent Quick Install section.
