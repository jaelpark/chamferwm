
#define FLAGS_FOCUS 0x1
#define FLAGS_FOCUS_NEXT 0x2

[[vk::push_constant]] cbuffer cb{
	float2 xy0; //normalized top-left corner location
	float2 xy1; //normalized bottom-right corner location
	float2 screen; //screen pixel dimensions
	float2 margin; //normalized gap margin in x and y directions
	uint flags; //flags such as whether client is focused
	float time; //time in seconds since client creation
};

