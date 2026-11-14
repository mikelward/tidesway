sway(5)

# NAME

sway - configuration file and commands

# DESCRIPTION

A sway configuration file is a list of sway commands that are executed by sway
on startup.  These commands usually consist of setting your preferences and
setting key bindings. An example config is likely present in /etc/sway/config
for you to check out.

Lines in the configuration file might be extended through multiple lines by
adding a '\\' character at the end of line. e.g.:

```
bindsym Shift+XF86AudioRaiseVolume exec \\
	pactl set-sink-volume @DEFAULT_SINK@ -1%
```

Commands can also be given as a block in the form *command { <subcommands...>
}*. Anything before the opening *{* will be prepended to the lines inside the
block. For example:

```
output eDP-1 {
	background ~/wallpaper.png fill
	resolution 1920x1080
}
```

is identical to

```
output eDP-1 background ~/wallpaper.png fill
output eDP-1 resolution 1920x1080
```

These commands can be executed in your config file, via *swaymsg*(1), or via
the bindsym command.

# COMMAND CONVENTIONS

Commands are split into several arguments using spaces. You can enclose
arguments with quotation marks (*"..."* or *'...'*) to add spaces to a single
argument. You may also run several commands in order by separating each with
*,* or *;*. Criteria is retained across commands separated by *,*, but will be
reset (and allow for new criteria, if desired) for commands separated by a *;*.

Throughout the documentation, *|* is used to distinguish between arguments for
which you may only select one. *[...]* is used for optional arguments, and
*<...>* for arguments where you are expected to supply some value.

# COMMANDS

This section only lists general commands. For input and output commands, refer
to *sway-input*(5) and *sway-output*(5).

## Config only commands
The following commands may only be used in the configuration file.

*bar* [<bar-id>] <bar-subcommands...>
	For details on bar subcommands, see *sway-bar*(5).

*default_orientation* horizontal|vertical|auto
	Sets the default container layout for tiled containers.

*include* <paths...>
	Include files from _paths_. _paths_ can include either a full path or a
	path relative to the parent config, and expands shell syntax (see
	*wordexp*(3) for details). The same include file can only be included once;
	subsequent attempts will be ignored.

*swaybg_command* <command>
	Executes custom background _command_. Default is _swaybg_. Refer to
	*sway-output*(5) for more information.

	It can be disabled by setting the command to a single dash:
	_swaybg\_command -_

*swaynag_command* <command>
	Executes custom command for _swaynag_. Default is _swaynag_. Additional
	arguments may be appended to the end. This should only be used to either
	direct sway to call swaynag from a custom path or to provide additional
	arguments. This should be placed at the top of the config for the best
	results.

	It can be disabled by setting the command to a single dash:
	_swaynag\_command -_

*workspace_layout* default|stacking|tabbed
	Specifies the initial layout for new containers in an empty workspace.

*xwayland* enable|disable|force
	Enables or disables Xwayland support, which allows X11 applications to be
	used. _enable_ will lazily load Xwayland so Xwayland will not be launched
	until the first client attempts to connect. In some cases, such as slower
	machines, it may be desirable to have Xwayland started immediately by
	using _force_ instead of _enable_.

*xwayland_scaling* enable|disable
	tidesway's, as tidewm's and Hyprland's _xwayland:force_zero_scaling_:
	with _disable_, X11 windows are drawn in their output's own pixels, so
	they're sharp at a fractional scale. An app that scales itself, by
	*GDK_SCALE*, *QT_SCALE_FACTOR* or *Xft.dpi*, is then the size it would be
	scaled; one that doesn't is small. A floating X11 window keeps its pixels
	on an output of another scale. With _enable_, the default, X11 windows
	are scaled with their output, and blurry at a fractional scale.

## Runtime only commands
The following commands cannot be used directly in the configuration file.
They are expected to be used with *bindsym* or at runtime through *swaymsg*(1).

*border* none|normal|csd|pixel [<n>]
	Set border style for focused window. _normal_ includes a border of
	thickness _n_ and a title bar. _pixel_ is a border without title bar _n_
	pixels thick. The title bar always shows in stacking or tabbed layouts.
	_csd_ is short for client-side-decorations, which allows the client to draw
	its own decorations. Default is _normal_ with border thickness 2.

*border* toggle
	Cycles through the available border styles.

*exit*
	Exit sway and end your Wayland session.

*floating* enable|disable|toggle
	Make focused window floating, non-floating, or the opposite of what it is now.

<criteria> *focus*
	Moves focus to the container that matches the specified criteria.

*focus* up|right|down|left
	Moves focus to the next container in the specified direction.

*focus* prev|next [sibling]
	Moves focus to the previous or next container in the current layout. By default,
	the last active child of the newly focused container will be focused. The _sibling_
	option indicates not to immediately focus a child of the container.

*focus* child
	Moves focus to the last-focused child of the focused container.

*focus* parent
	Moves focus to the parent of the focused container.

*focus* output up|right|down|left
	Moves focus to the next output in the specified direction.

*focus* output <name>
	Moves focus to the named output.

*focus* tiling
	Sets focus to the last focused tiling container.

*focus* floating
	Sets focus to the last focused floating container.

*focus* mode_toggle
	Moves focus between the floating and tiled layers.

*fullscreen* [enable|disable|toggle] [global]
	Makes focused window fullscreen, non-fullscreen, or the opposite of what it
	is now. If no argument is given, it does the same as _toggle_. If _global_
	is specified, the window will be fullscreen across all outputs.

*gaps* inner|outer|horizontal|vertical|top|right|bottom|left all|current
set|plus|minus|toggle <amount>
	Changes the _inner_ or _outer_ gaps for either _all_ workspaces or the
	_current_ workspace. _outer_ gaps can be altered per side with _top_,
	_right_, _bottom_, and _left_ or per direction with _horizontal_ and
	_vertical_.

*inhibit_idle* focus|fullscreen|open|none|visible
	Set/unset an idle inhibitor for the window. _focus_ will inhibit idle when
	the window is focused by any seat. _fullscreen_ will inhibit idle when the
	view is fullscreen (or a descendant of a fullscreen container) and is
	visible. _open_ will inhibit idle until the window is closed (or the
	inhibitor is unset/changed). _visible_ will inhibit idle when the window is
	visible on any output. _none_ will remove any existing idle inhibitor for
	the window.

	This can also be used with criteria to set an idle inhibitor for any
	existing window or with _for_window_ to set idle inhibitors for future windows.

*layout* default|splith|splitv|stacking|tabbed
	Sets the layout mode of the focused container.

	When using the _stacking_ layout, only the focused window in the container is
	displayed, with the opened windows' list on the top of the container.

	The _tabbed_ layout is similar to _stacking_, but the windows’ list is vertically
	split.

*layout* toggle [split|all]
	Cycles the layout mode of the focused container though a preset list of
	layouts. If no argument is given, then it cycles through stacking, tabbed
	and the last split layout. If _split_ is given, then it cycles through
	splith and splitv. If _all_ is given, then it cycles through every layout.

*layout* toggle [split|tabbed|stacking|splitv|splith] [split|tabbed|stacking|splitv|splith]...
	Cycles the layout mode of the focused container through a list of layouts.

*max_render_time* off|<msec>
	Controls when the relevant application is told to render this window, as a
	positive number of milliseconds before the next time sway composites the
	output. A smaller number leads to fresher rendered frames being composited
	by sway and lower perceived input latency, but if set too low, the
	application may not finish rendering before sway composites the output,
	leading to delayed frames.

	When set to off, the relevant application is told to render this window
	immediately after display refresh. How much time is left for rendering
	before sway composites the output at that point depends on the output
	*max_render_time* setting.

	To set this up for optimal latency:
	. Set up *output max_render_time* (see *sway-output*(5)).
	. Put the target application in _full-screen_ and have it continuously
	  render something.
	. Start by setting *max_render_time 1*. If the application drops
	  frames, increment by *1*.

	This setting only has an effect if a per-output *max_render_time* is in
	effect on the output the window is currently on. See *sway-output*(5) for
	further details.

*allow_tearing* yes|no
	Allows or disallows screen tearing as a result of immediate page flips
	for a fullscreen application.

	When this option is not set, the tearing hints provided by the application
	determine whether tearing is allowed. When _yes_ is specified,
	the application allows tearing regardless of the tearing hints.
	When _no_ is specified, tearing will never be allowed on the application,
	regardless of the tearing hints.

	This setting only has an effect if tearing is allowed on the output through
	the per-output *allow_tearing* setting. See *sway-output*(5) for further
	details.

*move* left|right|up|down [<px> px]
	Moves the focused container in the direction specified. The optional _px_
	argument specifies how many pixels to move the container. If unspecified,
	the default is 10 pixels. Pixels are ignored when moving tiled containers.

*move* [absolute] position <pos_x> [px|ppt] <pos_y> [px|ppt]
	Moves the focused container to the specified position in the workspace.
	The position can be specified in pixels or percentage points, omitting
	the unit defaults to pixels. If _absolute_ is used, the position is
	relative to all outputs. _absolute_ can not be used with percentage points.

*move* [absolute] position center
	Moves the focused container to be centered on the workspace. If _absolute_
	is used, it is moved to the center of all outputs.

*move* position cursor|mouse|pointer
	Moves the focused container to be centered on the cursor.

*move* [container|window] [to] mark <mark>
	Moves the focused container to the specified mark.

*move* [--no-auto-back-and-forth] [container|window] [to] workspace [number] <name>
	Moves the focused container to the specified workspace. The string _number_
	is optional and is used to match a workspace with the same number, even if
	it has a different name.

*move* [container|window] [to] workspace prev|next|current
	Moves the focused container to the previous, next or current workspace on
	this output, or if no workspaces remain, the previous or next output.

*move* [container|window] [to] workspace prev_on_output|next_on_output
	Moves the focused container to the previous or next workspace on this
	output, wrapping around if already at the first or last workspace.

*move* [container|window] [to] workspace back_and_forth
	Moves the focused container to previously focused workspace.

*move* [container|window] [to] output <name-or-id>|current
	Moves the focused container to the specified output.

*move* [container|window] [to] output up|right|down|left
	Moves the focused container to next output in the specified
	direction.

*move* [container|window] [to] scratchpad
	Moves the focused container to the scratchpad.

*move* workspace [to] output <name-or-id>|current
	Moves the focused workspace to the specified output.

*move* workspace to [output] <name-or-id>|current
	Moves the focused workspace to the specified output.

*move* workspace [to] output up|right|down|left
	Moves the focused workspace to next output in the specified direction.

*move* workspace to [output] up|right|down|left
	Moves the focused workspace to next output in the specified direction.

*nop* <comment>
	A no operation command that can be used to override default behaviour. The
	optional comment argument is ignored, but logged for debugging purposes.

*reload*
	Reloads the sway config file and applies any changes. The config file is
	located at path specified by the command line arguments when started,
	otherwise according to the priority stated in *sway*(1). In tidesway, as
	in tidewm, a file with an error, or including one, fails the reload,
	naming the first error's file and line, and the config stays as it was.
	A reload sent over IPC has happened by the time its reply comes.

*rename* workspace [<old_name>] to <new_name>
	Rename either <old_name> or the focused workspace to the <new_name>

*resize* shrink|grow up|right|down|left|width|height [<amount> [px|ppt]]
	Resizes the currently focused container by _amount_, specified in pixels or
	percentage points. If the units are omitted, floating containers are resized
	in px and tiled containers by ppt. _amount_ will default to 10 if omitted.
	For tiling containers, space is taken/given from the container in the
	specified direction. If _width_ or _height_ is specified, space will be
	taken/given from all other containers.

*resize* set height <height> [px|ppt]
	Sets the height of the container to _height_, specified in pixels or
	percentage points. If the units are omitted, floating containers are
	resized in px and tiled containers by ppt. If _height_ is 0, the container
	will not be resized. For tiling containers, space is taken/given from all
	other containers.

*resize* set [width] <width> [px|ppt]
	Sets the width of the container to _width_, specified in pixels or
	percentage points. If the units are omitted, floating containers are
	resized in px and tiled containers by ppt. If _width_ is 0, the container
	will not be resized. For tiling containers, space is taken/given from all
	other containers.

*resize* set [width] <width> [px|ppt] [height] <height> [px|ppt]
	Sets the width and height of the container to _width_ and _height_,
	specified in pixels or percentage points. If the units are omitted,
	floating containers are resized in px and tiled containers by ppt. If
	_width_ or _height_ is 0, the container will not be resized on that axis.
	For tiling containers, space is taken/given from all other containers.

*scratchpad* show
	Shows a window from the scratchpad. Repeatedly using this command will
	cycle through the windows in the scratchpad.

*shortcuts_inhibitor* enable|disable
	Enables or disables the ability of clients to inhibit keyboard
	shortcuts for a window. This is primarily useful for virtualization and
	remote desktop software. It affects either the currently focused window
	or a set of windows selected by criteria. Subcommand _disable_
	additionally deactivates any active inhibitors for the given window(s).
	Criteria are particularly useful with the *for_window* command to
	configure a class of windows differently from the per-seat defaults
	established by the *seat* subcommand of the same name. See
	*sway-input*(5) for more ways to affect inhibitors.

*split* vertical|v|horizontal|h|none|n|toggle|t
	Splits the current container, vertically or horizontally. When _none_ is
	specified, the effect of a previous split is undone if the current
	container is the only child of a split parent. When _toggle_ is
	specified, the current container is split opposite to the parent
	container's layout.

*splith*
	Equivalent to *split horizontal*

*splitv*
	Equivalent to *split vertical*

*splitt*
	Equivalent to *split toggle*

*sticky* enable|disable|toggle
	"Sticks" a floating window to the current output so that it shows up on all
	workspaces.

*swap* container with id|con_id|mark <arg>
	Swaps the position, geometry, and fullscreen status of two containers. The
	first container can be selected either by criteria or focus. The second
	container can be selected by _id_, _con_id_, or _mark_. _id_ can only be
	used with xwayland windows. If the first container has focus, it will retain
	focus unless it is moved to a different workspace or the second container
	becomes fullscreen on the same workspace as the first container. In either
	of those cases, the second container will gain focus.

*gesture* swipe|pinch|hold <fingers>
	tidesway's, as tidewm's: a touchpad gesture with that many fingers, as
	if from a touchpad, which goes to the window under the pointer or a
	*bindgesture* binding, as a real one does: a swipe 10 to the right, a
	pinch 10 to the right that zooms to twice the size and turns 90 degrees,
	or a hold. For a test, which has no touchpad.

*switch* lid|tablet on|off
	tidesway's, as tidewm's: runs the *bindswitch* lines for that switch
	as if it had moved, for a test, which has no lid, or a lid the hardware
	doesn't report.

*title_format* <format>
	Sets the format of window titles. The following placeholders may be used:

	*%title*
		The title supplied by the window

	*%app_id*
		The wayland app ID (applicable to wayland windows only)

	*%class*
		The X11 classname (applicable to xwayland windows only)

	*%instance*
		The X11 instance (applicable to xwayland windows only)

	*%shell*
		The protocol the window is using (typically xwayland or xdg_shell)

	*%sandbox_engine*
		The associated sandbox engine

	*%sandbox_app_id*
		The app ID provided by the associated sandbox engine

	*%sandbox_instance_id*
		The instance ID provided by the associated sandbox engine

	This command is typically used with *for_window* criteria. For example:

		for_window [title="."] title_format "<b>%title</b> (%app_id)"

	Note that markup requires pango to be enabled via the *font* command.

	The default format is "%title".

## Config or runtime commands
The following commands may be used either in the configuration file or at
runtime.

*assign* <criteria> [→] [workspace] [number] <workspace>
	Assigns windows matching _criteria_ (see *CRITERIA* for details) to
	_workspace_. The → (U+2192) is optional and cosmetic. This command is
	equivalent to:

		for_window <criteria> move container to workspace <workspace>

*assign* <criteria> [→] output left|right|up|down|<name>
	Assigns windows matching _criteria_ (see *CRITERIA* for details) to the
	specified output. The → (U+2192) is optional and cosmetic. This command is
	equivalent to:

		for_window <criteria> move container to output <output>

*bindsym* [--whole-window] [--border] [--exclude-titlebar] [--release] [--locked] \
[--to-code] [--input-device=<device>] [--no-warn] [--no-repeat] [--inhibited] \
[--kept-open] [--name <name>] [--desc <text>] [Group<1-4>+]<key combo> <command>
	Binds _key combo_ to execute the sway command _command_ when pressed. You
	may use XKB key names here (*wev*(1) is a good tool for discovering these).
	With the flag _--release_, the command is executed when the key combo is
	released. If _input-device_ is given, the binding will only be executed for
	that input device and will be executed instead of any binding that is
	generic to all devices. If a group number is given, then the binding will
	only be available for that group. By default, if you overwrite a binding,
	swaynag will give you a warning. To silence this, use the _--no-warn_ flag.

	For specifying modifier keys, you can use the XKB modifier names _Shift_,
	_Lock_ (for Caps Lock), _Control_, _Mod1_ (for Alt), _Mod2_ (for Num Lock),
	_Mod3_ (for XKB modifier Mod3), _Mod4_ (for the Logo key), and _Mod5_ (for
	AltGr). In addition, you can use the aliases  _Ctrl_ (for Control), _Alt_
	(for Alt), and _Super_ (for the Logo key).

	Unless the flag _--locked_ is set, the command will not be run when a
	screen locking program is active. If there is a matching binding with
	and without _--locked_, the one with will be preferred when locked and the
	one without will be preferred when unlocked.

	With _--desc_ _text_, tidewm's, the binding says what it does, as
	*bindsym*'s. If there are matching bindings
	and one has both _--input-device_ and _--locked_ and the other has neither,
	the former will be preferred even when unlocked.

	Unless the flag _--inhibited_ is set, the command will not be run when
	a keyboard shortcuts inhibitor is active for the currently focused
	window. Such inhibitors are usually requested by remote desktop and
	virtualization software to enable the user to send keyboard shortcuts
	to the remote or virtual session. The _--inhibited_ flag allows one to
	define bindings which will be exempt from pass-through to such
	software. The same preference logic as for _--locked_ applies.

	With the flag _--kept-open_, tidesway's, the binding counts only while
	the window you're typing in is kept open (*keep\_open*), so it can take
	an app's own close keys, such as Ctrl+W, from that window alone.

	With _--name_ _name_, tidesway's, the binding has a name that *rebind*
	can give other keys. A name is a lower-case letter, then lower-case
	letters, digits and _\__. Several lines can give a name keys, each in
	the same mode with the same command and flags; those are its default
	keys. A later line that gives the name another command, flags or mode
	redefines it, as tidewm's does, so a config that includes another can
	bind its names to commands of its own: the name's earlier keys go.

	With _--desc_ _text_, or _--desc=text_, tidewm's, the binding says what
	it does, for a shell's list of keyboard shortcuts (GET_BINDINGS). _text_
	is a word, or text in double quotes. A named binding's is its name's,
	for each of its keys.

	Unless the flag _--no-repeat_ is set, the command will be run
	repeatedly when the key is held, according to the repeat
	settings specified in the input configuration.

	Bindings to keysyms are layout-dependent. This can be changed with the
	_--to-code_ flag. In this case, the keysyms will be translated into the
	corresponding keycodes in the first configured layout.

	Mouse bindings operate on the container under the cursor instead of the
	container that has focus. Mouse buttons can either be specified in the form
	_button[1-9]_ or by using the name of the event code (ex _BTN\_LEFT_ or
	_BTN\_RIGHT_). For the former option, the buttons will be mapped to their
	values in X11 (1=left, 2=middle, 3=right, 4=scroll up, 5=scroll down,
	6=scroll left, 7=scroll right, 8=back, 9=forward). For the latter option,
	you can find the event names using _libinput debug-events_.

	The priority for matching bindings is as follows: input device, group,
	and locked state.

	_--whole-window_, _--border_, and _--exclude-titlebar_ are mouse-only options
	which affect the region in which the mouse bindings can be triggered.  By
	default, mouse bindings are only triggered when over the title bar. With the
	_--border_ option, the border of the window will be included in this region.
	With the _--whole-window_ option, the cursor can be anywhere over a window
	including the title, border, and content. _--exclude-titlebar_ can be used in
	conjunction with any other option to specify that the titlebar should be
	excluded from the region of consideration.

	If _--whole-window_ is given, the command can be triggered when the cursor
	is over an empty workspace. Using a mouse binding over a layer surface's
	exclusive region is not currently possible.

	Example:
```
		# Execute firefox when alt, shift, and f are pressed together
		bindsym Mod1+Shift+f exec firefox
```

	*bindcode* [--whole-window] [--border] [--exclude-titlebar] [--release] \
[--locked] [--input-device=<device>] [--no-warn] [--no-repeat] [--inhibited] \
[Group<1-4>+]<code> <command>
	is also available for binding with key/button codes instead of key/button names.

*rebind* <name> <key combo...>|none
	tidesway's: gives the binding *bindsym --name* named _name_ these keys in
	place of the ones it has, or none. A key it gives is taken from the
	binding that had it. A name no line has given is logged and skipped, so
	a shell's file that outlives a name the config dropped still loads. It
	can't be in a mode block. A shell's keyboard shortcuts can write a file
	of these lines that the config includes:

```
		bindsym --name terminal $mod+t exec foot
		include tide-*.conf

		# tide-keys.conf, which a shell writes
		rebind terminal Mod4+Return
```

*bindswitch* [--locked] [--no-warn] [--reload] [--desc <text>] <switch>:<state> <command>
	Binds <switch> to execute the sway command _command_ on state changes.
	Supported switches are _lid_ (laptop lid), _tablet_ (tablet mode) and
	_keypad_slide_ (whether the device keypad is exposed or not) switches. Valid
	values for _state_ are _on_, _off_ and _toggle_. These switches are on when
	the device lid is shut, when tablet mode is active and when the keypad is
	exposed respectively. _toggle_ is also supported to run a command both when
	the switch is toggled on or off.

	Unless the flag _--locked_ is set, the command will not be run when a
	screen locking program is active. If there is a matching binding with
	and without _--locked_, the one with will be preferred when locked and the
	one without will be preferred when unlocked.

	If the _--reload_ flag is given, the binding will also be executed when
	the config is reloaded. _toggle_ bindings will not be executed on reload.
	The _--locked_ flag will operate as normal so if the config is reloaded
	while locked and _--locked_ is not given, the binding will not be executed.

	By default, if you overwrite a binding, swaynag will give you a warning. To
	silence this, use the _--no-warn_ flag.

	Example:
```
		# Show the virtual keyboard when tablet mode is entered.
		bindswitch tablet:on busctl call --user sm.puri.OSK0 /sm/puri/OSK0 sm.puri.OSK0 SetVisible b true

		# Log a message when the laptop lid is opened or closed.
		bindswitch lid:toggle exec echo "Lid moved"
```

*bindgesture* [--exact] [--input-device=<device>] [--no-warn] \
<gesture>[:<fingers>][:directions] <command>
	Binds _gesture_ to execute the sway command _command_ when detected.
	Currently supports the _hold_, _pinch_ or _swipe_ gesture. Optionally
	can be limited to bind to a certain number of _fingers_ or, for a
	_pinch_ or _swipe_ gesture, to certain _directions_.

[[ *type*
:[ *fingers*
:< *direction*
|  hold
:- 1 - 5
:  none
|  swipe
:  3 - 5
:  up, down, left, right
|  pinch
:  2 - 5
:  all above + inward, outward, clockwise, counterclockwise

	The _fingers_ can be limited to any sensible number or left empty to accept
	any finger counts.
	Valid directions are _up_, _down_, _left_ and _right_, as well as _inward_,
	_outward_, _clockwise_, _counterclockwise_ for the _pinch_ gesture.
	Multiple directions can be combined by a plus.

	If a _input-device_ is given, the binding will only be executed for
	that input device and will be executed instead of any binding that is
	generic to all devices. By default, if you overwrite a binding,
	swaynag will give you a warning. To silence this, use the _--no-warn_ flag.

	The _--exact_ flag can be used to ensure a binding only matches when exactly
	all specified directions are matched and nothing more. If there is matching
	binding with _--exact_, it will be preferred.

	The priority for matching bindings is as follows: input device, then
	exact matches followed by matches with the highest number of matching
	directions.

	Gestures executed while the pointer is above a bar are not handled by sway.
	See the respective documentation, e.g. *bindgesture* in *sway-bar*(5).

	Example:
```
		# Allow switching between workspaces with left and right swipes
		bindgesture swipe:right workspace prev
		bindgesture swipe:left workspace next

		# Allow container movements by pinching them
		bindgesture pinch:inward+up move up
		bindgesture pinch:inward+down move down
		bindgesture pinch:inward+left move left
		bindgesture pinch:inward+right move right

```

*client.background* <color>
	This command is ignored and is only present for i3 compatibility.

*client.<class>* <border> <background> <text> [<indicator> [<child_border>]]
	Configures the color of window borders and title bars. The first three
	colors are required. When omitted _indicator_ will use a sane default and
	_child_border_ will use the color set for _background_. Colors may be
	specified in hex, either as _#RRGGBB_ or _#RRGGBBAA_.

	The available classes are:

	*client.focused*
		The window that has focus.

	*client.focused_inactive*
		The most recently focused window within a container which is not focused.

	*client.focused_tab_title*
		A window that has focused descendant container.
		Tab or stack container title that is the parent of the focused container
		but is not directly focused. Defaults to focused_inactive if not
		specified and does not use the indicator and child_border colors.

	*client.placeholder*
		Ignored (present for i3 compatibility).

	*client.unfocused*
		A window that does not have focus.

	*client.urgent*
		A window with an urgency hint. *Note*: Native Wayland windows do not
		support urgency. Urgency only works for Xwayland windows.

	The meaning of each color is:

	_border_
		The border around the title bar.

	_background_
		The background of the title bar.

	_text_
		The text color of the title bar.

	_indicator_
		The color used to indicate where a new window will open. In a tiled
		container, this would paint the right border of the current window if a
		new window would be opened to the right.

	_child_border_
		The border around the window itself.

The default colors are:

[- *class*
:[ _border_
:[ _background_
:[ _text_
:[ _indicator_
:[ _child_border_
|[ *background*
:  n/a
:  #ffffff
:  n/a
:  n/a
:  n/a
|  *focused*
:  #4c7899
:  #285577
:  #ffffff
:  #2e9ef4
:  #285577
|  *focused_inactive*
:  #333333
:  #5f676a
:  #ffffff
:  #484e50
:  #5f676a
|  *focused_tab_title*
:  #333333
:  #5f676a
:  #ffffff
:  n/a
:  n/a
|  *unfocused*
:  #333333
:  #222222
:  #888888
:  #292d2e
:  #222222
|  *urgent*
:  #2f343a
:  #900000
:  #ffffff
:  #900000
:  #900000
|  *placeholder*
:  #000000
:  #0c0c0c
:  #ffffff
:  #000000
:  #0c0c0c


*default_border* normal|none|pixel [<n>]
	Set default border style for new tiled windows. Config reload won't affect
	existing windows, only newly created ones after the reload.

*default_floating_border* normal|none|pixel [<n>]
	Set default border style for new floating windows. This only applies to
	windows that are spawned in floating mode, not windows that become floating
	afterwards.

*exec* <shell command>
	Executes _shell command_ with sh.

*exec_always* <shell command>
	Like *exec*, but the shell command will be executed _again_ after *reload*.

*floating_maximum_size* <width> x <height>
	Specifies the maximum size of floating windows. -1 x -1 removes the upper
	limit. The default is 0 x 0, which will use the width and height of the
	entire output layout as the maximums

*floating_minimum_size* <width> x <height>
	Specifies the minimum size of floating windows. The default is 75 x 50.

*floating_modifier* <modifier> [normal|inverse]
	When the _modifier_ key is held down, you may hold left click to move
	windows, and right click to resize them. Setting _modifier_ to _none_
	disables this feature. If _inverse_ is specified, left click is used for
	resizing and right click for moving. In tidesway, as in tidewm, a floating
	window resizes by its bottom right corner, which the pointer jumps to.
	tidesway, as tidewm, also takes modifiers joined by _+_, as _Mod4+Shift_,
	every one of which must be held; it can't be in a mode.

*focus_follows_mouse* yes|no|always
	If set to _yes_, moving your mouse over a window will focus that window. If
	set to _always_, the window under the cursor will always be focused, even
	after switching between workspaces. In tidesway, as in tidewm, a jump of
	the pointer with no time, as a virtual pointer's, doesn't focus; the next
	move over a window does.

*focus_on_window_activation* smart|urgent|focus|none
	This option determines what to do when a client requests window activation.
	If set to _urgent_, the urgent state will be set for that window. If set to
	_focus_, the window will become focused. If set to _smart_, the window will
	become focused only if it is already visible, otherwise the urgent state
	will be set. Default is _urgent_.

*focus_wrapping* yes|no|force|workspace
	This option determines what to do when attempting to focus over the edge
	of a container. If set to _no_, the focused container will retain focus,
	if there are no other containers in the direction. If set to _yes_, focus
	will be wrapped to the opposite edge of the container, if there are no
	other containers in the direction. If set to _force_, focus will be wrapped
	to the opposite edge of the container, even if there are other containers
	in the direction. If set to _workspace_, focus will wrap like in the _yes_
	case and additionally wrap when moving outside of workspaces boundaries.
	Default is _yes_.

*font* [pango:]<font>
	Sets font to use for the title bars. To enable support for pango markup,
	preface the font name with _pango:_. For example, _monospace 10_ is the
	default font. To enable support for pango markup, _pango:monospace 10_
	should be used instead. Regardless of whether pango markup is enabled,
	_font_ should be specified as a pango font description. For more
	information on pango font descriptions, see
	https://docs.gtk.org/Pango/type_func.FontDescription.from_string.html#description

*force_display_urgency_hint* <timeout> [ms]
	If an application on another workspace sets an urgency hint, switching to this
	workspace may lead to immediate focus of the application, which also means the
	window decoration color would be immediately reset to *client.focused*. This
	may make it unnecessarily hard to tell which window originally raised the
	event. This option allows one to set a _timeout_ in ms to delay the urgency hint reset.

*titlebar_border_thickness* <thickness>
	Thickness of the titlebar border in pixels

*titlebar_padding* <horizontal> [<vertical>]
	Padding of the text in the titlebar. _horizontal_ value affects horizontal
	padding of the text while _vertical_ value affects vertical padding (space
	above and below text). Padding includes titlebar borders so their value
	should be greater than titlebar_border_thickness. If _vertical_ value is
	not specified it is set to the _horizontal_ value.

*for_window* <criteria> <command>
	Whenever a window that matches _criteria_ appears, run list of commands.
	See *CRITERIA* for more details.

*gaps* inner|outer|horizontal|vertical|top|right|bottom|left <amount>
	Sets default _amount_ pixels of _inner_ or _outer_ gap, where the inner
	affects spacing around each window and outer affects the spacing around each
	workspace. Outer gaps are in addition to inner gaps. To reduce or remove
	outer gaps, outer gaps can be set to a negative value. _outer_ gaps can
	also be specified per side with _top_, _right_, _bottom_, and _left_ or
	per direction with _horizontal_ and _vertical_.

	This affects new workspaces only, and is used when the workspace doesn't
	have its own gaps settings (see: workspace <ws> gaps ...).

*hide_edge_borders* [--i3] none|vertical|horizontal|both|smart|smart_no_gaps
	Hides window borders adjacent to the screen edges. Default is _none_. The
	_--i3_ option enables i3-compatible behavior to hide the title bar on
	tabbed and stacked containers with one child. The _smart_|_smart_no_gaps_
	options are equivalent to setting _smart_borders_ smart|no_gaps and
	_hide_edge_borders_ none.

*input* <input_device> <input-subcommands...>
	For details on input subcommands, see *sway-input*(5).

	\* may be used in lieu of a specific device name to configure all input
	devices. A list of input device names may be obtained via *swaymsg -t
	get_inputs*.

*seat* <seat> <seat-subcommands...>
	For details on seat subcommands, see *sway-input*(5).

*kill*
	Kills (closes) the currently focused container and all of its children.

*smart_borders* on|no_gaps|off
	If smart_borders are _on_, borders will only be enabled if the workspace
	has more than one visible child. If smart_borders is set to _no_gaps_,
	borders will only be enabled if the workspace has more than one visible
	child and gaps equal to zero.

*smart_gaps* on|off|toggle|inverse_outer
	If smart_gaps are _on_ gaps will only be enabled if a workspace has more
	than one child. If smart_gaps are _inverse_outer_ outer gaps will only
	be enabled if a workspace has exactly one child.

*mark* --add|--replace [--toggle] <identifier>
	Marks are arbitrary labels that can be used to identify certain windows and
	then jump to them at a later time. Each _identifier_ can only be set on a
	single window at a time since they act as a unique identifier. By default,
	*mark* sets _identifier_ as the only mark on a window. _--add_ will instead
	add _identifier_ to the list of current marks for that window. If _--toggle_
	is specified mark will remove _identifier_ if it is already marked.

*mode* <mode>
	Switches to the specified mode. The default mode is _default_.

*mode* [--pango_markup] <mode> <mode-subcommands...>
	The only valid _mode-subcommands..._ are *bindsym*, *bindcode*,
	*bindswitch*, and *set*. If _--pango_markup_ is given, then _mode_ will be
	interpreted as pango markup.

*mouse_warping* output|container|none
	If _output_ is specified, the mouse will be moved to new outputs as you
	move focus between them. If _container_ is specified, the mouse will be
	moved to the middle of the container on switch. Default is _output_.

*no_focus* <criteria>
	Prevents windows matching <criteria> from being focused automatically when
	they're created. This has no effect on the first window in a workspace.

*output* <output_name> <output-subcommands...>
	For details on output subcommands, see *sway-output*(5).

	\* may be used in lieu of a specific output name to configure all outputs.
	A list of output names may be obtained via *swaymsg -t get_outputs*.

*popup_during_fullscreen* smart|ignore|leave_fullscreen
	Determines what to do when a fullscreen window opens a dialog.
	If _smart_ (the default), the dialog will be displayed. If _ignore_, the
	dialog will not be rendered. If _leave_fullscreen_, the window will exit
	fullscreen mode and the dialog will be rendered.

*primary_selection* enabled|disabled
	Enable or disable the primary selection clipboard. May only be configured
	at launch. Default is _enabled_.

*set* $<name> <value>
	Sets variable $_name_ to _value_. You can use the new variable in the
	arguments of future commands. When the variable is used, it can be escaped
	with an additional $ (ie $$_name_) to have the replacement happen at run
	time instead of when reading the config. However, it does not always make
	sense for the variable to be replaced at run time since some arguments do
	need to be known at config time.

*show_marks* yes|no
	If *show_marks* is yes, marks will be displayed in the window borders.
	Any mark that starts with an underscore will not be drawn even if
	*show_marks* is yes. The default is _yes_.

*opacity* [set|plus|minus] <value>
	Adjusts the opacity of the window between 0 (completely transparent) and
	1 (completely opaque). If the operation is omitted, _set_ will be used.

*tiling_drag*  enable|disable|toggle
	Sets whether or not tiling containers can be dragged with the mouse. If
	_enabled_ (default), the _floating_mod_ can be used to drag tiling, as well
	as floating, containers. Using the left mouse button on title bars without
	the _floating_mod_ will also allow the container to be dragged. _toggle_
	should not be used in the config file.

*tiling_drag_threshold* <threshold>
	Sets the threshold that must be exceeded for a container to be dragged by
	its titlebar. This has no effect if _floating_mod_ is used or if
	_tiling_drag_ is set to _disable_.  Once the threshold has been exceeded
	once, the drag starts and the cursor can come back inside the threshold
	without stopping the drag.  _threshold_ is multiplied by the scale of the
	output that the cursor on.  The default is 9.

*title_align* left|center|right
	Sets the title alignment. If _right_ is selected and _show_marks_ is set
	to _yes_, the marks will be shown on the _left_ side instead of the
	_right_ side.

*unbindswitch* <switch>:<state>
	Removes a binding for when <switch> changes to <state>.

*unbindgesture* [--exact] [--input-device=<device>] \
<gesture>[:<fingers>][:directions]
	Removes a binding for the specified _gesture_, _fingers_
	and _directions_ combination.

*unbindsym* [--whole-window] [--border] [--exclude-titlebar] [--release] [--locked] \
[--to-code] [--input-device=<device>] <key combo>
	Removes the binding for _key combo_ that was previously bound with the
	given flags.  If _input-device_ is given, only the binding for that
	input device will be unbound.

	*unbindcode* [--whole-window] [--border] [--exclude-titlebar] [--release] \
[--locked] [--input-device=<device>] <code>
	is also available for unbinding with key/button codes instead of key/button names.

*unlock*
	tidesway's, as tidewm's: drops a lock whose screen locker has gone, as
	after it crashed, showing the windows again and focusing the one you were
	in. Until then the screen stays covered and no input reaches a window. From
	a TTY, *swaymsg -s* _socket_ *unlock* gets you back in, keeping the
	session. It fails while a locker holds the lock, and with nothing locked.

*unmark* [<identifier>]
	*unmark* will remove _identifier_ from the list of current marks on a
	window. If _identifier_ is omitted, all marks are removed.

*urgent* enable|disable
	Marks the window urgent, as the most recent mark, even if it's marked
	already, or takes its mark away, as tidewm does, for a shell's own marks.
	Every window may mark itself: sway's _allow_ and _deny_ aren't supported.

*workspace* [--no-auto-back-and-forth] [number] <[num:]name>
	Switches to the specified workspace. The _num:_ portion of the name is
	optional and will be used for ordering. If _num:_ is not given and
	_name_ is a number, then it will be also be used for ordering.

	If the _no-auto-back-and-forth_ option is given, then this command will
	not perform a back-and-forth operation when the workspace is already
	focused and _workspace_auto_back_and_forth_ is enabled.

	If the _number_ keyword is specified and a workspace with the number
	already exists, then the workspace with the number will be used. If a
	workspace with the number does not exist, a new workspace will be created
	with the name _name_.

*workspace* prev|next
	Switches to the next workspace on the current output or on the next output
	if currently on the last workspace.

*workspace* prev_on_output|next_on_output
	Switches to the next workspace on the current output.

*workspace* back_and_forth
	Switches to the previously focused workspace.

*workspace* <name> gaps inner|outer|horizontal|vertical|top|right|bottom|left
<amount>
	Specifies that workspace _name_ should have the given gaps settings when it
	is created.

	This command does not affect existing workspaces. To alter the gaps of an
	existing workspace, use the _gaps_ command.

*workspace* <name> output <outputs...>
	Specifies that workspace _name_ should be shown on the specified _outputs_.
	Multiple outputs can be listed and the first available will be used. If the
	workspace gets placed on an output further down the list and an output that
	is higher on the list becomes available, the workspace will be moved to the
	higher priority output.

	This command does not affect existing workspaces. To move an existing
	workspace, use the _move_ command in combination with the _workspace_
	criteria (non-empty workspaces only) or _workspace_ command (to switch
	to the workspace before moving).

*workspace_auto_back_and_forth* yes|no
	When _yes_, repeating a workspace switch command will switch back to the
	prior workspace. For example, if you are currently on workspace 1,
	switch to workspace 2, then invoke the *workspace 2* command again, you
	will be returned to workspace 1. Default is _no_.

# TIDE'S LAYOUTS

tidesway lays out a workspace by tide's rules, as tidewm does, rather than
i3's split tree. Its tiled windows, in tree order, are the masters, then the
stack. Each workspace has its own layout, _mfact_ and _nmaster_, and each
layout its own _mfact_ and _nmaster_ there. A lone tiled window, in any
layout, is centered at the _single\_window_ width.

A new workspace starts in tide's layouts. One of sway's layouts or split
commands (*layout splith*, *splitv*, *split*, ...) puts that workspace back
in sway's layouts, until a *layout* command names one of tide's.

*layout* tile|threecol|twocol|monocle
	Sets the focused workspace's layout. _tile_ puts the masters in a column
	on the left and the stack on the right; _threecol_ centers the master,
	with a stack either side, right first; _twocol_ puts the masters side by
	side, with the stack on the right; _monocle_ gives each window the whole
	area.

*layout* next|prev
	Goes to the next or previous layout in the cycle, _tile threecol
	twocol_ and round, or _layout\_cycle_'s. From _monocle_, it goes on from
	the layout before it.

*layout* toggle
	On a workspace in tide's layouts, goes back to the layout before.

*mfact* <F>|+<F>|-<F>
	Sets the master area's share of the width, from 0.1 to 0.9.

*nmaster* <N>|+<N>|-<N>
	Sets how many windows the master area holds.

*single_width* <F>|reset
	Sets the lone window's width on the focused workspace, from 0.1 to 1, in
	place of the _single\_window_ lines, or goes back to them.

*zoom*
	Swaps the focused window with the master, or with the next window if
	it's the master.

*movestack* next|prev
	Swaps the focused window with the next window in the stack, or the
	previous one. From the end, _next_ goes round to the master's place; from
	there, _prev_ goes to the end.

On a workspace in tide's layouts, *focus* and *move* left|right|up|down go
by the tiles' places on screen, as tidewm's do: to the window that way
sharing the most height (width, going up or down), then the nearest. A move
swaps the two tiles. Past an output's edge, focus goes to the next output's
window at that edge, and a tile moves there, in that window's place.
*focus next|prev* goes round the workspace's windows in tiled order, and
*resize grow|shrink width* on a tile moves the master/stack edge, in
percentage points or pixels: a master's grows the masters, a stack window's
the stack. A tile's height is its stack's equal share, so resizing it fails.

These may only be used in the configuration file:

*single_window* <min_aspect> <width>
	Centers a lone tiled window at _width_ of the window area, from 0.1 to 1,
	when the area is at least _min\_aspect_ as wide as it's high, from 0 to
	100. Of several lines, the one with the largest _min\_aspect_ the area
	reaches wins, and none means the whole width. The lines replace tide's,
	which are 80% from 2.1 and 60% from 3.2.

*default_layout* normal|ultrawide tile|threecol|twocol|monocle
	The layout a new workspace starts in, on an output narrower than
	_ultrawide\_aspect_ or not. Without it, _threecol_ on an ultrawide and
	_tile_ otherwise.

*ultrawide_aspect* <N>
	How wide an output must be for its height, from 0.1 to 100, for
	_default\_layout ultrawide_. 2.1 without it.

*layout_defaults* tile|threecol|twocol [mfact <F>] [nmaster <N>]
	The _mfact_, from 0.1 to 0.9, and _nmaster_, up to 100, from 0 for _tile_
	and 1 for the others, that a layout starts with. Without it, _tile_ starts
	at 0.55 with one master, _threecol_ at 0.5 with one, and _twocol_ at 0.72
	with two. A reload that changes them starts every workspace's layouts from
	them again.

*layout_cycle* tile|threecol|twocol...
	The layouts *layout next* and *prev* go through, each at most once.

*new_window* end|top|next|master
	Where a new tiled window goes: at the _end_ of the stack, as without the
	line, at its _top_, just after the masters, _next_ after the focused
	window, or as the _master_. A window moved in from another workspace, or
	tiled again after floating, counts as new. (*new_window* with a border
	style is still *default_border*'s old name.)

*resize_grab* <pixels>
	How many pixels either side of an edge a plain left press drags it from,
	from 0 to 32; 0 turns the drag off. 4 without it.

A plain left press near the edge between the masters and the stack drags it,
setting _mfact_; either side of _threecol_'s centered master drags it both
ways, and a lone window's sides drag its width, centered, as *single_width*
sets it. Over a floating window, a layer surface or a popup, on an output
that isn't focused, or under a fullscreen window, the press goes where it
would without the edge. *floating_modifier*'s resize button on a tile drags
its nearest edge the same way, or floats a tile with none.

In IPC, *GET_WORKSPACES* and *GET_TREE* give a workspace's tide layout as its
_layout_, and the _layout_ event (0x8000001e) says when it changes or a
workspace is shown: _change_ ("layout"), _workspace_, _output_, _layout_ (null
for sway's layouts) and _symbol_ (tidewm's: "[]=", "|M|", "||=", or
monocle's "[n]", n its windows). See *sway-ipc*(7).

# TIDE'S FOCUS GUARD

With the focus guard on, as tidewm has it, nothing takes the keyboard unless
you asked for it. A new window takes focus when a grant names its app or a
process it descends from; when it's from the app you're in, by app ID or
process; when it's a portal's dialog, such as a file chooser, while a window
has focus; or when it's a polkit agent's prompt right after a key press. Any
other opens where it would, held from focus and marked urgent, even the lone
window on an empty workspace, until you go to it: by the pointer, a click,
*focus next*, its workspace, or *[con_id=N] focus*. When the window you're in
closes, focus never goes to a held window. A window with neither an app ID nor
a title is a status popup: it floats, guard or no guard, and with the guard
it's held but not marked, and *focus next|prev* and the arrow keys pass it
by. With nothing focused, *focus next|prev* goes to the workspace's most
recent window, or one the guard holds, in sway's layouts too.

A grant is used once, by the first new window that matches, or by an
activation of a window its app already had; one naming an app is used before
_\*_. A granted window takes focus even behind a fullscreen window
on its workspace, which leaves fullscreen. A key press, a click, or focus moving to another window by any means
cancels its focus: a new window taking focus with no grant, or a workspace
switch, among them, but not focus coming back to the window you were in, as
when a launcher closes, nor focus moving as an output goes. A canceled grant
still places its window: if you've left the workspace it was made on, the
window opens there, or on its _--workspace_, held and marked, as Hyprland's
initial_workspace_tracking does; a dialog goes with its parent, and _\*_
without _--pid_, or a notification's grant, places nothing. A grant lapses
after _grant\_seconds_.

A held window that asks to go fullscreen, as it opens or later, or that a
*fullscreen* command or rule makes fullscreen, goes fullscreen once you go to
it, unless the request is taken back first; until then it leaves the window
you're in as it is.

A grant naming its app lets an activation focus a window, with the guard on
or off. With the guard on, any other activation marks the window urgent,
whatever *focus_on_window_activation* says; with it off, that setting
decides. With the guard off, a new window still uses its grant, and goes to
its _--workspace_.

Closing the window you're in focuses the window under the pointer, as tide's
SPEC §14.2 has it, going by where the windows left are once they've moved
into its space; the pointer stays where it is, and a layer surface over a
window, such as a notification, doesn't count. Focus goes to the window you
were in before, as in sway, when the pointer is over nothing or over a window
the guard holds, and a dialog closing hands focus back to its parent. A window
you weren't in closing moves no focus. This holds with the guard off too.

*grant* [--pid <pid>] [--key <key>] [--workspace <name>] [<app>...]
	A one-shot grant: the first new window of one of the apps takes focus,
	or of any app's for _\*_, which stands alone. Apps match ignoring case
	and a ".desktop", and a bare name matches a qualified ID's last part.
	With _--pid_, a window whose process descends from _pid_ takes it too.
	_--workspace_ takes the window to that workspace, made if it's new, with
	focus. _--key_ names the grant for *grant extend*. Options take _--opt
	value_ or _--opt=value_.

*grant* --or-recent <app>...
	A notification's click: the same grant, which, if it lapses unused and
	you've done nothing else, brings up the most recently focused window of
	any of the apps.

*grant extend* --key <key> [--pid <pid>] <app> <id>...
	Adds the ids to the grant made with that _key_, _pid_ and first _app_, if it
	still holds.

*focus recent* <app>...
	Brings up the most recently focused window of any of the apps. It's you
	moving on, so it cancels every grant. It fails for an app with no window.

This may only be used in the configuration file:

*focus_guard* enable|disable|grant_seconds <S>|prompt_seconds <S>
	Turns the guard on or off; it's off without the line. _grant\_seconds_
	is how long a grant holds, 10 without it, and _prompt\_seconds_ how soon
	after a key press a polkit prompt may take focus, 2 without it, each more
	than 0 and at most a day.

In IPC, a window has tidewm's _workspace_ and _output_, its workspace's and
output's names, and *GET_SEATS* gives tidewm's _last\_input_, your last key
press or click as _type_ ("key" or "button"), _window_ (the id of the window
it went to, or null) and _msec\_ago_, or null before any, and
_prompt\_may\_focus_, whether a polkit prompt opening now may take focus.

# TIDE'S FOCUS GRABS

tidesway offers hyprland-focus-grab-v1, as tidewm does, which tide's
notification center takes through Quickshell. While a grab holds, the
keyboard stays on its surfaces and the pointer enters only them. A click
anywhere else ends it and goes no further. So does focus you move, by a
binding or a command, an exclusive layer surface such as the launcher
opening, or the lock. A window opening doesn't take the keyboard meanwhile.
A second grab ends the first, and none can start under the lock.

A layer surface that takes the keyboard on demand, such as a notification's
reply field, gets it when you click it, not when it opens or the pointer
crosses it. It keeps it the same way: until you click elsewhere, use a
binding or a command, or the launcher or the lock takes it.

# TIDE'S DIALOGS

As in tidewm, a window floats when it has a parent, is a modal dialog
(xdg-dialog-v1), asks for a fixed size, or has neither an app ID nor a
title, or when a rule floats it. One that floats as it opens goes in the
middle of its parent, or of its output's window area without one, as big as
it asks but no more than 80% of the output, unless its fixed size is bigger,
and kept in the window area. A browser's picture-in-picture window, titled
"Picture-in-Picture" or "Picture in picture", floats a quarter of the output
each way in the window area's bottom right corner, and is sticky. A dialog
stays above its parent, and follows it to another workspace or output, to
the middle of it there, unless it's sticky itself.

A floating window's place is remembered for its app for the session:
wherever one of its floating windows last opened, moved or was resized to,
floating a window of that app by hand puts it, kept in the window area. With
no place yet, it stays where it was tiled. Dialogs, fixed-size and modal
windows and picture-in-picture place themselves, so theirs isn't kept.

# TIDE'S WINDOW STATES

*maximize* [enable|disable|toggle]
	Makes the window cover its output's window area, above the other tiles,
	which stay where they are, with the bar still showing, as tidewm does.
	It lasts until it's turned off, the window closes, or another window on
	its workspace takes focus (not a dialog of it). An app's own request
	maximizes it too. Fullscreen covers it while it lasts, and a floating
	window gets its place back after. Alone, it toggles.

*keep_open* enable|disable|toggle
	Keeps the window open, as tidewm does for tide's pinned windows: *kill*,
	from a key or IPC, leaves it open, and a _close\_refused_ window event
	says so. While you're typing in it, its bindings made with *--kept-open*
	count, so a config can take an app's own close keys from it alone. It's
	kept open on the workspace it's on, minimized or not: on any other, it
	isn't.
	A window that maps again is a new one, not kept open. Its own close
	button still closes it.

*send_shortcut* <mods>+<key>
	Sends the window a press and release of _key_, such as _ctrl+v_, with the
	modifiers _ctrl_, _shift_, _alt_ or _super_ held, whether or not it's
	focused, as tidewm and Hyprland's sendshortcut do, for tide's picker to
	paste. _key_ is a key name, sent as the key that gives it on the
	keyboard's layout now, at its lowest level, so a shifted one, as _plus_
	on a US keyboard, wants _shift_ too. Focus stays where it was. It fails
	while the screen is locked.

*minimize* [enable|disable|toggle]
	Hides the window, as tidewm does: it leaves the layout, and focus on it
	goes to the window you were in before. It stays on its workspace, which
	stays while it waits, and keeps its states. Focusing it by any means
	brings it back, at the end of the stack. Alone, it toggles. An app's own
	request to minimize does nothing.

# TIDE'S DIMMING

With *dim\_inactive* above 0, as tidewm has it, every window but the focused
one is darkened by a translucent black layer over its content, which clicks
go through. Windows aren't made see-through. An output showing one window,
or a fullscreen one, has nothing to tell apart, so nothing on it dims. Nor
do picture-in-picture windows, by the title "Picture-in-Picture", video, by
the app's content type (wp-content-type-v1), or a window *dim disable*
exempts. A window's menus and tooltips, and layer surfaces such as a bar or
a launcher, never dim; a window that's still focused while a launcher has the
keyboard doesn't either.

*dim* enable|disable|toggle
	Whether the window dims when it isn't focused. A *for\_window* rule's
	*dim disable* exempts a screen share's preview, say.

This may only be used in the configuration file:

*dim\_inactive* <strength>
	How dark the layer is, from 0, none, to 1, black; tide's is 0.07.
	Without the line, nothing dims.

In IPC, a window has tidewm's _dim_, whether it dims when it isn't focused,
and _dimmed_, whether it's dimmed now.

# TIDE'S FADES

With *fade\_duration* above 0, as tidewm has it, a window fades in as it
opens, and the dim (above) fades as focus moves, each easing out over that
long. A window that opens dimmed has its dim fade in with it. A border or
title bar shows at once. A reload changes no window's dim by a fade.

This may only be used in the configuration file:

*fade\_duration* <milliseconds>
	How long a fade takes, from 0, at once, to 3600000; tide's is 300.
	Without the line, nothing fades.

# TIDE'S ROUNDED CORNERS AND SHADOWS

With *corner\_radius* above 0, each window's corners are rounded, showing
what's behind them, as tide's are on Hyprland. These windows stay square: a
fullscreen one, one with a border or title bar, a floating one that draws
its own shadow, as GTK's do, an X11 one in its output's own pixels
(*xwayland\_scaling disable*), and one whose colors an app describes, as HDR
video's. *shadows* puts a soft shadow around windows, as in tidewm: each
floating one, the focused one alone, or all of them. A fullscreen or
maximized window has none, nor a floating one that draws its own. Menus,
tooltips and layer surfaces, such as a bar or a launcher, have neither.

Tiles touch, so a tile's shadow falls on the tiles beside it. The focused
tile's is over them, and goes with focus; with *shadows all* the other
tiles' are under every tile, so they show only in the gaps between tiles,
if there are any. Of tabbed or stacked windows, the one showing casts the
shadow, around its tabs too. A tile's shadow stops at the edge of its
output's window area, so none reaches a bar or the next output. A shadow
takes no clicks and moves no window. GET_TREE and window events say whether
a window casts one, as its _shadow_.

These may only be used in the configuration file:

*corner\_radius* <pixels>
	How far each corner is rounded, from 0, square, to 100; tide's is 6.
	Without the line, corners are square.

*shadows* disable|floating|active|all
	Which windows have shadows: none, each floating window, the focused
	window alone, whether it's tiled or floating, or every window.
	*enable* means *floating*. Without the line, none do.

*shadow\_blur\_radius* <pixels>
	How far a shadow reaches past its window, from 0 to 100. The default
	is 8.

*shadow\_color* <color>
	A shadow's color where it's darkest, by its window, as _#RRGGBB_ or
	_#RRGGBBAA_. The default is _#00000044_.

# TIDE'S WORKSPACES

As in tidewm, a workspace spans every output: each output shows its own part
of it, the windows on that output, and one workspace is current, on every
output at once. Switching workspaces switches every output, and focus stays
on the output it was on. A workspace is made the first time it's named, with
a part on every output, and goes once it's empty and not current, unless
it's persistent.

*workspace next*|*prev* go through the workspaces in tidewm's order: those
named by a number by that number, then the rest in the order they were made,
going round. *next\_on\_output* and *prev\_on\_output* do the same, as every
output has every workspace. *move* *container* *to* *workspace* takes a
window to that workspace's part on the window's own output; *move*
*container* *to* *output* takes it to the other output's part of its
workspace. *move workspace to output* fails, as a workspace is on every
output already.

An output that's disabled gives each part's windows to the part of the same
workspace on the first output left. One that's enabled gets a part of every
workspace, showing the current one. An output that's only powered off keeps
its windows.

*output* <name>|\* power|dpms on|off|toggle
	As tidewm's, at runtime: turns the output off as an idle daemon does,
	keeping its windows and its place, or on again. \* means every enabled
	output, each toggled by its own power. A disabled output stays off,
	and the command fails for it alone, as it does for a name no output
	has. The config file's power lines are sway's.

*persist workspace* <name>
	Keeps that workspace, making it if it's new, even while it's empty.
	_name_ may be in double quotes.

*unpersist workspace* <name>
	Lets that workspace go once it's empty and not current.

In IPC, _GET\_WORKSPACES_ lists each workspace once, in the order made, as its
part on the focused output, with tidewm's _persistent_; _focused_ and
_visible_ both mean it's current, and _urgent_ means a window on any part is.
_GET\_TREE_ has every output's parts. The workspace events are a workspace's,
not a part's: _init_ when it's made, _empty_ when it goes, _focus_ when it
becomes current, and _urgent_. ext-workspace has one group, on every output,
holding each workspace once, which a client may activate.

# CRITERIA

A criteria is a string in the form of, for example:

```
[app_id="some-application" title="[Rr]egex.*"]
```

The string contains one or more (space separated) attribute/value pairs. They
are used by some commands to choose which windows to execute actions on. All
attributes must match for the criteria to match. Criteria is retained across
commands separated by a *,*, but will be reset (and allow for new criteria, if
desired) for commands separated by a *;*.

Criteria may be used with either the *for_window* or *assign* commands to
specify operations to perform on new windows. A criteria may also be used to
perform specific commands (ones that normally act upon one window) on all windows
that match that criteria. For example:

Focus on a window with the mark "IRC":

```
[con_mark="IRC"] focus
```

Kill all windows where the title contains "Emacs":

```
[title="Emacs"] kill
```

Several attributes allow regular expressions. These use Perl-compatible regular
expressions (PCRE2), which are documented in *pcre2pattern*(3) and summarized in
*pcre2syntax*(3). For example, this moves all windows with titles ending in
"sway" or "Sway" to workspace 1:

```
[title="[Ss]way$"] move workspace 1
```

You may like to use swaymsg -t get_tree for finding the values of these
properties in practice for your applications.

The following attributes may be matched with:

*all*
	Matches all windows.

*app_id*
	Compare value against the app id. Can be a regular expression. If value is
	\_\_focused\_\_, then the app id must be the same as that of the currently
	focused window. _app_id_ are specific to Wayland applications.

*class*
	Compare value against the window class. Can be a regular expression. If
	value is \_\_focused\_\_, then the window class must be the same as that of
	the currently focused window. _class_ are specific to X11 applications and
	require XWayland.

*con_id*
	Compare against the internal container ID, which you can find via IPC. If
	value is \_\_focused\_\_, then the id must be the same as that of the
	currently focused window.

*con_mark*
	Compare against the window marks. Can be a regular expression.

*floating*
	Matches floating windows.

*id*
	Compare value against the X11 window ID. Must be numeric. id is specific to
	X11 applications and requires XWayland.

*instance*
	Compare value against the window instance. Can be a regular expression. If
	value is \_\_focused\_\_, then the window instance must be the same as that
	of the currently focused window. instance is specific to X11 applications and
	requires XWayland.

*pid*
	Compare value against the window's process ID. Must be numeric.

*shell*
	Compare value against the window shell, such as "xdg_shell" or "xwayland".
	Can be a regular expression. If value is \_\_focused\_\_, then the shell
	must be the same as that of the currently focused window.

*tag*
	Compare value against the tag. _tag_ is specific to Wayland applications.

*tiling*
	Matches tiling windows.

*title*
	Compare against the window title. Can be a regular expression. If value is
	\_\_focused\_\_, then the window title must be the same as that of the
	currently focused window.

*urgent*
	Compares the urgent state of the window. Can be _first_, _last_, _latest_,
	_newest_, _oldest_ or _recent_.

*window_role*
	Compare against the window role (WM_WINDOW_ROLE). Can be a regular
	expression. If value is \_\_focused\_\_, then the window role must be the
	same as that of the currently focused window. window_role is specific to X11
	applications and requires XWayland.

*window_type*
	Compare against the window type (\_NET_WM_WINDOW_TYPE). Possible values
	are normal, dialog, utility, toolbar, splash, menu, dropdown_menu,
	popup_menu, tooltip and notification. window_type is specific to X11
	applications and requires XWayland.

*workspace*
	Compare against the workspace name for this window. Can be a regular
	expression. If the value is \_\_focused\_\_, then all the windows on the
	currently focused workspace matches.

*sandbox_engine*
	Compare against the associated sandbox engine. Can be a regular expression.
	If the value is \_\_focused\_\_, then the sandbox engine must be the same as
	that of the currently focused window.

*sandbox_app_id*
	Compare against the app ID provided by the associated sandbox engine. Can be
	a regular expression. If the value is \_\_focused\_\_, then the sandbox app
	ID must be the same as that of the currently focused window.

*sandbox_instance_id*
	Compare against the instance ID provided by the associated sandbox engine.
	Can be a regular expression. If the value is \_\_focused\_\_, then the
	sandbox instance ID must be the same as that of the currently focused
	window.

# SEE ALSO

*sway*(1) *sway-input*(5) *sway-output*(5) *sway-bar*(5) *sway-ipc*(7)
*pcre2pattern*(3) *pcre2syntax*(3)
