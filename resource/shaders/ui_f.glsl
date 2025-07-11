uniform sampler2D tex;
uniform sampler2D tex2;
uniform vec2 iResolution;
uniform float iTime;
uniform float param1;
uniform float param2;
uniform float param3;
in vec4 f_color;
in vec2 f_uv;
in vec3 f_coord;
out vec4 fragColor;

#ifdef RENDER_DROID2
float hash11(float p)
{
    p = fract(p * .1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

vec2 hammersley(uint i, uint N)
{
	return vec2(float(i) / float(N), float(bitfieldReverse(i)) * 2.3283064365386963e-10);
}

// random intensity at a specific time input
// same idea as https://www.shadertoy.com/view/4lBSDD
const float flickerRate = 5.0;
float flicker_at(float t)
{
	t = floor(t * flickerRate) / flickerRate;
    
	const float lightMin = 1.5;
	const float lightMax = 2.5;

	float intensity = lightMin + (sin(t) * lightMax + lightMax);
	intensity += hash11(t) * 2.0 - 1.0;
	intensity /= lightMin + lightMax;
    
	return intensity;
}

// ACES gives the lighting more punch
// done on luma to avoid ugly color shifts when untonemapping
// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
// https://www.wolframalpha.com/input?i=2.51y%5E2%2B.03y%3Dx%282.43y%5E2%2B.59y%2B.14%29+solve+for+y
vec3 ACES(vec3 x)
{
	float lum = dot(x, vec3(0.2125, 0.7154, 0.0721)) + 1e-4;
	float l = lum * (2.51 * lum + .03) / (lum * (2.43 * lum + .59) + .14);

	return (x / lum) * l;
}

vec3 ACES_Inv(vec3 x)
{
	float lum = dot(x, vec3(0.2125, 0.7154, 0.0721)) + 1e-4;
	float l = (sqrt(-10127.0 * lum * lum + 13702.0 * lum + 9.0) + 59.0 * lum - 3.0) / (502. - 486. * lum);
	return (x / lum) * l;
}

#endif

void main(void)
{
    vec4 sampled_color = textureLod(tex, f_uv, 0);
    vec4 vertex_color = f_color;
    if (sampled_color.a < 0.01)
	{
        if (param1 == 1.0)
            discard;
        else
            sampled_color = vec4(sampled_color.r, sampled_color.g, sampled_color.b, 1.0);
    }

#ifdef RENDER_DROID2
	// light effect
	// perhaps it would be better to drive this from the gui code
	// and use a generic light system for the UI
	if (param2 > 0.0)
	{
		const vec2 texsize = textureSize(tex, 0).xy;
		const float aspect = (texsize.x/texsize.y);

		vec2 snappedUV = floor(f_uv.xy * texsize * 0.5) / (texsize * 0.5);
		vec3 surfacePos = vec3(snappedUV.xy * 2.0 - 1.0, 0.0);
		surfacePos.x   *= aspect;

		// sample some lights around the center of the screen
		float light = 0.0;
		for(uint i = 0u; i < 8; ++i)
		{
			// lazily generate the positions from a hammersley sequence
			vec2  Xi    = hammersley(i, 8).xy;
			float r     = sqrt(Xi.x);
			float theta = Xi.y * M_2PI;
			vec2  point = vec2(r * 0.7 * cos(theta), r * sin(theta));
			point.y    -= 0.25; // bump them up a bit so the top of the menu is well lit

			vec3 lightPos    = vec3(point, 0.5);
			     lightPos.x *= aspect;

			float rand = hash11(i * 657.9759);
			float intensity = 1.0 - rand * 0.2;

			vec3 ldir = lightPos.xyz - surfacePos;
			vec3 lvec = normalize(ldir);

			// inverse square
			//float atten = 1.0 / (dot(ldir, ldir) + 0.01);
			//atten *= 0.5;

			// tighter attenuation via quadratic falloff
			const float invLightRadius = 1.0 / 1.4;
			vec3 dist = ldir * invLightRadius;
			float atten = saturate(1.0 - dot(dist, dist));
			//atten *= atten;
        
			float lightIntensity = sqrt(saturate(intensity + 0.1));
			lightIntensity *= saturate(lvec.z); // ndotl
			lightIntensity *= atten * 0.6;

			// smooth random flicker
			// interpolate between 2 flickers at N time
			float flickerTime = rand * 8.0 + iTime;
			lightIntensity *= mix(
				flicker_at(flickerTime - 1.0/flickerRate), // last frame
				flicker_at(flickerTime), // current frame
				fract(flickerTime * flickerRate)
			);

			light += lightIntensity;
		}

		// inverse tone map, apply lighting, then apply again
		// this prevents bright whites from dimming too much
		sampled_color.rgb = ACES_Inv(sampled_color.rgb);
		sampled_color.rgb = sampled_color.rgb * light;
		sampled_color.rgb = ACES(sampled_color.rgb);
	}
#endif

    fragColor     = sampled_color * vertex_color;
    fragColor.rgb = pow(fragColor.rgb, vec3(1.0/param3));
}
