
struct PS_INPUT{
	float4 posh : SV_Position;
	uint2 texc : TEXCOORD;
};

#if defined(SHADER_STAGE_VS)

void main(float2 pos : POSITION, uint2 texc : TEXCOORD, out PS_INPUT output){
	output.posh = float4(pos,0,1.0f);
	output.texc = texc;
}

#elif defined(SHADER_STAGE_PS)

[[vk::binding(0)]] Texture2D<float> fontAtlas;

float4 main(PS_INPUT input) : SV_Target{
	//
	float c = fontAtlas.Load(float3(input.posh.xy-80,0));
	return float4(c,c,c,1);
	//return float4(1,c,0,1);
	//return float4(saturate(input.posh.xy/255),0,1);
}

#endif

