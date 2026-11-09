# Measurements

Does patching sway in place give tidewm's numbers, where sway plus an IPC helper
didn't? This adds a tidesway column to the tables in tidewm's sway experiment
([`experiments/sway/REPORT.md`][report] at `a83bea8`, "Measurements"), and says how
much of tidewm's suite tidesway passes now.

**Short answer: yes, for everything the benchmark measures.** A new window is in its
final place when it's first seen. Nothing passes through an in-between state, on
open or close. No close showed a wrong frame. The layouts match tidewm's to the pixel.
Replies to commands are sway's speed, not tidewm's: about 0.5 ms slower, still well
under a frame.

[report]: https://github.com/mikelward/tidewm/blob/a83bea8de3fe648f65b1fc22e114319370d06173/experiments/sway/REPORT.md

## How

[`measure.yml`](.github/workflows/measure.yml) runs the experiment's `bench.py` against
tidewm, then against tidesway, in one job on one runner, 50 runs each. tidewm runs
through the experiment's own copy, unchanged. tidesway runs through
[`bench/bench.py`](bench/bench.py), a copy that only adds a `tidesway` mode: it starts
tidesway through `tests/tidewm-harness/run`, as tidewm's suite does here.
[`bench/compare.py`](bench/compare.py) makes the tables. The workflow runs on demand
(`workflow_dispatch`) or when a pull request changes the bench, and takes about 5
minutes.

The numbers below are from [measure run 42563090752][run], on this commit:

- tidesway: main at `9612edaf` (the four tide patches and the click fix), with only
  the bench added;
- tidewm: `a83bea8`, the experiment's branch, whose tidewm is the one REPORT.md
  measured;
- Debian testing's wlroots 0.20, headless, pixman, grim for frames; 4 CPUs (AMD EPYC
  7763).

An earlier run on another runner ([42562628764][run1], AMD EPYC 9V45) gave the same
counts and the same layouts; its times were 20 to 60 percent lower for both.

The stock sway and sway + helper columns are REPORT.md's, from [its run][exprun] on
another runner. Compare their times with care; the counts compare directly.

[run]: https://github.com/mikelward/tidesway/actions/runs/42563090752
[run1]: https://github.com/mikelward/tidesway/actions/runs/42562628764
[exprun]: https://github.com/mikelward/tidewm/actions/runs/42517460212

## Latency

Times in ms, median / p95. "Seen" is when the window first appears in the
compositor's own tree, polled as fast as it answers.

| Transition | tidewm (REPORT) | stock sway | sway + helper | tidewm (here) | tidesway |
|---|---|---|---|---|---|
| New window: seen → its `new` event reaches tide | −0.6 / −0.1 | −0.2 / 1.2 | 2.1 / 183 | −1.2 / −0.5 | −0.3 / 0.6 |
| New window: seen → helper has sway's `new` | – | – | −0.2 / −0.1 | – | – |
| New window: seen → helper sends its layout | – | – | 1.2 / 2.0 | – | – |
| New window: seen → sway acks the layout | – | – | 1.6 / 3.1 | – | – |
| New window: seen → final geometry | 0 / 0 | 0 / 0 | 4.4 / 206 | 0 / 0 | **0 / 0** |
| Close: gone → final geometry | 0 / 0 | 0 / 0 | 0.8 / 1.2 | 0 / 0 | **0 / 0** |
| Workspace switch: command → reply | 0.18 / 0.23 | 0.63 / 0.93 | 2.3 / 3.3 | 0.19 / 0.25 | 0.68 / 1.3 |
| Workspace switch: command → tree settled | 1.6 / 2.3 | 0.8 / 1.2 | 1.0 / 1.3 | 1.6 / 2.9 | 0.97 / 1.5 |
| `focus next`: command → reply | 0.17 / 0.21 | 0.61 / 1.4 | 2.2 / 2.6 | 0.19 / 0.26 | 0.89 / 1.0 |
| Granted window: seen → focused | 0 / 0 | 0 / 0 (no guard) | 2.7 / 3.5 | 0 / 0 | **0 / 0** |

tidesway has no helper, so the helper's rows don't apply. Its replies take as long as
stock sway's. A workspace switch settles sooner than tidewm's, as stock sway's does.

## Intermediate states and flicker

