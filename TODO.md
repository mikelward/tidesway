# TODO

tidesway is a prototype: sway 1.12 with what tide needs from a compositor, to measure
against tidewm (tidewm's `SWAY.md` and its `experiments/sway/REPORT.md`). Items go in
this order, one PR each; each makes more of tidewm's suite pass.

## Plan

- ~~**Layouts**~~. Done: tide's `tile`, `threecol`, `twocol` and `monocle`, with
  `mfact`, `nmaster`, `single_width`, `zoom` and `movestack`, tidewm's layout
  lines and its `layout` IPC (`sway/tide/`).
- ~~**Focus guard**~~. Done: tidewm's grants, held windows and status popups,
  a canceled grant's placement, `grant`, `grant extend`, `grant --or-recent`,
  `focus recent` and `focus_guard`, with GET_SEATS' `last_input` and a
  window's `workspace` and `output` (`sway/tide/focus.c`).
- ~~**Dimming**~~. Done: tidewm's `dim_inactive` and `dim`, a black layer
  over every window but the focused one, which clicks go through, with a
  window's `dim` and `dimmed` in IPC (`sway/tide/dim.c`).
- ~~**Workspaces spanning every monitor**~~. Done: each workspace a group of
  sway's per-output workspaces of one name, its parts, on every output and
  switching together, with `persist` and `unpersist`, tidewm's workspace IPC
  and events, one ext-workspace group, and a disabled output's windows
  joining the parts left (`sway/tide/workspace.c`).
- ~~**Measure**~~. Done: tidewm's suite and the experiment's benchmarks, against
  tidewm, in `MEASUREMENTS.md`: tidewm's latency, no intermediate states and no
  wrong frames on close; 348 of 665 of the suite (`measure.yml`, `bench/`).
- ~~**Fail on regressions**~~. Done: the `test` job fails when a test in
  `tests/tidewm-harness/passing.txt` stops passing, unless `flaky.txt` lists it
  (`tests/tidewm-harness/gate`).
- **Track sway**. A weekly check of swaywm/sway: new commits on the current release
  branch (`v1.12`) become a PR cherry-picking them; a new release tag becomes an issue,
  since moving to it is a rebase of `main` (`RELEASE_BASE`). Today `v1.12` has nothing
  past `1.12`. Why this can't sync as tidewm does: tide's `docs/dwl-vs-sway.md`.

## Baseline

`tests/tidewm-harness/passing.txt` is the baseline CI holds tidesway to: the 341
tests that pass on main (2026-11-09, `9612edaf`, tidewm main at `1f47ac2`), less
the 9 then in `flaky.txt`, and since then `GrantTest`'s grant on a workspace, once
its crash was fixed, and each test a later PR made pass. A PR that makes a test pass for good adds it there. CI runs
tidewm's suite at the commit `ci.yml`'s `TIDEWM_REF` pins, `1f47ac2`, so tidewm's
changes reach it only through a PR that moves the pin and updates the lists.

Before any tide patch, tidewm's suite on sway 1.12 as forked (2026-11-08, tidewm main
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
- **Status popups float** (2026-11-08): a window with neither an app ID nor a
  title floats, guard or no guard, as in tidewm, so `zoom` and the tiles
  never meet it; `focus next|prev` and the arrow keys pass it by while the
  guard holds it.
- **A grant's workspace and `assign`** (2026-11-08): an `assign` rule's
  workspace wins over a grant's `--workspace`, as it does over a canceled
  grant's place. In tidewm a grant's `--workspace` wins over the rules; here
  both places are chosen where sway chooses a new window's workspace.
- **`focus next` with nothing focused** (2026-11-08): goes to the workspace's
  most recent window, or one the guard holds, in sway's layouts as in tide's,
  where sway's did nothing; otherwise a held window in sway's layouts could
  be reached only by the pointer.
- **Unbalanced quotes** (2026-11-08): sway strips a lone `"` before a command
  sees it, so `grant "app` grants `app`, where tidewm fails with "expected
  app ids".
- **Activations with the guard off** (2026-11-08): `focus_on_window_activation`
  decides, as in sway, unless a grant names the app. tidewm marks every
  ungranted activation urgent, guard or no guard; here that holds with the
  guard on, whatever the setting says.
- **The focused window stays undimmed under a launcher** (2026-11-09): a
  window dims unless it's the one GET_TREE calls focused, tidewm's rule. In
  sway that window keeps the name while a layer surface, such as tide's
  launcher, has the keyboard, so it stays undimmed, where tidewm dims every
  window then.
- **The dim covers a window's content, not sway's borders or title bars**
  (2026-11-09): tidewm's layer covers its borders too. tide has neither, and
  sway's colors already mark the focused one's.
- **What an output shows** (2026-11-09): every window on its workspace, a
  hidden tab of a tabbed or stacked container included, as tidewm counts a
  monocle's hidden windows; a fullscreen one, on the workspace or across
  every output, counts alone.
- **A click on a window not yet drawn at its place** (2026-11-09): goes to
  that window, as GET_TREE places it. sway shows a window's new place only
  once its client has drawn it there, or after 200 ms, so a click right
  after a window opens or moves found nothing; a press now shows every
  window's new place at once, as that timeout would, and the part of a
  place its client hasn't drawn yet still takes the pointer. tidewm
  places windows at once, with no wait.
- **A part on every output** (2026-11-09): every workspace has a part on every
  output, empty or not, so `GET_TREE` lists every workspace under each
  output, as tidewm's does, and a workspace goes only when every part is
  empty. Parts made only on demand would leave the tree closer to sway's.
- **No workspace moves to an output** (2026-11-09): `move workspace to
  output` fails, ext-workspace's assign does nothing, and `workspace NAME
  output OUTPUT` lines move no part, as a workspace is on every output
  already. tidewm has none of these.
- **A disabled output's windows** (2026-11-09): go to the part of their
  workspace on the first output left, as new windows go there, tiled or
  floating, each with a `move` window event. sway would move its whole
  workspaces over.
- **A floating window's own resize during a configure** (2026-11-10, deferred
  from #13's review): a floating window takes a size its client chose only
  once the client has answered every configure, so a tile's size arriving
  late isn't taken. A client that resizes itself while an unrelated
  configure, such as an activation's, is unanswered, then commits the same
  size again, keeps its old box until its next change of size. Taking the
  size once the configures drain instead let a tile's late size in (3 of 60
  runs of `test_fullscreen.GuardTest`'s held-request test). A fix needs to
  tell a client's own size from a stale one sway sent.
- **No events before a window's new one** (2026-11-10): a window's `new`
  event now comes once its `for_window` rules have run and it has its place,
  as tidewm sends it, and the events its rules would cause before then
  (`floating`, `move`, `title`) aren't sent: `new` says what they did. sway
  sent `new` first, then the rules' events.
- **tidewm's floating places** (2026-11-10): a window floating as it opens
  takes tidewm's place (its parent's middle, or the window area's, capped at
  80% of the output), where sway centered it on the workspace at its own
  size, with `floating_maximum_size`. A floating window's client-chosen size
  waits, too, while sway has a configure scheduled but not yet sent.
- **No `urgent allow|deny`** (2026-11-10): `urgent` takes only `enable` and
  `disable`, as tidewm's does, and every window may mark itself. Each mark,
  by an activation, the guard, X11's hint coming on or `urgent enable`, is
  the most recent, with tidewm's `urgent_order`, even on a window already
  marked. sway's `allow` and `deny` fail.
- **A binding's event when its command fails** (2026-11-10): a binding sends
  its binding event whether or not its command succeeds, as tidewm's does,
  where sway sent it only on success; a key bound to `kill` in a window kept
  open says it ran. Criteria that match nothing fail with tidewm's "No
  window matches the criteria", where sway said "No matching node."
- **No maximize in a floating layout, nor ids across a remap** (2026-11-10):
  tidewm's maximize tests that use its floating layout fail here, as
  tidesway has none (above), and so do its maximize and keep_open tests that
  remap a window, since sway makes a new container, with a new id, for a
  window that maps again.
- **On-demand layer surfaces wait for a click** (2026-11-10): one that takes
  the keyboard on demand, such as a notification's reply field, gets it when
  clicked, as tide's SPEC §14.1 has it, where sway gave it the keyboard as it
  opened in the top or overlay layer and as the pointer crossed it. Once it
  has it, only focus you move (a click, a binding, a command), an exclusive
  layer surface or the lock takes it, as with a focus grab.
- **A virtual keyboard or pointer going leaves the seat one** (2026-11-10): the
  seat keeps its keyboard, or pointer, when the last to go was virtual, as
  wtype's is once it has typed, so clients keep theirs, as tidewm's seat
  does. A real one going still takes it, so a client can tell none is
  plugged in; tide's lock and greeter go by udev (`tide keyboards`) anyway.
- **Focus doesn't follow a jump** (2026-11-10): a pointer motion with no
  time, as a virtual pointer's, moves the pointer without
  `focus_follows_mouse` focusing what it lands on, as in tidewm, and the
  next motion over a window focuses it. tidewm's tests jump the pointer so;
  in sway the jump focused a held window under it
  (`test_focus.HeldCloseTest`, tidesway#7) and the output under it, so a
  drag there started on the wrong output (`test_split.TwoMonitorsTest`).
  Both were flaky. It changes sway's behavior for tools such as `wlrctl`
  that send no time.
- **Flaky tests** (2026-11-09): a test that passes and fails on the same code goes
  in `tests/tidewm-harness/flaky.txt` with a reason, and never fails CI, rather
  than being retried. The 9 put there each flipped in 12 CI runs of main's code
  (main's re-runs and the last commits of the workspaces and measure branches):
  five X11 first-map timeouts, `FollowTest`'s dialog stacking, `HeldCloseTest`
  (above, tidesway#7), and two seen once each with no known cause (`GrantTest`'s
  closed IPC connection, `TwoMonitorsTest`'s resize). `GrantTest`'s was a crash,
  since fixed, and it's back in `passing.txt`. By 2026-11-10 each had its cause
  fixed and moved to `passing.txt`, leaving `flaky.txt` empty: the X11 ones
  were two races in wlroots (`sway/tide/xwayland.c`).
- **`passing.txt` by hand** (2026-11-09): nothing rewrites it. The PR that makes a
  test pass adds it, from the "Newly passing" list in the `test` job's summary,
  so each addition is reviewed. tidewm is pinned (`TIDEWM_REF` in `ci.yml`), so
  a test it renames or removes fails the gate as missing only in the PR that
  moves the pin, which updates the list.
