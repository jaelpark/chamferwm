
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

[maxvertexcount(12)]
//void main(point GS_INTPUT input[1], inout TriangleStream<GS_OUTPUT> stream){
void main(point float2 posh[1], inout TriangleStream<GS_OUTPUT> stream){
	GS_OUTPUT output;
	
	float2 aspect = float2(1.0f,screen.x/screen.y);
	float2 borderWidth = border*aspect; //this results in borders half the gap size

	borderWidth *= 2.0f; //stretch to double to allow room for the effects

	//window shadow
	[unroll]
	for(uint i = 0; i < 4; ++i){
		output.posh = float4(vertices[i]+(2.0*vertexPositions[i]-1.0f)*4.0f*borderWidth,0,1);
		output.texc = vertexPositions[i];
		output.geomId = 0;
		stream.Append(output);
	}
	stream.RestartStrip();

	//window border
	[unroll]
	for(uint i = 0; i < 4; ++i){
		output.posh = float4(vertices[i]+(2.0*vertexPositions[i]-1.0f)*borderWidth,0,1);
		output.texc = vertexPositions[i];
		output.geomId = 1;
		stream.Append(output);
	}
	stream.RestartStrip();

	//window contents
	[unroll]
	for(uint i = 0; i < 4; ++i){
		output.posh = float4(vertices[i],0,1);
		output.texc = vertexPositions[i];
		output.geomId = 2;
		stream.Append(output);
	}
	stream.RestartStrip();

}

#elif defined(SHADER_STAGE_PS)

#include "chamfer.hlsl"
#define FLAGS_FOCUS_NEXT 0x2

[[vk::binding(0)]] Texture2D<float4> content;
//[[vk::binding(1)]] SamplerState sm;

float ChamferMap(float2 p, float2 b, float r){
	return length(max(abs(p)-b,0.0f))-r;
}

float4 main(float4 posh : SV_Position, float2 texc : TEXCOORD0, uint geomId : ID0) : SV_Target{
	float2 aspect = float2(1.0f,screen.x/screen.y);
	float2 borderWidth = border*aspect; //this results in borders half the gap size
	float borderScaling = screen.x/3840.0f;

	float2 p1 = screen*(0.25f*(xy0+xy1)+0.5f);

	float2 p = screen*(0.5f*xy0+0.5f);
	float2 m = screen*(0.5f*xy1+0.5f);
	float2 d1 = m-p; //d1: extent of the window

	float4 c = float4(0.0f,0.0f,0.0f,1.0f);
	if(geomId == 0){
		float2 q = posh.xy-p1;
		if(ChamferMap(q,0.5f*d1-40.0f*borderScaling,40.0f*borderScaling) < 0.0f){
			discard;
			return c;
		}

		//float constScaling = screen.x/(d1.x+2.0f*screen.x*borderWidth.x);
		//float d = ChamferMap(q/(1.0f+border.x*constScaling),0.5f*d1-50.0f*borderScaling,75.0f*borderScaling)*1.015f; //TODO: use different xy0, xy1 for shadows
		float2 d2 = d1+50.0f*borderScaling;
		float d = ChamferMap(q,0.5f*d2-50.0f*borderScaling,75.0f*borderScaling);
		return float4(0.0f,0.0f,0.0f,0.9f*saturate(-d/30.0f));

	}else
	if(geomId == 1){
		float2 q = posh.xy-p1;
		if(ChamferMap(q,0.5f*d1-50.0f*borderScaling,75.0f*borderScaling) > 0.0f){
			discard;
			return c;
		}
		if(ChamferMap(q,0.5f*d1-(40.0f-2.0f)*borderScaling,40.0f*borderScaling) < 0.0f){
			discard; //remove background to allow for transparency effects
			return c;
		}
		if(flags & FLAGS_FOCUS)
			//dashed line around focus
			if((any(posh > p1-0.5f*d1 && posh < p1+0.5f*d1 && fmod(floor(posh/(50.0f*borderScaling)),3.0f) < 0.5f) &&
				any(posh < p1-0.5f*d1-0.25f*screen*borderWidth || posh > p1+0.5f*d1+0.25f*screen*borderWidth)))
				c.xyz = float3(1.0f,0.6f,0.33f);

		if(flags & FLAGS_FOCUS_NEXT)
			if((any(posh > p1-0.5f*d1 && posh < p1+0.5f*d1 && fmod(floor(posh/(50.0f*borderScaling)),3.0f) < 0.5f) &&
				any(posh < p1-0.5f*d1-0.25f*screen*borderWidth || posh > p1+0.5f*d1+0.25f*screen*borderWidth)))
				c.xyz = float3(0.957f,0.910f,0.824f);

	}else{
		if(ChamferMap(posh.xy-p1,0.5f*d1-40.0f*borderScaling,40.0f*borderScaling) > 0.0f){
			return c;
			discard;
		}
		//float4 c = content.Sample(sm,texc);
		float2 r = posh.xy-p;
		c = content.Load(float3(r,0)); //p already has the 0.5f offset
		//^^returns black when out of bounds (border)
	}

	return c;
}

#endif

