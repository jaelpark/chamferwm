
#if defined(SHADER_STAGE_VS)

float2 main(uint x : SV_VertexID) : POSITION0{
	return float2(0,0);
}

#elif defined(SHADER_STAGE_GS)

#include "chamfer.hlsl"

const float2 vertexPositions[4] = {
	float2(0.0f,0.0f),
	float2(1.0f,0.0f),
	float2(0.0f,1.0f),
	float2(1.0f,1.0f)
};

struct GS_OUTPUT{
	float4 posh : SV_Position;
};

const float2 vertices[4] = {
	xy0,
	float2(xy1.x,xy0.y),
	float2(xy0.x,xy1.y),
	xy1
};

[maxvertexcount(4)]
void main(point float2 posh[1], inout TriangleStream<GS_OUTPUT> stream){
	GS_OUTPUT output;
	
	//window contents
	[unroll]
	for(uint i = 0; i < 4; ++i){
		output.posh = float4(vertices[i],0,1);
		stream.Append(output);
	}
	stream.RestartStrip();

}

#elif defined(SHADER_STAGE_PS)

#include "chamfer.hlsl"

[[vk::binding(0)]] Texture2D<float4> content;

float4 main(float4 posh : SV_Position, float2 texc : TEXCOORD0, uint geomId : ID0) : SV_Target{
	float2 p = screen*(0.5f*xy0+0.5f);
	float2 r = posh.xy-p;
	float4 c = content.Load(float3(r,0)); //p already has the 0.5f offset

	return c;
}

#endif

