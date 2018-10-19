
#if defined(SHADER_STAGE_VS)

/*void main(uint x : SV_VertexID, out float4 posh : SV_Position, out float2 texc : TEXCOORD0){
	texc = float2((x<<1)&2,x&2);
	posh = float4(texc*float2(2,-2)+float2(-1,1),0,1);
}*/

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
	float2 texc : TEXCOORD;
	uint geomId : ID;
};

const float2 vertices[4] = {
	xy0,
	float2(xy1.x,xy0.y),
	float2(xy0.x,xy1.y),
	xy1
};

[maxvertexcount(8)]
//void main(point GS_INTPUT input[1], inout TriangleStream<GS_OUTPUT> stream){
void main(point float2 posh[1], inout TriangleStream<GS_OUTPUT> stream){
	GS_OUTPUT output;
	
	float2 aspect = float2(1.0f,screen.x/screen.y);
	float2 borderWidth = border*aspect; //this results in borders half the gap size

	//todo: bitmask as adjacency information

	[unroll]
	for(uint i = 0; i < 4; ++i){
		output.posh = float4(vertices[i]+(2.0*vertexPositions[i]-1.0f)*borderWidth,0,1);
		output.texc = vertexPositions[i];
		output.geomId = 0;
		stream.Append(output);
	}
	stream.RestartStrip();

	[unroll]
	for(uint i = 0; i < 4; ++i){
		output.posh = float4(vertices[i],0,1);
		output.texc = vertexPositions[i]; //TODO: should be adjusted according to dimensions of the box. Push constants need the maximum known border width
		output.geomId = 1;
		stream.Append(output);
	}
	stream.RestartStrip();
}

#elif defined(SHADER_STAGE_PS)

#include "chamfer.hlsl"

[[vk::binding(0)]] Texture2D<float4> content;
//[[vk::binding(1)]] SamplerState sm;

//TODO: create chamfer with ndc coords and sdf transformation
float4 main(float4 posh : SV_Position, float2 texc : TEXCOORD0, uint geomId : ID0) : SV_Target{
	//float4 c = content.Sample(sm,texc);
	float2 p = screen*(0.5f*xy0+0.5f);
	float4 c = content.Load(float3(posh.xy-p,0)); //p already has the 0.5f offset
	//^^returns black when out of bounds (border)
	if(flags & FLAGS_FOCUS && geomId == 0)
		c.xyz = float3(1,0,0);
	c.w = 1.0f;
	return c;
#if 0
	float2 s = saturate(abs(2.0f*(xy1-xy0))); //reference
	float2 ct = 2.0f*texc-1.0f;
	ct = s*ct+sign(ct)*(1-s); //TODO: aspect ratio check, 1.0f/0.5f
	if(length(max(abs(ct)-1.2*float2(0.5,0.5),0.0f))-0.4f > 0.0f)
		discard;
	/*float2 ct = 0.5f*(xy0+xy1);
	if(length(max(abs(posh.xy+xy0)-float2(0.5,0.5),0.0f))-0.4f > 0.0f)
		discard;*/
	
	return float4(1,1,1,1);
#endif
}

#endif

