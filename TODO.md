# TODO

tidesway is a prototype: sway 1.12 with what tide needs from a compositor, to measure
against tidewm (tidewm's `SWAY.md` and its `experiments/sway/REPORT.md`). Items go in
this order, one PR each; each makes more of tidewm's suite pass.

## Plan

- ~~**Layouts**~~. Done: tide's `tile`, `threecol`, `twocol` and `monocle`, with
  `mfact`, `nmaster`, `single_width`, `zoom` and `movestack`, tidewm's layout
  lines and its `layout` IPC (`sway/tide/`).
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
- **A new workspace's layout** (2026-11-08): tide's, as in tidewm: `tile`, or
  `threecol` on an ultrawide output, or `default_layout`'s. A workspace in sway's
  layouts is reached only by one of sway's layout or split commands.
- **sway's layouts on a tide workspace** (2026-11-08): `layout splith|splitv|
  stacking|tabbed|default`, `layout toggle split|all|...` and `split*` put the
  workspace back in sway's layouts, then do what they do in sway, rather than
  failing; a `layout` naming one of tide's puts it back in tide's. `mfact`,
  `nmaster`, `single_width` and `movestack` fail on a workspace in sway's
  layouts, as tidewm's fail in its floating layout.
- **No floating layout** (2026-11-08): sway has none, so `layout floating`
  fails, and `layout toggle` on a workspace that's had only one layout does
  nothing, where tidewm's goes to floating.
- **A flat tree in a tide layout** (2026-11-08): a workspace in a tide layout
  keeps its windows direct children of the workspace. Setting a tide layout
  drops sway's split containers (and makes a tabbed or stacked workspace a
  row); a new window, or one moved in or tiled again, goes in unwrapped, even
  with `workspace_layout tabbed|stacking`; sway's split commands go back to
  sway's layouts. `focus` and `move` by direction go by tidewm's rule for
  a tile, so they don't nest either; anything else that nests is laid out in
  tree order, and `zoom`, `movestack` and the next new window flatten it.
