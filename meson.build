project('xwm','cpp')

src = [
	'src/main.cpp',
	'src/container.cpp',
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

spirv = find_program('glslc')
custom_target('filter_vertex',output:'filter_vertex.spv',input:'src/filter_vertex.hlsl',command:[spirv,'--target-env=vulkan','-fshader-stage=vertex','-x','hlsl','-o','@OUTPUT@','@INPUT@'])
#http://mesonbuild.com/Custom-build-targets.html

