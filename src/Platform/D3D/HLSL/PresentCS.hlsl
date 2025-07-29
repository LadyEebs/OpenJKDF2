#define VRAM_READ_ONLY
#define VRAM_REGISTER t0

#include"VRAM.h"

cbuffer PresentInfo : register( b0 )
{
	int2 SrcAddressAndStride;
	int2 SrcSize;
	int2 DstSize;
	int2 Padding;
	int4 DstRect;
};

RWTexture2D<float4> BackBuffer : register(u0);

Buffer<float4> Palette : register(t1);

[numthreads(256, 1, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
	uint2 coord = dispatchThreadID.xy;
	
	if (any(coord.xy >= DstRect.zw))
		return;
	
	uint2 srcCoord = (coord.xy * SrcSize.xy) / DstRect.zw;
	uint pixel = Load8(srcCoord.xy, SrcAddressAndStride);
	BackBuffer[coord.xy + DstRect.xy] = Palette[pixel];
}
