---
when: 2026-04-14T15:50:21Z
why: scripts/install.sh failed with curl error 23 and the embedded install.sh failed with "No module named pip" on Ubuntu 24.04
what: fix curl broken-pipe bug in scripts/install.sh and add pip bootstrap to host installer
model: github-copilot/claude-sonnet-4.6
tags: [bugfix, installer, pip, curl, debian]
---

Fixed two bugs blocking installation on Ubuntu 24.04. In `scripts/install.sh`, replaced `python3 - <<'EOF'` heredoc (which stole stdin from the curl pipe, triggering curl error 23) with `python3 -c '...'` so stdin remains connected to the pipe. In `host/ptt-listen/ptt-listen/install.sh`, added a `ensurepip --upgrade` bootstrap step so pip is available in the venv on systems that ship `python3-venv` without pip. Bumped package to version 1.09 and rebuilt `host/ptt-listen-1.09.deb`.
