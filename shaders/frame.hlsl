
#if defined(SHADER_STAGE_VS)

float2 main(uint x : SV_VertexID) : POSITION0{
	return float2(0,0);
}

#elif defined(SHADER_STAGE_GS)

#include "chamfer.hlsl"

struct GS_OUTPUT{
	float4 posh : SV_Position;
	float2 texc : TEXCOORD;
	//uint geomId : ID;
};

const float2 vertexPositions[4] = {
	float2(0.0f,0.0f),
	float2(1.0f,0.0f),
	float2(0.0f,1.0f),
	float2(1.0f,1.0f)
};

const float2 vertices[4] = {
	xy0,
	float2(xy1.x,xy0.y),
	float2(xy0.x,xy1.y),
	xy1
};

[maxvertexcount(4)]
//void main(point GS_INTPUT input[1], inout TriangleStream<GS_OUTPUT> stream){
void main(point float2 posh[1], inout TriangleStream<GS_OUTPUT> stream){
	GS_OUTPUT output;
	
	float2 aspect = float2(1.0f,screen.x/screen.y);
	float2 marginWidth = margin*aspect; //this results in borders half the gap size

	marginWidth *= 8.0f; //stretch to make room for the effects (border + shadow)

	//expand the vertex into a quad
	[unroll]
	for(uint i = 0; i < 4; ++i){
		output.posh = float4(vertices[i]+(2.0*vertexPositions[i]-1.0f)*marginWidth,0,1);
		output.texc = vertexPositions[i];
		//output.geomId = 0;
		stream.Append(output);
	}
	stream.RestartStrip();
}

#elif defined(SHADER_STAGE_PS)

#include "chamfer.hlsl"

const float borderScaling = 1.0f;
const float4 borderColor = float4(0.0f,0.0f,0.0f,1.0f);
const float4 focusColor = float4(1.0f,0.6f,0.33f,1.0f);
const float4 taskSelectColor = float4(0.957f,0.910f,0.824f,1.0f);

[[vk::binding(0)]] Texture2D<float4> content;
//[[vk::binding(1)]] SamplerState sm;

//signed distance field of a chamfered rectangle
float ChamferMap(float2 p, float2 b, float r){
	float2 d = abs(p)-b;
	return length(max(d,0.0f))-r+min(max(d.x,d.y),0.0f);
}

//float4 main(float4 posh : SV_Position, float2 texc : TEXCOORD0, uint geomId : ID0) : SV_Target{
float4 main(float4 posh : SV_Position, float2 texc : TEXCOORD0) : SV_Target{
	float2 aspect = float2(1.0f,screen.x/screen.y);

	float2 borderScalingScr = borderScaling*screen*aspect;

	float2 a = screen*(0.5f*xy0+0.5f); //top-left corner in pixels
	float2 b = screen*(0.5f*xy1+0.5f); //bottom-right corner in pixels
	float2 center = 0.5f*(a+b); //center location in pixels
	float2 d1 = b-a; //d1: pixel extent of the window

	float2 q = posh.xy-center;
	if(ChamferMap(q,0.5f*d1-0.0130f*borderScalingScr.x,0.0195f*borderScalingScr.x) > 0.0f){
		//shadow region
		float d = ChamferMap(q,0.5f*d1-0.0065f*borderScalingScr.x,0.0195f*borderScalingScr.x);
		return float4(0.0f,0.0f,0.0f,0.9f*saturate(-d/30.0f));
	}
	if(ChamferMap(q,0.5f*d1-0.0104f*borderScalingScr.x,0.0104f*borderScalingScr.x) > -0.5f){
		//border region
		if(flags & FLAGS_FOCUS)
			//dashed line around focus
			if((any(posh > center-0.5f*d1 && posh < center+0.5f*d1 && fmod(floor(posh/(0.0130f*borderScalingScr.x)),3.0f) < 0.5f) &&
				any(posh < center-0.5f*d1-0.0037f*borderScalingScr || posh > center+0.5f*d1+0.0037f*borderScalingScr)))
				return focusColor;

		if(flags & FLAGS_FOCUS_NEXT)
			if((any(posh > center-0.5f*d1 && posh < center+0.5f*d1 && fmod(floor(posh/(0.0130f*borderScalingScr.x)),3.0f) < 0.5f) &&
				any(posh < center-0.5f*d1-0.0037f*borderScalingScr || posh > center+0.5f*d1+0.0037f*borderScalingScr)))
				return taskSelectColor;

		return borderColor;
	}

	//content region
	float4 c = content.Load(float3(posh.xy-a,0));

	return c;
}

#endif

