#define VRAM_REGISTER u0
#define DESCRIPTOR_REGISTER t0

#include"VRAM.h"

cbuffer FillInfo : register( b0 )
{
	int2 SrcAddressAndStride;
	int2 SrcSize;
	int4 SrcRect;
	int  Fill;
	int  Pad0, Pad1, Pad2;
};

[numthreads(256, 1, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
	if (any(dispatchThreadID.xy >= SrcRect.zw))
		return;
	
	int2 coord = dispatchThreadID.xy + SrcRect.xy;
	if (any(coord.xy >= SrcSize.xy))
		return;
		
	int bpp = 8;
	switch (bpp)
	{
	case 8:
		Store8(Fill & 0xFF, coord, SrcAddressAndStride);
		return;

	case 16:
		Store16(Fill & 0xFFFF, coord, SrcAddressAndStride);
		return;

	case 32:
		Store32(Fill, coord, SrcAddressAndStride);
		return;

	default:
		return;
	}		
}
