project('xwm','cpp')

src = [
	'src/main.cpp',
	'src/backend.cpp',
	'src/compositor.cpp'
]
	
xcb = [
	dependency('xcb'),
	dependency('xcb-keysyms'),
	dependency('xcb-xfixes'),
	dependency('xcb-composite')
]

vk = [
	dependency('vulkan')
]

executable('xwm',sources:src,dependencies:[xcb,vk],cpp_args:['-std=c++14'])

#http://mesonbuild.com/Custom-build-targets.html

