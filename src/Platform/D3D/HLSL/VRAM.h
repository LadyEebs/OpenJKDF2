static const uint VRAM_SIZE = 64 * 1024 * 1024;
static const uint VRAM_MASK = ((VRAM_SIZE >> 1) - 1);

#ifndef VRAM_READ_ONLY
RWBuffer<uint> VRAM : register(VRAM_REGISTER);
#else
Buffer<uint> VRAM : register(VRAM_REGISTER);
#endif

uint CalculateAddress(int x, int y, int offset, int stride)
{
	return (x + y * stride + offset) & VRAM_MASK;
}

uint Load8(int2 coord, int2 offset)
{
	return VRAM[CalculateAddress(coord.x, coord.y, offset.x, offset.y)] & 0xFF;
}

uint Load16(int2 coord, int2 offset)
{
	coord.x <<= 1;
	
	uint texel;
	texel  = (VRAM[CalculateAddress(coord.x + 0, coord.y, offset.x, offset.y)] & 0xFF) <<  0;
	texel |= (VRAM[CalculateAddress(coord.x + 1, coord.y, offset.x, offset.y)] & 0xFF) <<  8;
	return texel;
}

uint Load32(int2 coord, int2 offset)
{
	coord.x <<= 2;
	
	uint texel;
	texel  = (VRAM[CalculateAddress(coord.x + 0, coord.y, offset.x, offset.y)] & 0xFF) <<  0;
	texel |= (VRAM[CalculateAddress(coord.x + 1, coord.y, offset.x, offset.y)] & 0xFF) <<  8;
	texel |= (VRAM[CalculateAddress(coord.x + 1, coord.y, offset.x, offset.y)] & 0xFF) << 16;
	texel |= (VRAM[CalculateAddress(coord.x + 1, coord.y, offset.x, offset.y)] & 0xFF) << 24;
	return texel;
}

#ifndef VRAM_READ_ONLY
void Store8(uint value, int2 coord, int2 offset)
{
	VRAM[CalculateAddress(coord.x, coord.y, offset.x, offset.y)] = value & 0xFF;
}

void Store16(uint value, int2 coord, int2 offset)
{
	coord.x <<= 1;
	VRAM[CalculateAddress(coord.x + 0, coord.y, offset.x, offset.y)] = (value >> 0) & 0xFF;
	VRAM[CalculateAddress(coord.x + 1, coord.y, offset.x, offset.y)] = (value >> 8) & 0xFF;
}

void Store32(uint value, int2 coord, int2 offset)
{
	coord.x <<= 2;
	VRAM[CalculateAddress(coord.x + 0, coord.y, offset.x, offset.y)] = (value >>  0) & 0xFF;
	VRAM[CalculateAddress(coord.x + 1, coord.y, offset.x, offset.y)] = (value >>  8) & 0xFF;
	VRAM[CalculateAddress(coord.x + 2, coord.y, offset.x, offset.y)] = (value >> 16) & 0xFF;
	VRAM[CalculateAddress(coord.x + 3, coord.y, offset.x, offset.y)] = (value >> 24) & 0xFF;
}
#endif