| | tidewm (REPORT) | stock sway | sway + helper | tidewm (here) | tidesway |
|---|---|---|---|---|---|
| New window: distinct tree states before the final one | 0 | 0 | 3 every time | 0 in 50 runs | **0 in 50 runs** |
| New window: focus changes before settling | 0 | 0 | 1 | 0 in 50 runs | **0 in 50 runs** |
| Close: distinct tree states before the final one | 0 | 0 | 1 every time | 0 in 50 runs | **0 in 50 runs** |
| Close: runs with a wrong frame on screen | 0 / 25 | 0 / 25 | 3 / 25 (15 and 25 / 25 earlier) | 0 / 25 | **0 / 25** |
| Open: runs with a wrong frame on screen | 0 / 25 | 0 / 25 | 0 / 25 | 0 / 25 | **0 / 25** |
| Held window (guard on): tree samples where it had focus | 0 | ~900 of ~900 (no guard) | 0 | 0 of 106,006 | **0 of 43,150** |

The helper's close flicker came from laying out in a second transaction. tidesway lays
out a window as sway maps or unmaps it, in the same transaction, so there is no hole
to show. Three windows opening, with `dim_inactive 0.12`, showed two distinct frames on
both, before and after.

## Layout parity

Master/stack with 1 to 5 windows, then closing the first stack window, then zooming
the third: tidesway matched tidewm exactly in **7 of 7 steps**. The helper matched 6
of 7, and stock sway only the lone window.

## IPC cost

20 windows, 200 round trips each, ms, median / p95. There's no proxy on either, so
only the direct rows apply.

| | tidewm (REPORT) | stock sway | sway + helper | tidewm (here) | tidesway |
|---|---|---|---|---|---|
| `GET_VERSION` | 0.042 / 0.047 | 0.039 / 0.050 | 0.030 / 0.032 | 0.036 / 0.061 | 0.039 / 0.049 |
| `GET_TREE` | 0.25 / 0.27 | 0.60 / 0.63 | 0.43 / 0.45 | 0.26 / 0.30 | 0.64 / 0.67 |

tidesway's tree costs what stock sway's does: sway's JSON has more in it than tidewm's.

## What wasn't measured

Nothing the benchmark has was skipped. The benchmark has no keyboard or pointer
rows, so what the suite says of input stands on its own.

## tidewm's suite on tidesway

From main's latest `test` run, [ci run 42562361355][suite] (tidesway `9612edaf`,
tidewm main `1f47ac2`): **348 of 665 pass**, 254 fail, 63 error, 0 time out. The
baseline, sway 1.12 before any tide patch, is [ci run 42523650897][base]: 117 of 664.
`test_split` has one more subtest now (32, from 31). No test that passed in the
baseline fails now.

| File | Tests | Baseline | Now | Change |
|---|---|---|---|---|
| test_capture | 5 | 0 | 0 | – |
| test_commands | 27 | 15 | 20 | +5 |
| test_config | 66 | 17 | 17 | – |
| test_dialogs | 32 | 7 | 11 | +4 |
| test_dimming | 12 | 0 | 11 | +11 |
| test_direction | 12 | 4 | 10 | +6 |
| test_ext_workspace | 6 | 1 | 6 | +5 |
| test_floating | 10 | 0 | 0 | – |
| test_focus | 95 | 5 | 90 | +85 |
| test_fullscreen | 15 | 3 | 14 | +11 |
| test_gestures | 5 | 1 | 1 | – |
| test_inputs | 22 | 3 | 3 | – |
| test_install | 4 | 4 | 4 | – |
| test_ipc | 17 | 6 | 6 | – |
| test_keep_open | 12 | 1 | 1 | – |
| test_layers | 38 | 0 | 0 | – |
| test_layouts | 49 | 0 | 40 | +40 |
| test_lock | 5 | 1 | 1 | – |
| test_maximize | 17 | 0 | 0 | – |
| test_minimize | 10 | 0 | 0 | – |
| test_mouse | 19 | 5 | 5 | – |
| test_outputs | 59 | 20 | 27 | +7 |
| test_resize | 13 | 9 | 12 | +3 |
| test_rules | 13 | 1 | 2 | +1 |
| test_seats | 9 | 0 | 8 | +8 |
| test_shortcuts | 2 | 0 | 0 | – |
| test_split | 32 | 0 | 24 | +24 |
| test_urgency | 9 | 0 | 1 | +1 |
| test_workspaces | 39 | 14 | 33 | +19 |
| test_xwayland | 11 | 0 | 1 | +1 |
| **all** | **665** | **117** | **348** | **+231** |

The files still at or near zero are what no patch has touched yet: layer lists and
`capture` events, `minimize`, `maximize`, `keep_open`, tidewm's floating layout, input
events, and config checking on `reload` (REPORT.md, "The fork path", patches 4 and 5).

[suite]: https://github.com/mikelward/tidesway/actions/runs/42562361355
[base]: https://github.com/mikelward/tidesway/actions/runs/42523650897
