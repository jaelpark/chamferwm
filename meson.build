project('chamferwm',['c','cpp'])

src = [
	'src/main.cpp',
	'src/config.cpp',
	'src/container.cpp',
	'src/backend.cpp',
	'src/compositor.cpp',
	'src/CompositorResource.cpp',
	'src/CompositorFont.cpp',
	'third/spirv_reflect/spirv_reflect.c'
]

inc = [
	include_directories('third/spirv_reflect'),
	include_directories('third/args')
]
	
xcb = [
	dependency('xcb'),
	dependency('xcb-keysyms'),
	dependency('xcb-cursor'),
	dependency('xcb-xfixes'),
	dependency('xcb-damage'),
	dependency('xcb-shm'),
	dependency('xcb-dri3'),
	dependency('xcb-composite'),
	dependency('xcb-icccm'),
	dependency('xcb-ewmh')
]

vk = [
	dependency('vulkan'),
	dependency('gbm')
]

python = [
	dependency('python3'),
	dependency('python3-embed'), #python 3.8
	dependency('boost',modules:['system','filesystem','python3'])
]

ft = [
	dependency('freetype2'),
	dependency('harfbuzz')
]

glslc = find_program('glslc')
glslc_invoke_vertex = [glslc,'--target-env=vulkan','-fhlsl_functionality1','-fshader-stage=vertex','-O','-x','hlsl','-DSHADER_STAGE_VS','-o','@OUTPUT@','@INPUT@']
glslc_invoke_geometry = [glslc,'--target-env=vulkan','-fhlsl_functionality1','-fshader-stage=geometry','-O','-x','hlsl','-DSHADER_STAGE_GS','-o','@OUTPUT@','@INPUT@']
glslc_invoke_fragment = [glslc,'--target-env=vulkan','-fhlsl_functionality1','-fshader-stage=fragment','-O','-x','hlsl','-DSHADER_STAGE_PS','-o','@OUTPUT@','@INPUT@']

custom_target('default_vertex',output:'default_vertex.spv',input:'shaders/default.hlsl',command:glslc_invoke_vertex,install:true,install_dir:'.')
custom_target('default_geometry',output:'default_geometry.spv',input:'shaders/default.hlsl',command:glslc_invoke_geometry,install:true,install_dir:'.')
custom_target('default_fragment',output:'default_fragment.spv',input:'shaders/default.hlsl',command:glslc_invoke_fragment,install:true,install_dir:'.')
custom_target('solid_fragment',output:'solid_fragment.spv',input:'shaders/solid.hlsl',command:glslc_invoke_fragment,install:true,install_dir:'.')
custom_target('frame_vertex',output:'frame_vertex.spv',input:'shaders/frame.hlsl',command:glslc_invoke_vertex,install:true,install_dir:'.')
custom_target('frame_geometry',output:'frame_geometry.spv',input:'shaders/frame.hlsl',command:glslc_invoke_geometry,install:true,install_dir:'.')
custom_target('frame_fragment',output:'frame_fragment.spv',input:'shaders/frame.hlsl',command:glslc_invoke_fragment,install:true,install_dir:'.')

executable('chamfer',sources:src,include_directories:inc,dependencies:[xcb,vk,python,ft],cpp_args:['-std=c++17','-Wno-reorder'])

