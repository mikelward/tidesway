# A test window with a subsurface, for tests/tidesway/test_tide_look.py:
# tidewm's test client, which the other tests use, has none. It sets its
# opacity over alpha-modifier-v1, which only it needs.
alpha_modifier_xml = wl_protocol_dir / 'staging/alpha-modifier/alpha-modifier-v1.xml'
alpha_modifier_src = [
	custom_target(
		'test_alpha_modifier_c',
		input: alpha_modifier_xml,
		output: 'alpha-modifier-v1-protocol.c',
		command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
	),
	custom_target(
		'test_alpha_modifier_client_h',
		input: alpha_modifier_xml,
		output: 'alpha-modifier-v1-client-protocol.h',
		command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
	),
]

executable(
	'subsurface',
	['subsurface.c'] + wl_protos_src + alpha_modifier_src,
	dependencies: [wayland_client],
	install: false,
)
