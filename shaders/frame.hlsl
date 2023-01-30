
#if defined(SHADER_STAGE_VS)

float2 main(uint x : SV_VertexID) : POSITION0{
	return float2(0,0);
}

#elif defined(SHADER_STAGE_GS)

#include "chamfer.hlsl"

struct GS_OUTPUT{
	float4 posh : SV_Position;
	float2 texc : TEXCOORD;
};

const float2 vertexPositions[4] = {
	float2(0.0f,0.0f),
	float2(1.0f,0.0f),
	float2(0.0f,1.0f),
	float2(1.0f,1.0f)
};

static float2 titlePadAspect = 2.0f*titlePad*float2(1.0f,screen.x/screen.y);
static float2 xy0_1 = xy0+min(titlePadAspect,0.0f);
static float2 xy1_1 = xy1+max(titlePadAspect,0.0f);
const float2 vertices[4] = {
	xy0_1,
	float2(xy1_1.x,xy0_1.y),
	float2(xy0_1.x,xy1_1.y),
	xy1_1
};

[maxvertexcount(4)]
//void main(point GS_INTPUT input[1], inout TriangleStream<GS_OUTPUT> stream){
void main(point float2 posh[1], inout TriangleStream<GS_OUTPUT> stream){
	GS_OUTPUT output;
	
	float2 aspect = float2(1.0f,screen.x/screen.y);
	float2 marginWidth = margin*aspect; //this results in borders half the gap size

	//stretch to make room for the effects (border + shadow) - also includes screen space 2.0f
	marginWidth *= 8.0f;

	//expand the vertex into a quad
	[unroll]
	for(uint i = 0; i < 4; ++i){
		output.posh = float4(vertices[i]+(2.0*vertexPositions[i]-1.0f)*marginWidth,0,1);
		output.texc = vertexPositions[i];
		stream.Append(output);
	}
	stream.RestartStrip();
}

#elif defined(SHADER_STAGE_PS)

#include "chamfer.hlsl"

#ifndef STOCK_FRAME_STYLE
#define STOCK_FRAME_STYLE 1 //select between two stock styles (1: chamfered edges, other: basic rectangle borders)
#endif

#ifndef DRAW_SHADOW
#define DRAW_SHADOW 1 //set 1 to draw shadow
#endif

#ifndef DRAW_BORDER
#define DRAW_BORDER 1
#endif

#ifndef ENABLE_TITLE
#define ENABLE_TITLE 1
#endif

#define BORDER_RADIUS 0.02f
#define BORDER_THICKNESS 0.008f

const float4 borderColor = float4(0.07f,0.07f,0.07f,1.0f);
const float4 focusColor = float4(1.0f,0.6f,0.33f,1.0f);
const float4 titleBackground[2] = {float4(0.4f,0.4f,0.4f,1.0f),float4(0.5,0.5,0.5,1.0f)}; //second value for alternating stack tabs
const float4 taskSelectColor = float4(0.957f,0.910f,0.824f,1.0f);

[[vk::binding(0)]] Texture2D<float4> content;
//[[vk::binding(1)]] SamplerState sm;

//signed distance field of a rectangle
float RectangleMap(float2 p, float2 b){
	float2 d = abs(p)-b;
	return length(max(d,0.0f))+min(max(d.x,d.y),0.0f);
}

//signed distance field of a chamfered rectangle
float ChamferMap(float2 p, float2 b, float r){
	float2 d = abs(p)-b;
	return length(max(d,0.0f))-r+min(max(d.x,d.y),0.0f);
}

