
#if defined(SHADER_STAGE_VS)

float4 main(float2 pos : POSITION) : SV_Position{
	return float4(pos,0,1.0f);
}

#elif defined(SHADER_STAGE_PS)

float4 main(float4 posh : SV_Position) : SV_Target{
	//
	return float4(1,0,0,1);
}

#endif

