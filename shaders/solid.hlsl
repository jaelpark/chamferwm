
#if defined(SHADER_STAGE_PS)

float4 main(float4 posh : SV_Position, float2 texc : TEXCOORD0) : SV_Target{
	return float4(1,1,1,1);
}

#endif

