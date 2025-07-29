#define VRAM_REGISTER u0

#include"VRAM.h"

cbuffer CopyInfo : register( b0 )
{
	int2 SrcAddressAndStride;
	int2 DstAddressAndStride;
	
	int2 SrcSize;
	int2 DstSize;
	
	int4 SrcRect;
	int4 DstRect;
	
	int  Flags;
	int  Pad0, Pad1, Pad2;
};

[numthreads(256, 1, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
	int2 coord = dispatchThreadID.xy;	
	if (any(coord.xy >= DstRect.zw))
		return;
		
	int2 srcCoord = coord.xy + SrcRect.xy;
	int2 dstCoord = coord.xy + DstRect.xy;
	if (any(srcCoord.xy >= SrcSize.xy || dstCoord.xy >= DstSize.xy))
		return;
		
	// todo: probably good to have format conversion here?
	uint pixel = Load8(srcCoord, SrcAddressAndStride);
	if (!pixel && (Flags & 1)) // Alpha
		return;
	
	Store8(pixel, dstCoord, DstAddressAndStride);
}
