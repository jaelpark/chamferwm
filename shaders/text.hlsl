
struct PS_INPUT{
	float4 posh : SV_Position;
	float2 texc : TEXCOORD;
};

#if defined(SHADER_STAGE_VS)

[[vk::push_constant]] cbuffer cb{
	float2 xy0;
};

void main(float2 pos : POSITION, uint2 texc : TEXCOORD, out PS_INPUT output){
	output.posh = float4(xy0+pos,0,1.0f);
	output.texc = float2(texc.x,texc.y);
}

#elif defined(SHADER_STAGE_PS)

[[vk::binding(0)]] Texture2D<float> fontAtlas;

float4 main(PS_INPUT input) : SV_Target{
	//
	float c = fontAtlas.Load(float3(input.texc,0));
	return float4(0,0,0,c);
}

#endif
