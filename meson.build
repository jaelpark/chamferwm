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

glslc = find_program('glslc')
glslc_invoke_vertex = [glslc,'--target-env=vulkan','-fshader-stage=vertex','-x','hlsl','-o','@OUTPUT@','@INPUT@']
glslc_invoke_fragment = [glslc,'--target-env=vulkan','-fshader-stage=fragment','-x','hlsl','-o','@OUTPUT@','@INPUT@']

custom_target('filter_vertex',output:'filter_vertex.spv',input:'src/filter_vertex.hlsl',command:glslc_invoke_vertex,install:true,install_dir:'.')
custom_target('filter_fragment',output:'filter_fragment.spv',input:'src/filter_fragment.hlsl',command:glslc_invoke_fragment,install:true,install_dir:'.')

executable('xwm',sources:src,dependencies:[xcb,vk],cpp_args:['-std=c++14'])

