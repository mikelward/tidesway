# tidesway, the command that starts sway as tidewm is started, and the
# look.conf it puts before your config, which it finds where they're
# installed, whatever bindir and datadir are
lookdir = join_paths(prefix, datadir, 'tidesway')
configure_file(
	input: 'tidesway.in',
	output: 'tidesway',
	configuration: {
		'bindir': join_paths(prefix, get_option('bindir')),
		'lookdir': lookdir,
	},
	install_dir: get_option('bindir'),
	install_mode: 'rwxr-xr-x',
)
install_data('look.conf', install_dir: lookdir)
