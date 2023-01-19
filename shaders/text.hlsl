
struct PS_INPUT{
	float4 posh : SV_Position;
	float2 texc : TEXCOORD;
};

#if defined(SHADER_STAGE_VS)

[[vk::push_constant]] cbuffer cb{
	float2x2 transform;
	float2 xy0;
	float2 screen;
};

void main(float2 pos : POSITION, uint2 texc : TEXCOORD, out PS_INPUT output){
	output.posh = float4(xy0+2.0f*mul(transform,pos)/screen-1.0f,0,1.0f);
	output.texc = float2(texc.x,texc.y);
}

#elif defined(SHADER_STAGE_PS)

[[vk::binding(0)]] Texture2D<float> fontAtlas;

float4 main(PS_INPUT input) : SV_Target{
	float c = fontAtlas.Load(float3(input.texc+0.5f,0));
	return float4(0,0,0,c);
}

#endif

