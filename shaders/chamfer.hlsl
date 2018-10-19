
#define FLAGS_FOCUS 0x1
#define FLAGS_ADJACENT_LEFT 0x2
#define FLAGS_ADJACENT_RIGHT 0x4
#define FLAGS_ADJACENT_UP 0x8
#define FLAGS_ADJACENT_DOWN 0x16

//https://developer.nvidia.com/vulkan-shader-resource-binding
//https://www.khronos.org/assets/uploads/developers/library/2018-gdc-webgl-and-gltf/2-Vulkan-HLSL-There-and-Back-Again_Mar18.pdf
[[vk::push_constant]] cbuffer cb{
	float2 xy0;
	float2 xy1;
	float2 screen;
	float2 border;
	uint flags;
	float time;
};

