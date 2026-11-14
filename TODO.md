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
- ~~**Rounded corners and shadows**~~. Done: tide's look on Hyprland, its
  `rounding` and `shadow`. `corner_radius` rounds each window's corners, and
  `shadows`, `shadow_blur_radius` and `shadow_color` put a soft shadow around
  each floating one, the focused one or them all, on wlroots' own scene graph
  and any of its renderers
  (`sway/tide/look.c`; *Decisions needing review* says how, and why not
  SceneFX). Still Hyprland's alone: blur and animations.
- **A tide session**. tide's shell runs on tidesway as on tidewm, from tide's
  own config and conf's over it, so the Appearance page's rounded corners
  reach it (maintainer, 2026-11-14). In order:
  - ~~tidewm's `bindsym --desc`, a later `bindsym --name` line redefining the
    name, and a minimized pinned window staying open~~. Done: tide's
    `tidewm/tide.conf` and conf's `config/tidewm/config` over it load but for
    one line.
  - ~~`fade_duration`, that line: a window fades in as it opens, and the dim
    fades as focus moves, as tidewm's and Hyprland's do~~. Done
    (`sway/tide/fade.c`): tide's config and conf's over it load whole.
  - ~~A `tidesway` command, started as tidewm is, with `-c` and `-s`~~.
    Done (`tide/tidesway.in`, which the harness's `run` now is): tidewm's
    look, the config, then `tidesway.d/*.conf` beside it, where tide can
    put `corner_radius`, which tidewm would fail on.
  - Then, in tide: its tidesway session (mikelward/tide#495) runs that
    command; the Appearance page writes `corner_radius` to
    `tidesway.d/`; and `tide-setup` builds tidesway.
- **The color behind windows**. tidewm draws its `rootcolor`, `#222222`,
  where no window is; tidesway shows black unless swaybg runs. Nine of
  tidewm's `test_effects` read that color, so they can't pass: its tests
  of a shadow on the wallpaper, and a window that opens dimmed fading in.
  A tenth is in tidewm's floating layout, which tidesway hasn't.
- **Track sway**. A weekly check of swaywm/sway: new commits on the current release
  branch (`v1.12`) become a PR cherry-picking them; a new release tag becomes an issue,
  since moving to it is a rebase of `main` (`RELEASE_BASE`). Today `v1.12` has nothing
  past `1.12`. Why this can't sync as tidewm does: tide's `docs/dwl-vs-sway.md`.
- ~~**X11 at a fractional scale**~~. Done: tidewm's `xwayland_scaling disable`
  (tide's SPEC §17, Hyprland's `xwayland:force_zero_scaling`), as tidewm's `x11.h`
  does it: a zxdg_output_manager_v1 only Xwayland sees, giving each output's box
  spread by the largest scale; X11 configures and input points translated to and
  from those boxes; and the scene showing an X11 buffer one pixel to one. Of
  tidewm's `test_xwayland`, four can't pass here: two assume tidewm's 1px
  borders, which tide's look has none of; one reads GET_CONFIG, which the
  harness's wrapper config can't give; and a drag over another window loses its
  press to Xwayland (see *Decisions needing review*). tidesway's own
  `test_tide_x11.py` covers the first two and the drag for tidesway.
- ~~**Input lines in tidewm's words**~~. Done: the input command failing when no
  device matches, an unknown `type:`, `xkb_switch_layout` past the keymap's
  layouts, and XKB's errors as "XKB can't make a keymap of its xkb_ settings".
  Still sway's: a keymap that fails only once a later line merges into it is
  blamed on that later line, not on the line that set its layout as tidewm's is.

## Baseline

`tests/tidewm-harness/passing.txt` is the baseline CI holds tidesway to: the 341
tests that pass on main (2026-11-09, `9612edaf`, tidewm main at `1f47ac2`), less
the 9 then in `flaky.txt`, and since then `GrantTest`'s grant on a workspace, once
its crash was fixed, and each test a later PR made pass. A PR that makes a test pass for good adds it there. CI runs
tidewm's suite at the commit `ci.yml`'s `TIDEWM_REF` pins, so tidewm's changes
reach it only through a PR that moves the pin and updates the lists. The pin
moved from `1f47ac2` to `e6e7c3f` on 2026-11-13: Debian testing took wtype 0.5,
which numbers the keys it types from 2, and two of `test_inputs`' tests of a
keyboard in its own group pressed a key by its number; `e6e7c3f` is the first
commit whose tests press it as either wtype numbers it. Of the tests its 15
commits added, the eight that pass went into `passing.txt`. A ninth,
`test_capture`'s capture of a window on a workspace not shown, passed in 5
runs of 6 and failed once with "failed buffer-constraints", so it was left
out. The cause (2026-11-14): sway tells a new window no size at first, so
the app drew at its own, mapped, and was then told its place's; the new
event went at map, and a capture begun then had a buffer for the old size.
A window's new event now waits for the size (`sway/tide/announce.c`), and
that test and its neighbor passed 56 runs of 56, so it's in. It moved again, to tidewm's main at `96a691b`, on
2026-11-14, with tidewm's `--desc`, its redefining of a named binding and its
keeping a minimized pinned window open, which tide's and conf's configs need
(below). Of that suite's 765 tests, 640 are in `passing.txt`: those that passed
in three runs. Two of `test_borders`' passed in only some, so they're out;
one, a fullscreen window's border, was in and passed only when the tree was
read before the window's first frame. GET_TREE now says a fullscreen window
has none, so it and the other pass each time, and with the fades (below) two
of `test_effects`' do: 643. `FadeTest`'s dim fading as focus moves passed in
one run of two, so it's out. It moved again, to tidewm's main at `1f28603`, on
2026-11-14, with tidewm's `shadows disable|floating|active|all` and its
`shadow` in GET_TREE: of its 800 tests, 674 are in `passing.txt`, 30 of them
its new shadow tests. Its other 5 new ones can't pass: four read the color
behind windows and one is in its floating layout (above).

Before any tide patch, tidewm's suite on sway 1.12 as forked (2026-11-08, tidewm main
at `1f47ac2`): **117 of 664 pass**, 421 fail, 126 error, 0 time out. A test with
subtests counts once per subtest. Nothing passes yet in layouts, split, minimize,
maximize, layers, floating, dimming, capture, seats or urgency; focus passes 5 of 95.
Each patch's PR says how far it moves this.

## Decisions needing review

- **Rounded corners without SceneFX** (2026-11-13): SwayFX rounds corners
  with SceneFX, which replaces wlroots' scene graph and renderer. SceneFX
  0.5 builds against wlroots 0.20 and Debian testing packages it, but it
  draws with GLES2 alone: not pixman, which tidewm's suite runs on (it needs
  a GPU or `/dev/udmabuf`), and not Vulkan, which sway's HDR needs. sway's
  window capture also walks wlroots' own scene graph, not SceneFX's. So
  tidesway keeps wlroots' and draws the corners itself: a rounded window's
  surfaces are clipped to the rows between its corners, and the rows the
  corners are in, a strip of `corner_radius` rows at its top and its bottom,
  are composited by wlroots' renderer into a buffer of their own, with the
  corners cut out, smoothed in the output's pixels. A strip is drawn again
  when a commit damages its rows, a subsurface commits, or the window's size
  or dimming changes: two small render passes each. Not measured against
  `MEASUREMENTS.md` yet. The pointer in a strip goes to the surface under
  it. Any change to a subsurface of a rounded window asks for a frame, and a
  surface wholly in them, which the scene graph has clipped out and never
  tells of a frame, is told by the strip's output's (#18's review).
- **Which windows stay square** (2026-11-13): a fullscreen one, as on
  Hyprland; one with a border or title bar of sway's, which tide's look has
  none of and which would stay square around it; a floating one that draws
  its own corners and shadow past its geometry, as GTK's do; an X11 one in
  its output's own pixels on a scaled output (`xwayland_scaling disable`),
  whose surface isn't in the layout's units; one with a color description
  (HDR), as the strips are composited as sRGB; and one narrower than two
  radii or no taller.
- **Corners during a resize** (2026-11-13): while sway shows a window's
  saved buffers, waiting for its client to draw at a new size, its strips
  stay as they were, with those buffers, and both change in the same frame.
- **Which windows cast a shadow** (2026-11-14): `shadows
  disable|floating|active|all`, tidewm's names, for tide's setting; `enable`
  still means `floating`, which was all there was. A window that draws its
  own shadow gets none, nor does a fullscreen or maximized one. The falloff
  is Hyprland's `render_power 2`, gone at `shadow_blur_radius`, tide's
  `range`.
- **A tile's shadow** (2026-11-14): tiles touch, so one's falls on its
  neighbors, and sway stacks tiles in the order it lays them out. So a
  tile's shadow isn't in its window's tree, as a floating window's is, but
  in one of two trees beside sway's tiles, below the floating windows, as
  tidewm has them: the focused tile's over every tile, and with `all` the
  others' under them all, where only gaps show them. Hyprland draws the
  focused tile last to the same end. With no gaps, `all` and `active` look
  the same but for floating windows. `active` goes by the window GET_TREE
  calls focused: the floating container it's in, if it's in one; the tab
  showing, with its tabs. A tile's shadow is cut off at its output's window
  area, or a lone tile's would fall on the bar and the next output. It
  takes its window's opacity and fade, not being in its tree.
  GET_TREE's `shadow`, tidewm's, goes by the config and the commands so
  far, not by what's laid out yet, so it's right as a command returns.
- **The look's lines** (2026-11-13): `shadows` is tidewm's
  (its main, `d6171f7`); `corner_radius`, `shadow_blur_radius` and
  `shadow_color` are SwayFX's, as tidewm has none. Config file only, as
  `dim_inactive` is. Off unless the config has
  them, as sway looks.
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
- **A reload with an error fails** (2026-11-10): `reload` fails, naming its
  first three errors as "FILE:LINE: ERROR", an included file's included, then
  how many more, and keeps the config it had, as tidewm's does; sway's always
  succeeded, showing errors in swaynag, and skipped an included file's. A
  missing included file is still skipped.
- **Config errors in tidewm's words, sway's lines kept** (2026-11-10): an
  unknown line, a binding's unknown key or flag, and bad `bindsym`,
  `bindswitch`, `set` and `include` lines say what tidewm's do. sway's own
  flags and lines stay: `--border`, `bindswitch --reload` and `bindswitch` in
  a mode, which tidewm's tests call errors; so does `mode NAME` alone in the
  file, which starts sway in that mode, and a mode name with a `;`.
- **GET_CONFIG under the harness** (2026-11-10): the harness's config
  includes the test's, so `config` and `path` are the harness's and the
  test's file is in `included_configs`; tidewm's tests of those fail for that
  alone.
- **Workspaces are named by number** (2026-11-10): a new workspace takes the
  next free number, so tidesway starts on `1`, as tidewm does. sway named it
  after the first workspace a binding shows.
- **Binding events and command replies as i3's** (2026-11-10): a binding
  event has one `input_code` and one `symbol`, without sway's `input_codes`
  and `symbols` lists, and a failed command's reply gives `parse_error` only
  when it's true, as i3 and tidewm do. A client reading sway's lists gets
  none.
- **Outputs named as tidewm names them** (2026-11-10): by Hyprland's
  description too; GET_OUTPUTS gives a missing make, model or serial as ""
  where sway said "Unknown"; in the file, a line naming what an output is
  beats one naming its connector whatever the order; and the output command
  fails for a name no output has, where sway kept it for one that might
  connect later.
- **Focus on close goes under the pointer** (2026-11-10): closing the window
  you're in focuses the window under the pointer, by the windows' places
  once they've moved in, with the focus guard on or off, as tidewm and
  tide's SPEC §14.2 have it; sway focused the one you were in before.
- **Named bindings** (2026-11-10): `bindsym --name` and `rebind`, as tidewm's.
  A rebind binds each key as a `bindsym` line with the name's flags would,
  taking the key from the binding that had it, without swaynag's warning.
- **Known flaky: maximize ended by focus** (2026-11-10):
  `test_maximize.MaximizeTest.test_another_window_on_the_workspace_taking_focus_ends_it`
  fails 2 runs in 60 under load: the test client prints "maximized" but
  not "unmaximized", though WAYLAND_DEBUG shows sway sending the configure
  without the maximized state each time. Not found yet; it's in
  `flaky.txt`.
- **floating_modifier resizes by the corner** (2026-11-10): the modifier and
  the resize button resize a floating window by its bottom right corner,
  which the pointer jumps to, as tidewm's and dwl's do, where sway resized
  by the corner nearest the pointer. The harness's config has
  `floating_modifier Mod1`, as tidewm's built-in mouse does.
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
- **No maximize in a floating layout** (2026-11-10): tidewm's maximize
  tests that use its floating layout fail here, as tidesway has none
  (above).
- **A window keeps its id as it maps again** (2026-11-10): sway gave the new
  container a new id; tidesway gives it the window's first, as tidewm does,
  so a shell that knows it by id still does.
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
- **Input lines checked as tidewm checks them** (2026-11-10): an on or off input
  setting such as `tap` takes only what sway's `parse_boolean` reads, so `tap
  maybe`, which sway took as off, fails. An input line in the config file with a
  quote it doesn't close fails. The `input` command over IPC fails when no device
  matches, as tidewm's does, where sway kept it for a device that came later; the
  config file's lines still wait for one.
- **X11 windows open within their size hints** (2026-11-10): an X11 window
  opens at the size its hints allow, as in tidewm, where sway opened a window
  that asks for a minimum bigger than it is at its own size.
- **X11 drags in tidewm's tests** (2026-11-10): sway moves the pointer to the
  window under it as a button comes up, as it always has. Xwayland handles its
  input later, so when a test presses, drags onto another window and releases
  in one burst, the press goes nowhere. Input from a person comes spread out in
  time, so it isn't lost; tidewm's `test_a_drag_out_of_an_x11_window_stays_in_its_pixels`
  stays failing rather than tidesway's pointer focus changing.
- **Only floating X11 windows keep their pixels** (2026-11-10): with
  `xwayland_scaling disable`, a floating X11 window keeps its pixel size on an
  output of another scale. Windows in a floating split are laid out within it as
  tiles are, so like tiled ones they take the layout's size there, as in tidewm,
  which has no floating splits; a split may hold Wayland windows too, whose
  pixels follow the output's scale.
