project('xwm',['c','cpp'])

src = [
	'src/main.cpp',
	'src/config.cpp',
	'src/container.cpp',
	'src/backend.cpp',
	'src/compositor.cpp',
	'src/CompositorResource.cpp',
	'third/spirv_reflect.c'
]

inc = [
	include_directories('third')
]
	
xcb = [
	dependency('xcb'),
	dependency('xcb-keysyms'),
	dependency('xcb-xfixes'),
	dependency('xcb-damage'),
	dependency('xcb-composite'),
]

vk = [
	dependency('vulkan')
]

python = [
	dependency('python3'),
	dependency('boost',modules:['python3'])
]

glslc = find_program('glslc')
glslc_invoke_vertex = [glslc,'--target-env=vulkan','-fshader-stage=vertex','-x','hlsl','-DSHADER_STAGE_VS','-o','@OUTPUT@','@INPUT@']
glslc_invoke_geometry = [glslc,'--target-env=vulkan','-fshader-stage=geometry','-x','hlsl','-DSHADER_STAGE_GS','-o','@OUTPUT@','@INPUT@']
glslc_invoke_fragment = [glslc,'--target-env=vulkan','-fshader-stage=fragment','-x','hlsl','-DSHADER_STAGE_PS','-o','@OUTPUT@','@INPUT@']

custom_target('frame_vertex',output:'frame_vertex.spv',input:'shaders/frame.hlsl',command:glslc_invoke_vertex,install:true,install_dir:'.')
custom_target('frame_geometry',output:'frame_geometry.spv',input:'shaders/frame.hlsl',command:glslc_invoke_geometry,install:true,install_dir:'.')
custom_target('frame_fragment',output:'frame_fragment.spv',input:'shaders/frame.hlsl',command:glslc_invoke_fragment,install:true,install_dir:'.')

executable('xwm',sources:src,include_directories:inc,dependencies:[xcb,vk,python],cpp_args:['-std=c++17'])

