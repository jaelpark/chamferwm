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
	dependency('xcb-ewmh'),
	dependency('xcb-randr'),
	dependency('xcb-util')
]

vk = [
	dependency('vulkan'),
	dependency('glm')
	#dependency('gbm')
]

python = [
	dependency('python3'),
	dependency('python3-embed'), #python 3.8
	dependency('boost',modules:['system','filesystem','python3'])
]

ft = [
	dependency('freetype2'),
	dependency('fontconfig'),
	dependency('harfbuzz')
]

glslc = find_program('glslc')
spirv_opt = find_program('spirv-opt') #optimization has to be done separately as a post-processing step, since glslc removes the reflection info

#glslc_invoke_vertex = [glslc,'--target-env=vulkan','-fhlsl_functionality1','-fshader-stage=vertex','-O','-g','-x','hlsl','-DSHADER_STAGE_VS','-o','@OUTPUT@','@INPUT@']
#glslc_invoke_geometry = [glslc,'--target-env=vulkan','-fhlsl_functionality1','-fshader-stage=geometry','-O','-g','-x','hlsl','-DSHADER_STAGE_GS','-o','@OUTPUT@','@INPUT@']
#glslc_invoke_fragment = [glslc,'--target-env=vulkan','-fhlsl_functionality1','-fshader-stage=fragment','-O','-g','-x','hlsl','-DSHADER_STAGE_PS','-o','@OUTPUT@','@INPUT@']
glslc_args_vertex = ['--target-env=vulkan','-fhlsl_functionality1','-fshader-stage=vertex','-x','hlsl','-DSHADER_STAGE_VS','-o','@OUTPUT@','@INPUT@']
glslc_args_geometry = ['--target-env=vulkan','-fhlsl_functionality1','-fshader-stage=geometry','-x','hlsl','-DSHADER_STAGE_GS','-o','@OUTPUT@','@INPUT@']
glslc_args_fragment = ['--target-env=vulkan','-fhlsl_functionality1','-fshader-stage=fragment','-x','hlsl','-DSHADER_STAGE_PS','@EXTRA_ARGS@','-o','@OUTPUT@','@INPUT@']
invoke_spirv_opt = [spirv_opt,'-O','-o','@OUTPUT@','@INPUT@']

#custom_target('default_vertex',output:'default_vertex.spv',input:'shaders/default.hlsl',command:glslc_invoke_vertex,install:true,install_dir:'.')
#custom_target('default_geometry',output:'default_geometry.spv',input:'shaders/default.hlsl',command:glslc_invoke_geometry,install:true,install_dir:'.')
#custom_target('default_fragment',output:'default_fragment.spv',input:'shaders/default.hlsl',command:glslc_invoke_fragment,install:true,install_dir:'.')
#custom_target('solid_fragment',output:'solid_fragment.spv',input:'shaders/solid.hlsl',command:glslc_invoke_fragment,install:true,install_dir:'.')
#custom_target('frame_vertex',output:'frame_vertex.spv',input:'shaders/frame.hlsl',command:glslc_invoke_vertex,install:true,install_dir:'.')
#custom_target('frame_geometry',output:'frame_geometry.spv',input:'shaders/frame.hlsl',command:glslc_invoke_geometry,install:true,install_dir:'.')
#obj = custom_target('frame_fragment',output:'frame_fragment.spv',input:'shaders/frame.hlsl',command:glslc_invoke_fragment,install:false)#,install:true,install_dir:'.')

gen_vertex = generator(glslc,output:'@BASENAME@_vertex_unopt.spv',arguments:glslc_args_vertex)
gen_geometry = generator(glslc,output:'@BASENAME@_geometry_unopt.spv',arguments:glslc_args_geometry)
gen_fragment = generator(glslc,output:'@BASENAME@_fragment_unopt.spv',arguments:glslc_args_fragment)

obj = gen_vertex.process(['shaders/default.hlsl'])
custom_target('default_vertex',output:'default_vertex.spv',input:obj,command:invoke_spirv_opt,install:true,install_dir:'.')

obj = gen_vertex.process(['shaders/frame.hlsl'])
custom_target('frame_vertex',output:'frame_vertex.spv',input:obj,command:invoke_spirv_opt,install:true,install_dir:'.')

obj = gen_vertex.process(['shaders/text.hlsl'])
custom_target('text_vertex',output:'text_vertex.spv',input:obj,command:invoke_spirv_opt,install:true,install_dir:'.')

obj = gen_geometry.process(['shaders/default.hlsl'])
custom_target('default_geometry',output:'default_geometry.spv',input:obj,command:invoke_spirv_opt,install:true,install_dir:'.')

obj = gen_geometry.process(['shaders/frame.hlsl'])
custom_target('frame_geometry',output:'frame_geometry.spv',input:obj,command:invoke_spirv_opt,install:true,install_dir:'.')

obj = gen_fragment.process(['shaders/default.hlsl'])
custom_target('default_fragment',output:'default_fragment.spv',input:obj,command:invoke_spirv_opt,install:true,install_dir:'.')

foreach name,args : {
		'fragment':['-DSTOCK_FRAME_STYLE=1'], #default "demo" decoration with border and rounded corners
		'fragment_basic':['-DSTOCK_FRAME_STYLE=0'], #rectangular borders, no bling
        'fragment_ext':['-DSTOCK_FRAME_STYLE=1','-DDRAW_BORDER=1','-DENABLE_TITLE=0','-DDRAW_SHADOW=1'], #default rounded corner style for external wms
        'fragment_ext_basic':['-DSTOCK_FRAME_STYLE=0','-DDRAW_BORDER=0','-DENABLE_TITLE=0','-DDRAW_SHADOW=1'] #simple compatible style for external wms, only draw shadows
	}
	obj = generator(glslc,output:'@BASENAME@_'+name+'_unopt.spv',arguments:glslc_args_fragment).process(['shaders/frame.hlsl'],extra_args:args)
	custom_target('frame_'+name,output:'frame_'+name+'.spv',input:obj,command:invoke_spirv_opt,install:true,install_dir:'.')
endforeach

#obj = gen_fragment.process(['shaders/frame.hlsl'],extra_args:['-DSTOCK_FRAME_STYLE=1'])
#custom_target('frame_fragment',output:'frame_fragment.spv',input:obj,command:invoke_spirv_opt,install:true,install_dir:'.')

obj = gen_fragment.process(['shaders/text.hlsl'])
custom_target('text_fragment',output:'text_fragment.spv',input:obj,command:invoke_spirv_opt,install:true,install_dir:'.')

obj = gen_fragment.process(['shaders/solid.hlsl'])
custom_target('solid_fragment',output:'solid_fragment.spv',input:obj,command:invoke_spirv_opt,install:true,install_dir:'.')

executable('chamfer',sources:src,include_directories:inc,dependencies:[xcb,vk,python,ft],cpp_args:['-std=c++17','-Wno-reorder','-Wno-error=narrowing'])

