#ifndef COLOR_H
#define COLOR_H

struct ColorFormat
{
	uint colorMode;
	uint bpp;

	uint r_bits;
	uint r_shift;
	uint r_bitdiff;

	uint g_bits;
	uint g_shift;
	uint g_bitdiff;

	uint b_bits;
	uint b_shift;
	uint b_bitdiff;

	uint a_bits;
	uint a_shift;
	uint a_bitdiff;
};

// Helper to extract bits [lo, hi) from a uint
uint ExtractBits(uint value, uint offset, uint bits)
{
	return (value >> offset) & ((1u << bits) - 1);
}

ColorFormat UnpackColorFormat(uint2 packed)
{
	uint low  = packed.x;
	uint high = packed.y;

	ColorFormat fmt;

	uint offset = 0;

	// First 8 bits
	fmt.colorMode = ExtractBits(low, offset, 2); offset += 2;
	fmt.bpp       = ExtractBits(low, offset, 6); offset += 6;

	// R (14 bits)
	fmt.r_bits    = ExtractBits(low, offset, 4); offset += 4;
	fmt.r_shift   = ExtractBits(low, offset, 6); offset += 6;
	fmt.r_bitdiff = ExtractBits(low, offset, 4); offset += 4;

	// G (14 bits)
	fmt.g_bits    = ExtractBits(low, offset, 4); offset += 4;
	fmt.g_shift   = ExtractBits(low, offset, 6); offset += 6;
	offset = 0;
	fmt.g_bitdiff = ExtractBits(high, offset, 4); offset += 4;

	// B (14 bits)
	fmt.b_bits    = ExtractBits(high, offset, 4); offset += 4;
	fmt.b_shift   = ExtractBits(high, offset, 6); offset += 6;
	fmt.b_bitdiff = ExtractBits(high, offset, 4); offset += 4;

	// A (14 bits)
	fmt.a_bits    = ExtractBits(high, offset, 4); offset += 4;
	fmt.a_shift   = ExtractBits(high, offset, 6); offset += 6;
	fmt.a_bitdiff = ExtractBits(high, offset, 4);

	return fmt;
}

uint ScaleColorComponent(uint cc, int srcBPP, int deltaBPP)
{
	if (deltaBPP <= 0) // Upscale
	{
		int dsrcBPP = srcBPP + deltaBPP;
		return (cc << -deltaBPP)
			| (dsrcBPP >= 0
			   ? (cc >> dsrcBPP)
			   : (cc * ((1 << -deltaBPP) - 1)));
	}
	return cc >> deltaBPP; // Downscale
}

uint EncodeRGB(ColorFormat ci, uint r, uint g, uint b)
{
	// Scale color components according to bits per component
	uint redScaled = r;
	uint greenScaled = g;
	uint blueScaled = b;

	// Adjust for component bit depth if needed
	if (ci.r_bits < 8)
		redScaled = redScaled >> (8 - ci.r_bits);

	if (ci.g_bits < 8)
		greenScaled = greenScaled >> (8 - ci.g_bits);
	
	if (ci.b_bits < 8)
		blueScaled = blueScaled >> (8 - ci.b_bits);

	// Shift components to their positions and combine
	uint encoded = 0;
	if (ci.r_shift >= 0)
		encoded |= (redScaled << ci.r_shift);
	else
		encoded |= (redScaled >> ci.r_bitdiff);

	if (ci.g_shift >= 0)
		encoded |= (greenScaled << ci.g_shift);
	else
		encoded |= (greenScaled >> ci.g_bitdiff);

	if (ci.b_shift >= 0)
		encoded |= (blueScaled << ci.b_shift);
	else
		encoded |= (blueScaled >> ci.b_bitdiff);

	return encoded;
}

uint EncodeRGBA(ColorFormat ci, uint r, uint g, uint b, uint a)
{
	// Start with RGB encoding
	uint encoded = EncodeRGB(ci, r, g, b);

	// Add alpha if supported by the format
	if (ci.a_bits > 0)
	{
		uint alphaScaled = a;

		// Adjust for alpha bit depth if needed
		if (ci.a_bits < 8)
			alphaScaled = alphaScaled >> (8 - ci.a_bits);

		// Shift alpha to its position and combine
		if (ci.a_shift >= 0)
			encoded |= (alphaScaled << ci.a_shift);
		else
			encoded |= (alphaScaled >> ci.a_bitdiff);
	}

	return encoded;
}

void DecodeRGB(uint encoded, ColorFormat ci, out uint r, out uint g, out uint b)
{
	// Create masks based on bit depths
	uint redMask = ((1 << ci.r_bits) - 1);
	uint greenMask = ((1 << ci.g_bits) - 1);
	uint blueMask = ((1 << ci.b_bits) - 1);

	// Extract components using shifts and masks
	uint redVal;
	if (ci.r_shift >= 0)
		redVal = (encoded >> ci.r_shift) & redMask;
	else
		redVal = (encoded << ci.r_bitdiff) & redMask;

	uint greenVal;
	if (ci.g_shift >= 0)
		greenVal = (encoded >> ci.g_shift) & greenMask;
	else
		greenVal = (encoded << ci.g_bitdiff) & greenMask;

	uint blueVal;
	if (ci.b_shift >= 0)
		blueVal = (encoded >> ci.b_shift) & blueMask;
	else
		blueVal = (encoded << ci.b_bitdiff) & blueMask;

	// Scale back to 8-bit range if needed
	if (ci.r_bits < 8)
		redVal = (redVal << (8 - ci.r_bits)) | (redVal >> (2 * ci.r_bits - 8));

	if (ci.g_bits < 8)
		greenVal = (greenVal << (8 - ci.g_bits)) | (greenVal >> (2 * ci.g_bits - 8));

	if (ci.b_bits < 8)
		blueVal = (blueVal << (8 - ci.b_bits)) | (blueVal >> (2 * ci.b_bits - 8));

	r = redVal;
	g = greenVal;
	b = blueVal;
}


void DecodeRGBA(uint encoded, ColorFormat ci, out uint r, out uint g, out uint b, out uint a)
{
	// First decode the RGB components
	DecodeRGB(encoded, ci, r, g, b);

	// Then handle alpha if the format supports it
	if (ci.a_bits > 0)
	{
		uint alphaMask = ((1 << ci.a_bits) - 1);
		uint alphaVal;

		// Extract alpha component
		if (ci.a_shift >= 0)
			alphaVal = (encoded >> ci.a_shift) & alphaMask;
		else
			alphaVal = (encoded << ci.a_bitdiff) & alphaMask;

		// Scale back to 8-bit range if needed
		if (ci.a_bits < 8)
			alphaVal = (alphaVal << (8 - ci.a_bits)) | (alphaVal >> (2 * ci.a_bits - 8));

		a = alphaVal;
	}
	else
	{
		// If format doesn't support alpha, set to fully opaque
		a = 255;
	}
}

#endif