float4 main(float4 posh : SV_Position, float2 texc : TEXCOORD) : SV_Target{
	float2 aspect = float2(1.0f,screen.x/screen.y);

	float2 borderScalingScr = screen*aspect;

	float2 titlePadAspect = 2.0f*titlePad*aspect;
	float2 xy0_1 = xy0+min(titlePadAspect,0.0f);
	float2 xy1_1 = xy1+max(titlePadAspect,0.0f);

	float2 a = screen*(0.5f*xy0_1+0.5f); //top-left corner in pixels
	float2 b = screen*(0.5f*xy1_1+0.5f); //bottom-right corner in pixels
	float2 center = 0.5f*(a+b); //center location in pixels
	float2 d1 = b-a; //d1: pixel extent of the window

	float2 q = posh.xy-center; //pixel offset from center of the window

#if STOCK_FRAME_STYLE == 1
	// ----- frame 1: rounded corners (demo frame) -------------------

	float sr1 = ChamferMap(q,0.5f*d1-BORDER_RADIUS*borderScalingScr.x,(BORDER_RADIUS+DRAW_BORDER*BORDER_THICKNESS)*borderScalingScr.x); //with border (thicknes determined here)
	if(sr1 > -1.0f){
		//shadow region
#if DRAW_SHADOW
		if(stackIndex == 0) //only first in stack casts a shadow
			return float4(0.0f,0.0f,0.0f,0.7f*saturate(1.0f-sr1/(0.0078f*borderScalingScr.x)));
		else{
#else
		{
#endif
			discard;
			return 0.0f;
		}
	}
#if DRAW_BORDER
	//radius extends from the rectangle
	float br1 = ChamferMap(q,0.5f*d1-BORDER_RADIUS*borderScalingScr.x,BORDER_RADIUS*borderScalingScr.x);
	if(br1 > -1.0f){
		//border region
		if(flags & FLAGS_FOCUS)
			//dashed line around focus
			if((any(posh > center-0.5f*d1 && posh < center+0.5f*d1 && fmod(floor(posh/(0.0130f*borderScalingScr.x)),3.0f) < 0.5f) &&
				any(posh < center-0.5f*d1-0.6f*BORDER_THICKNESS*borderScalingScr || posh > center+0.5f*d1+0.6f*BORDER_THICKNESS*borderScalingScr))) //0.0037f
				return focusColor;

		if(flags & FLAGS_FOCUS_NEXT)
			if((any(posh > center-0.5f*d1 && posh < center+0.5f*d1 && fmod(floor(posh/(0.0130f*borderScalingScr.x)),3.0f) < 0.5f) &&
				any(posh < center-0.5f*d1-0.6f*BORDER_THICKNESS*borderScalingScr || posh > center+0.5f*d1+0.6f*BORDER_THICKNESS*borderScalingScr)))
				return taskSelectColor;

		return borderColor;
	}
#endif //DRAW_BORDER
#else //STOCK_FRAME_STYLE
	// ----- frame 0: basic rectangular ------------------------------

	float sr1 = RectangleMap(q,0.5f*d1+DRAW_BORDER*BORDER_THICKNESS*borderScalingScr.x);
	if(sr1 > -1.0f){
		//shadow region
#if DRAW_SHADOW
		if(stackIndex == 0) //only first in stack casts a shadow
			return float4(0.0f,0.0f,0.0f,0.9f*saturate(1.0f-sr1/(0.0078f*borderScalingScr.x)));
		else{
#else
		{
#endif
			discard;
			return 0.0f;
		}
	}
#if DRAW_BORDER
	float br1 = RectangleMap(q,0.5f*d1);
	if(br1 > -1.0f){
		//border region
		if(flags & FLAGS_FOCUS)
			return focusColor;
		if(flags & FLAGS_FOCUS_NEXT)
			return taskSelectColor;

		return borderColor;
	}
#endif //DRAW_BORDER
#endif //STOCK_FRAME_STYLE

	float2 a_content = screen*(0.5f*xy0+0.5f); //top-left corner in pixels, content area
#if ENABLE_TITLE
	float2 b_content = screen*(0.5f*xy1+0.5f); //bottom-right corner in pixels, content area
	if(any(posh.xy < a_content) || any(posh.xy > b_content)){ //title region
		bool1 tb = abs(titlePad.x) < abs(titlePad.y); //check whether the title bar is horizontal or vertical
		bool2 sb = posh < (a+(b-a)*titleSpan.x) || posh > (a+(b-a)*titleSpan.y);
		if((tb && sb[0]) || (!tb && sb[1]))
			discard; //clear the region to show the titles of the clients stacked under
		if(flags & FLAGS_FOCUS)
			return focusColor;
		return titleBackground[stackIndex%2];
	}
#endif
	//content region
	if(flags & FLAGS_CONTAINER_FOCUS){
		float4 c = content.Load(float3(posh.xy-a_content,0));
		return c;
	}
	discard;
	return 0.0f;
}

#endif

