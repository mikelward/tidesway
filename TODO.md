# TODO

tidesway is a prototype: sway 1.12 with what tide needs from a compositor, to measure
against tidewm (tidewm's `SWAY.md` and its `experiments/sway/REPORT.md`). Items go in
this order, one PR each; each makes more of tidewm's suite pass.

## Plan

- **Layouts**. tide's `tile`, `threecol`, `twocol` and `monocle`, with `mfact`,
  `nmaster` and `single_width`, placed before a new window is first shown.
- **Focus guard**. Hooks in `view_map()` and `should_focus()` so a held window never
  takes focus, the lone window on an empty workspace included.
- **Dimming**. `dim_inactive`, darkening unfocused windows rather than making them
  see-through.
- **Workspaces spanning every monitor**. Grouped workspaces: each output's
  workspace of one name switches together.
- **Measure**. tidewm's suite and the experiment's benchmarks, against tidewm.
- **Fail on regressions**. Make the `test` job fail when a test that passed in the
  baseline stops passing.

## Baseline

tidewm's suite on sway 1.12 as forked, before any tide patch (2026-11-08, tidewm main
at `1f47ac2`): **117 of 664 pass**, 421 fail, 126 error, 0 time out. A test with
subtests counts once per subtest. Nothing passes yet in layouts, split, minimize,
maximize, layers, floating, dimming, capture, seats or urgency; focus passes 5 of 95.
Each patch's PR says how far it moves this.

## Decisions needing review

- **Release tags** (2026-11-08): `v<sway version>-tide.N`, sway's version padded to
  three parts, N the commits since `RELEASE_BASE` (sway's release commit, in
  `meson.build`): `v1.12.0-tide.1` is the first. sway tags `1.12` and `1.12.1`, with
  no `v` and no `-tide`, so they can't collide, and the tag says which sway it's
  built on. In semver it's a pre-release, sorting below `1.12.0`; `v1.12.N` would
  sort right, but `v1.12.1` would be ours and `1.12.1` sway's.
