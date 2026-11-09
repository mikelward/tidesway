---
trigger: always_on
alwaysApply: true
last_modified: 2026-11-09
---

# tidesway

tide's fork of sway 1.12, as tidewm is tide's fork of dwl, in C, built with meson
against wlroots 0.20. It is to carry tide's layouts, focus guard, `dim_inactive` and
workspaces that span every monitor (as grouped workspaces), then be measured against
tidewm (tidewm's `SWAY.md`). `README.md` says what it adds to sway, `TODO.md` what's
next; sway's man pages (`sway/*.scd`) stay the reference for the rest.

## Fleet conventions

Shared by every mikelward repo and the user-level agent config; canonical in mikelward/conf
`agents/AGENTS.md`. A repo's Project section overrides it.

### This file

- **Session start**. At the start of every session, print the full absolute path of the
  `AGENTS.md` you loaded and its `last_modified` date.
- **Canonical**. AGENTS.md is canonical; `CLAUDE.md` and `GEMINI.md` point to it. Keep it
  brief, generic to all agents (noting per-agent exceptions), and under 22,000 bytes.
  Bump `last_modified` whenever you edit this file.

### Talking to the user

- **Questions**. One at a time; wait for the answer.
- **Messages**. Send one only for a question, a blocker, something to act on, or a change
  of plan. No progress reports or narrating events.
- **Claude Code**. It reliably shows only a turn's last message; put everything there.
- **Replies**. Short, key point first, ending with open PR links and any pending
  question.
- **Network access**. If you want access to a host and you can't access it or a mirror,
  ask the user to allowlist the domain(s) along with the purpose.

### Git

- **Branches**. `<agent>/<topic>` off a fresh `origin/main` (fetch by refspec). Never
  commit to `main`.
- **Your branches**. Push, force-push with lease, and delete them freely. Ask before
  touching others', `main`, settings, secrets or paid services.
- **Commits**. One per logical change; fold in review fixes.
- **Subjects**. Changelog lines: imperative, the user-visible change, ≤ 70 chars. PR
  titles match.
- **Prefixes**. `docs:`, `test:`, `build:`, `refactor:` mark what users won't notice. No
  `feat:` or `fix:`.
- **Model names**. Never in anything pushed.
- **After a merge**. Branch afresh.
- **Shallow clones**. Unshallow before trusting history or calculating a version code.
- **Language**. US English everywhere.

### Pull requests

- **Opening**. Open the PR unasked; update its title and body before each push.
- **Descriptions**. Scope, rationale and test plan, plus the change's privacy,
  reliability and cost impact (say "none" where there is none).
- **TODO.md**. Tick an item off in the PR that implements it, not a follow-up. Don't
  spend a CI run on trivia.
- **Watching**. PR events plus one scheduled check (events miss CI success and Codex's
  👍).
- **Auto-merge**. Enable it on PRs that target `main`; stack dependent ones.
- **Base moved**. After a merge, tell open PRs on that base whose body ends with a session
  URL.
- **Merge or close**. Answer remaining threads and stop watching.

### Reviews

- **Codex**. It reviews automatically. Fix, decline with a reason, or defer to `TODO.md`
  every comment; reply citing the commit, then resolve.
- **Merit**. Codex over-reads rules, privacy especially: quote and decline rather than
  cripple the code. Design changes are the user's call.
- **Verdict**. 👍 is clean, 👀 is reviewing. Only the code review gates merging: its
  findings (review comments, PR comments, reviews) block it. Security reviews often
  hang; don't wait on them.
- **Hung review**. `@codex review` after five quiet minutes, again an hour later, then
  tell the user.
- **After a 👍**. Ask before unneeded changes.
- **Reports**. Give each review's findings in your next message.

### Autonomy

- **Fix proactively**. Lint, test, presubmit, CI and review failures, unasked.
- **Proactive refactors**. Repeated bugs in one area, duplicated code, or a change with
  twins elsewhere: suggest the redesigned or shared version.
- **Drive to merge**. Once the details are agreed, see the change through unattended:
  implement it, open the PR, keep it mergeable (fix CI, answer reviews, rebase when it
  conflicts or falls behind), and merge once CI is green and Codex 👍s, or let
  auto-merge.
- **Drive**. Take `TODO.md` items one at a time and drive each to merge, until done or
  stopped.
- **Autopilot**. Drive without asking, logging guesses in `TODO.md` under *Decisions
  needing review*. Still ask before anything irreversible, outside this repo, or private.
- **Red baseline**. Fix existing failures first, in their own commit.
- **Lasting fixes**. Fix the cause in a repo. Don't propose a step that lasts only this
  session (stopping a service, a setting in this container) or a hand edit to a dotfile:
  neither persists.

### Quality

- **Tests**. Every behavior change gets one; a bug fix's fails first. Assert behavior,
  not structure. No sleeps or retries; never disable a failing check.
- **Docs**. Keep `SPEC.md` and `README.md` current.
- **CI**. Wait for it before claiming success.
- **Errors**. Never swallow one silently: every catch logs it with context (no user
  data), cleans up what the `try` acquired, and handles the case explicitly. To ignore
  one on purpose, say why in a comment and still log it at debug level.
- **Cost**. State the cost and reliability of new dependencies or services up front.
- **Privacy**. No user data (personal paths, hostnames, tokens, keys, private remotes)
  leaves this machine; use placeholders.
- **Portability**. Flag anything that won't run on Linux and macOS.
- **UI text**. Words a person would say, as few as do the job. Say what a setting does;
  leave out what works by default, what the screen already shows, and how it works.

## Project

- **Build**. `meson setup build && ninja -C build` before every commit; sway's
  `werror` stays on. CI also builds with `--fatal-meson-warnings -Dauto_features=enabled
  -Dsd-bus-provider=libsystemd`, as sway's own `.builds/` do.
- **Tests**. tidewm's suite is the spec. `tests/tidewm-harness/suite TIDEWM OUT` runs
  a mikelward/tidewm checkout's `tests/` against `build/sway/sway`, with
  `tests/tidewm-harness/run` standing in for tidewm (its `-c` and `-s`). Build its
  clients first (`make tests/client ...`, as `ci.yml` does). The harness's own tests
  run with `python3 -m unittest discover -s tests/tidewm-harness`. tidesway's own
  tests, for what tidewm's suite can't express (sway's split tree, say), are
  `tests/tidesway/test_*.py`, on tidewm's `tests/harness.py`: `TIDEWM_CHECKOUT=DIR
  python3 -m unittest discover -s tests/tidesway`. They fail CI's `test` job.
- **CI**. Builds on Arch (`build`) and Debian testing (`build-debian`), the two places
  a packaged wlroots 0.20 exists; tidewm's suite runs on Debian testing (`test`).
  `test` fails if a test in `tests/tidewm-harness/passing.txt` doesn't pass
  (`tests/tidewm-harness/gate`), except one in `flaky.txt`, which says why it's
  flaky; a PR that makes a test pass adds it to `passing.txt`. Its summary lists
  newly passing tests.
- **Upstream**. tide's code lives in its own files, `sway/tide/` and
  `include/sway/tide/`, with small hooks in sway's files, so rebasing onto each sway
  release stays cheap. One commit per patch. Rebase onto upstream; never merge it.
- **Subjects**. Write them for someone using tidesway.
- **Releases**. Tags `v<sway version>-tide.N` on `main` (`v1.12.0-tide.N`), N being
  the commits since `RELEASE_BASE`, sway's release commit; both are in `meson.build`,
  and `.github/release-tag` prints the tag. The `release` workflow tags each push to
  main. A pushed tag is never moved. A rebase onto a new sway release sets
  `RELEASE_BASE` to its commit.
