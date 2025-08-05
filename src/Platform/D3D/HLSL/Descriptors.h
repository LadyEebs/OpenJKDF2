#include "VRAM.h"
#include "Color.h"

struct Descriptor
{
	uint2 resolution;
	uint  padding;
	uint  totalSize;
	uint  offset; // offset into vram
	uint  rowStride;
	uint2 format;
};

StructuredBuffer<Descriptor> Descriptors : register(DESCRIPTOR_REGISTER);

uint GetDescriptorBits(Descriptor desc)
{
	return ExtractBits(desc.format.x, 2, 6);
}

uint4 SampleTextureRGBA(Descriptor desc, int2 coord)
{
	uint colorMode = ExtractBits(desc.format.x, 0, 2);

	if (colorMode == 0)
	{
		uint pixel = Load8(coord.xy, int2(desc.offset, desc.rowStride));
		// todo: colormap
		return uint4(pixel,pixel,pixel,pixel);
	}

	uint bpp = ExtractBits(desc.format.x, 2, 6);
	
	uint pixel;
	if (bpp == 16)
		pixel = Load16(coord.xy, int2(desc.offset, desc.rowStride));
	else
		pixel = Load32(coord.xy, int2(desc.offset, desc.rowStride));
		
	// todo: unpack
	return uint4(pixel, pixel, pixel, pixel);
}

uint SampleTexture(Descriptor desc, int2 coord)
{
	uint colorMode = ExtractBits(desc.format.x, 0, 2);
	if (colorMode == 0)
		return Load8(coord.xy, int2(desc.offset, desc.rowStride));
	
	uint bpp = ExtractBits(desc.format.x, 2, 6);
	
	uint pixel;
	if (bpp == 16)
		pixel = Load16(coord.xy, int2(desc.offset, desc.rowStride));
	else
		pixel = Load32(coord.xy, int2(desc.offset, desc.rowStride));
		
	// todo: unpack + search LUT
	return pixel;
}
