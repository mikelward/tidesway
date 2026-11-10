# tidesway

tidesway is [tide]'s fork of sway, as [tidewm] is tide's fork of dwl: sway
1.12 with what tide needs from a compositor, to measure against tidewm
([why][SWAY.md]). It has:

* tide's layouts (`tile`, `threecol`, `twocol`, `monocle`), with `mfact`,
  `nmaster`, `single_width`, `zoom` and `movestack`, placed before a new
  window is first shown, and tidewm's layout lines and IPC (`man 5 sway`,
  "TIDE'S LAYOUTS");
* tide's focus guard, so a window you didn't ask for never takes focus, with
  tidewm's `grant`, `focus recent` and `focus_guard` lines (`man 5 sway`,
  "TIDE'S FOCUS GUARD");
* tide's dimming: `dim_inactive` darkens every window but the one you're in,
  rather than making it see-through, with tidewm's `dim` command for windows
  that shouldn't dim (`man 5 sway`, "TIDE'S DIMMING");
* tide's workspaces, which span every monitor: each output shows its own part
  of the current one, and they all switch together, with tidewm's `persist`
  and `unpersist` commands and its workspace IPC (`man 5 sway`, "TIDE'S
  WORKSPACES");
* fixes for what tide hit in sway: a floating window's dialogs stay above it,
  and an X11 window always shows up, past two races in wlroots that could
  leave one unmapped.

How it compares with tidewm, in latency, flicker and tidewm's tests, is in
[MEASUREMENTS.md](MEASUREMENTS.md).

Everything else is sway's, as described below. tidesway builds the same way, with
meson against wlroots 0.20, and releases as `v1.12.0-tide.N`. Report its bugs
here, not to sway.

[tide]: https://github.com/mikelward/tide
[tidewm]: https://github.com/mikelward/tidewm
[SWAY.md]: https://github.com/mikelward/tidewm/blob/main/SWAY.md

# sway

**[English][en]** - [عربي][ar] - [Azərbaycanca][az] - [Česky][cs] - [Deutsch][de] - [Dansk][dk] - [Español][es] - [Français][fr] - [ქართული][ge] - [Ελληνικά][gr] - [हिन्दी][hi] - [Magyar][hu] - [فارسی][ir] - [Italiano][it] - [日本語][ja] - [한국어][ko] - [Nederlands][nl] - [Norsk][no] - [Polski][pl] - [Português][pt] - [Română][ro] - [Русский][ru] - [Српски][sr] - [Svenska][sv] - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

sway is an [i3]-compatible [Wayland] compositor. Read the [FAQ]. Join the
[IRC channel] \(#sway on irc.libera.chat).

## Release Signatures

Releases are signed with [E88F5E48] and published [on GitHub][GitHub releases].

## Installation

### From Packages

Sway is available in many distributions. Try installing the "sway" package for
yours.

### Compiling from Source

Check out [this wiki page][Development setup] if you want to build the HEAD of
sway and wlroots for testing or development.

Install dependencies:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (optional: additional image formats for system tray)
* [swaybg] (optional: wallpaper)
* [scdoc] (optional: man pages) \*
* git (optional: version info) \*

_\* Compile-time dep_

Run these commands:

    meson setup build/
    ninja -C build/
    sudo ninja -C build/ install

## Configuration

If you already use i3, then copy your i3 config to `~/.config/sway/config` and
it'll work out of the box. Otherwise, copy the sample configuration file to
`~/.config/sway/config`. It is usually located at `/etc/sway/config`.
Run `man 5 sway` for information on the configuration.

## Running

Run `sway` from a TTY or from a display manager.

[en]: https://github.com/swaywm/sway#readme
[ar]: README.ar.md
[az]: README.az.md
[cs]: README.cs.md
[de]: README.de.md
[dk]: README.dk.md
[es]: README.es.md
[fr]: README.fr.md
[ge]: README.ge.md
[gr]: README.gr.md
[hi]: README.hi.md
[hu]: README.hu.md
[ir]: README.ir.md
[it]: README.it.md
[ja]: README.ja.md
[ko]: README.ko.md
[nl]: README.nl.md
[no]: README.no.md
[pl]: README.pl.md
[pt]: README.pt.md
[ro]: README.ro.md
[ru]: README.ru.md
[sr]: README.sr.md
[sv]: README.sv.md
[tr]: README.tr.md
[uk]: README.uk.md
[zh-CN]: README.zh-CN.md
[zh-TW]: README.zh-TW.md
[i3]: https://i3wm.org/
[Wayland]: http://wayland.freedesktop.org/
[FAQ]: https://github.com/swaywm/sway/wiki
[IRC channel]: https://web.libera.chat/gamja/?channels=#sway
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub releases]: https://github.com/swaywm/sway/releases
[Development setup]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
[swaybg]: https://github.com/swaywm/swaybg/
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
