ByteAddressBuffer aPrimitiveStream : register(STREAM_REGISTER);

struct PrimitiveHeader
{
	uint numBytes;

	// Control flags
	uint geoMode;
	uint texMode;
	uint lightMode;
	uint blend;
	uint hasDiscard;
	uint colorDepth;
	uint numTris;

	uint2 colorMapOffsetAndStride;
	
	int2 minMaxX;
	int2 minMaxY;
};

struct PrimitiveTex
{
	uint2 textureOffsetAndStride;
	uint uWrap, vWrap;
	uint texRowShift;
};

struct PrimitiveFmt
{
	uint r_mask;
	uint r_shift;
	uint r_loss;
	uint a_mask;

	uint g_mask;
	uint g_shift;
	uint g_loss;
	uint a_shift;

	uint b_mask;
	uint b_shift;
	uint b_loss;
	uint a_loss;
};

struct TriangleHeader
{
	int2 minMaxX;
	int2 minMaxY;

	int w0_dx, w0_dy, w0_offset;
	int w1_dx, w1_dy, w1_offset;
	int w2_dx, w2_dy, w2_offset;

	float z_dx, z_dy, z_offset;
};

struct TriangleUVs
{
	float u_dx, u_dy, u_offset;
	float v_dx, v_dy, v_offset;
};

struct TriangleLights
{
	float l_dx, l_dy, l_offset;
};

uint ReadUint(inout uint Offset)
{
	uint res = aPrimitiveStream.Load(Offset);
	Offset += 4;
	return res;
}

uint2 ReadUint2(inout uint Offset)
{
	uint2 result = aPrimitiveStream.Load2(Offset);
	Offset += 8;
	return result;
}

uint3 ReadUint3(inout uint Offset)
{
	uint3 result = aPrimitiveStream.Load3(Offset);
	Offset += 12;
	return result;
}

uint4 ReadUint4(inout uint Offset)
{
	uint4 result = aPrimitiveStream.Load4(Offset);
	Offset += 16;
	return result;
}

int2 ReadInt2(inout uint Offset)
{
	uint2 res = ReadUint2(Offset);
	return asint(res);
}

int3 ReadInt3(inout uint Offset)
{
	uint3 res = ReadUint3(Offset);
	return asint(res);
}

int4 ReadInt4(inout uint Offset)
{
	uint4 res = ReadUint4(Offset);
	return asint(res);
}

float2 ReadFloat2(inout uint Offset)
{
	uint2 res = ReadUint2(Offset);
	return asfloat(res);
}

float3 ReadFloat3(inout uint Offset)
{
	uint3 res = ReadUint3(Offset);
	return asfloat(res);
}

float4 ReadFloat4(inout uint Offset)
{
	uint4 res = ReadUint4(Offset);
	return asfloat(res);
}

PrimitiveHeader ReadPrimitiveHeader(inout uint Offset)
{
	PrimitiveHeader header;
	header.numBytes = ReadUint(Offset);

	uint flags = ReadUint(Offset);
    header.geoMode     = flags & 0x7;
    header.texMode     = (flags >> 3) & 0x1;
    header.lightMode   = (flags >> 4) & 0x7;
    header.blend       = (flags >> 7) & 0x1;
    header.hasDiscard  = (flags >> 8) & 0x1;
    header.colorDepth  = (flags >> 9) & 0x3;
    header.numTris     = (flags >> 11) & 0x1F;
	
	header.colorMapOffsetAndStride = ReadUint2(Offset);
	header.minMaxX = ReadInt2(Offset);
	header.minMaxY = ReadInt2(Offset);
	
    return header;
}

PrimitiveTex ReadPrimitiveTex(inout uint Offset)
{
	PrimitiveTex tex;
	tex.textureOffsetAndStride = ReadUint2(Offset);
	
	uint4 data = ReadUint4(Offset); // handle padding
	tex.uWrap = data.x;
	tex.vWrap = data.y;
	tex.texRowShift = data.z;

	return tex;
}

PrimitiveFmt ReadPrimitiveFmt(inout uint Offset)
{
	uint3 data = ReadUint3(Offset);

	PrimitiveFmt fmt;
	
	fmt.r_mask  = (data.x >>  0) & 0xFF;
    fmt.r_shift = (data.x >>  8) & 0xFF;
    fmt.r_loss  = (data.x >> 16) & 0xFF;
    fmt.a_mask  = (data.x >> 24) & 0xFF;

    fmt.g_mask  = (data.y >>  0) & 0xFF;
    fmt.g_shift = (data.y >>  8) & 0xFF;
    fmt.g_loss  = (data.y >> 16) & 0xFF;
    fmt.a_shift = (data.y >> 24) & 0xFF;

    fmt.b_mask  = (data.z >>  0) & 0xFF;
    fmt.b_shift = (data.z >>  8) & 0xFF;
    fmt.b_loss  = (data.z >> 16) & 0xFF;
    fmt.a_loss  = (data.z >> 24) & 0xFF;

	return fmt;
}

TriangleHeader ReadTriangleHeader(inout uint Offset)
{
	TriangleHeader header;
	header.minMaxX = ReadUint2(Offset);
	header.minMaxY = ReadUint2(Offset);
	
	int3 w0 = ReadInt3(Offset);
	header.w0_dx = w0.x;
	header.w0_dy = w0.y;
	header.w0_offset = w0.z;
	
	int3 w1 = ReadInt3(Offset);
	header.w1_dx = w1.x;
	header.w1_dy = w1.y;
	header.w1_offset = w1.z;	
	
	int3 w2 = ReadInt3(Offset);
	header.w2_dx = w2.x;
	header.w2_dy = w2.y;
	header.w2_offset = w2.z;
	
	float3 z = ReadFloat3(Offset);
	header.z_dx = z.x;
	header.z_dy = z.y;
	header.z_offset = z.z;
	
	return header;
}

TriangleUVs ReadTriangleUVs(inout uint Offset)
{
	TriangleUVs uvs;
			
	float3 u = ReadFloat3(Offset);
	uvs.u_dx = u.x;
	uvs.u_dy = u.y;
	uvs.u_offset = u.z;
	
	float3 v = ReadFloat3(Offset);
	uvs.v_dx = v.x;
	uvs.v_dy = v.y;
	uvs.v_offset = v.z;
	
	return uvs;
}

TriangleLights ReadTriangleLights(inout uint Offset)
{
	TriangleLights lights;
			
	float3 l = ReadFloat3(Offset);
	lights.l_dx = l.x;
	lights.l_dy = l.y;
	lights.l_offset = l.z;
		
	return lights;
}
