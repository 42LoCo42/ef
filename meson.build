project(
	'example',
	'c',
	default_options: {
		'c_std': 'c23',
		'buildtype': 'debugoptimized',
		'b_ndebug': 'if-release',
		'warning_level': '3',
	},
)

isClang = meson.get_compiler('c').get_id() == 'clang'

executable(
	'example',
	sources: ['ef.c', 'main.c'],
	c_args: [
		'-DVERSION="@0@"'.format(meson.project_version()),
		'-Wno-empty-translation-unit',
		'-Wno-pragma-once-outside-header',
	] + (isClang ? ['-fblocks'] : []),
	link_args: (isClang ? ['-lBlocksRuntime'] : []),
	dependencies: [],
	install: true,
)
