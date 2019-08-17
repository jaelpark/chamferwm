
#define FLAGS_FOCUS 0x1

[[vk::push_constant]] cbuffer cb{
	float2 xy0;
	float2 xy1;
	float2 screen;
	float2 margin;
	uint flags;
	float time;
};

