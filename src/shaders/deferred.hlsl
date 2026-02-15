
#include "BRDF.hlsl"
#include "constants.hlsl"
Texture2D tAlbedo : register(t0);
Texture2D tMetallicRoughness : register(t1);
Texture2D tNormal : register(t2);
Texture2D tFragPos : register(t3);
Texture2D tObjectID : register(t4);
Texture2D tDepth : register(t5);

struct Light
{
	int lightType;
	float3 position;
	float intensity;
	float3 color;
	float3 direction;
	float padding1; // Padding to align to 16 bytes
	float2 spotParams;
	float2 padding2; // Additional padding to align to 16 bytes
	float3 attenuations;
	float radius; // radius for point lights
	float objectID;
	float3 padding3;
};

struct GBuffer
{
	float4 albedo;
	float metallic;
	float roughness;
	float3 normal;
	float3 fragPos;
	float objectID;
};

cbuffer CB : register(b0)
{
	float IBLrotationY;
	float IBLintensity;
	float selectedID;
	int numLights;
	float3 cameraPosition;
	int drawWSUI;
};

StructuredBuffer<Light> lights : register(t6);
Texture2D<float4> tBackground : register(t7);
TextureCube tIrradianceMap : register(t8);
TextureCube tPrefilteredMap : register(t9);
Texture2D tBRDFLUT : register(t10);
Texture2D worldSpaceUITexture : register(t11);
Texture2D gtaoTexture : register(t12);

SamplerState linearSampler : register(s0);
RWTexture2D<unorm float4> outColor : register(u0);

static const float MAX_REFLECTION_LOD = 7.0;
static const float MIN_REFLECTANCE = 0.04;
static const float YAW = PI / 180.0;

static const float ICONS_TRANSPARENCY_FACTOR = 0.3;

static float3 MOD3 = float3(443.8975, 397.2973, 491.1871);
static const float c0 = 32.0;

float hash11(float p)
{
	float3 p3 = frac(float3(p, p, p) * MOD3);
	p3 += dot(p3, p3.yzx + 19.19);
	return frac((p3.x + p3.y) * p3.z);
}
float hash12(float2 p)
{
	float3 p3 = frac(float3(p.xyx) * MOD3);
	p3 += dot(p3, p3.yzx + 19.19);
	return frac((p3.x + p3.y) * p3.z);
}
float3 hash32(float2 p)
{
	float3 p3 = frac(float3(p.xyx) * MOD3);
	p3 += dot(p3, p3.yxz + 19.19);
	return frac(float3((p3.x + p3.y) * p3.z, (p3.x + p3.z) * p3.y, (p3.y + p3.z) * p3.x));
}

void applyDitheredNoise(uint2 DTid, inout float4 outcol)
{
	float2 seed = float2(DTid);

	float3 rnd = hash32(seed);

	outcol += float4((rnd - 0.) / 64.0, 0.0);
}

float3x3 getIrradianceMapRotation()
{
	float angle = IBLrotationY * YAW;
	float cosA = cos(angle);
	float sinA = sin(angle);
	return float3x3(cosA, 0.0, -sinA, 0.0, 1.0, 0.0, sinA, 0.0, cosA);
}

void linearRGBtoSRGB(inout float3 color) // took it from blender's source code
{
	[unroll]
	for (int i = 0; i < 3; i++)
	{
		if (color[i] < 0.0031308)
		{
			color[i] = (color[i] < 0.0) ? 0.0 : color[i] * 12.92;
		}
		else
		{
			color[i] = 1.055 * pow(color[i], 1.0 / 2.2) - 0.055;
		}
	}
}

void aces(inout float3 color)
{
	color = color * (2.51 * color + 0.03) / (color * (2.43 * color + 0.59) + 0.14);
}

float3 agxDefaultContrastApprox(float3 x)
{
	float3 x2 = x * x;
	float3 x4 = x2 * x2;

	return +15.5 * x4 * x2
		- 40.14 * x4 * x
		+ 31.96 * x4
		- 6.868 * x2 * x
		+ 0.4298 * x2
		+ 0.1191 * x
		- 0.00232;
}

float3 agx(float3 color)
{
	const float3x3 agxMat = float3x3(
		0.842479062253094, 0.0423282422610123, 0.0423756549057051,
		0.0784335999999992, 0.878468636469772, 0.0784336,
		0.0792237451477643, 0.0791661274605434, 0.879142973793104
	);

	const float3x3 agxMatInv = float3x3(
		1.19687900512017, -0.0528968517574562, -0.0529716355144438,
		-0.0980208811401368, 1.15190312990417, -0.0980434501171241,
		-0.0990297440797205, -0.0989611768448433, 1.15107367264116
	);

	const float minEv = -12.47393;
	const float maxEv = 4.026069;

	color = mul(agxMat, color);
	color = max(color, 1e-10);
	color = log2(color);
	color = (color - minEv) / (maxEv - minEv);
	color = clamp(color, 0.0, 1.0);
	color = agxDefaultContrastApprox(color);
	color = mul(agxMatInv, color);

	return color;

}

uint querySpecularTextureLevels()
{
	uint width, height, levels;
	tPrefilteredMap.GetDimensions(0, width, height, levels);
	return levels;
}
float3 applyPointLight(Light light, float3 V, float3 F0, GBuffer gbuffer)
{
	float3 L = normalize(light.position - gbuffer.fragPos);
	float3 H = normalize(V + L);

	float distance = length(light.position - gbuffer.fragPos);

	// Attenuation
	float attenuation = saturate(pow(1.0 - pow(distance / light.radius, 4.0), 2.0)) / (pow(distance, 2.0) + 1.0);

	if (light.radius < distance)
	{
		attenuation = 0.0;
	}

	float3 radiance = light.color * light.intensity * attenuation;

	float NdotL = max(dot(gbuffer.normal, L), 0.0);
	float NdotV = max(dot(gbuffer.normal, V), 0.0);
	float NdotH = max(dot(gbuffer.normal, H), 0.0);
	float HdotV = max(dot(H, V), 0.0);

	float D = distributionGGX(NdotH, gbuffer.roughness);
	float G = geometrySmith(NdotV, NdotL, gbuffer.roughness);
	float3 F = fresnelSchlick(HdotV, F0, gbuffer.roughness);

	// Energy conservation
	float3 kS = F;
	float3 kD = (1.0 - kS) * (1.0 - gbuffer.metallic);
	// Specular
	float3 numerator = D * G * F;
	float denominator = 4.0 * NdotV * NdotL + 0.001; // Add epsilon to prevent divide by zero
	float3 specular = numerator / denominator;

	float3 result = (kD * gbuffer.albedo.rgb / PI + specular) * radiance * NdotL;

	return result;
}

float3 outline(uint3 DTid, float objectID)
{
	if (objectID != selectedID) // -1 because 0 is value for background but we want to select first object with ID 0
		return float3(0.0, 0.0, 0.0);
	float3 outlineColor = float3(0.65, 0.25, 0.0);
	int thickness = 1;

	// Simple outline based on object ID difference with neighboring pixels
	bool isEdge = false;
	[unroll]
		for (int y = -thickness; y <= thickness; y++)
		{
			[unroll]
			for (int x = -thickness; x <= thickness; x++)
			{
				if (x == 0 && y == 0) continue;
				float neighborID = tObjectID.Load(int3(DTid.xy + int2(x, y), 0));
				if (neighborID != objectID)
				{
					isEdge = true;
					break;
				}
			}
			if (isEdge) break;
		}

	return isEdge ? outlineColor : float3(0.0, 0.0, 0.0);
}

void applyLighting(inout float3 color, GBuffer gbuffer)
{
	float3 V = normalize(cameraPosition - gbuffer.fragPos);
	float3 F0 = lerp(float3(MIN_REFLECTANCE, MIN_REFLECTANCE, MIN_REFLECTANCE), gbuffer.albedo.rgb, gbuffer.metallic);
	{
		for (int i = 0; i < numLights; ++i)
		{
			Light light = lights[i];
			if (light.lightType == 0) // Point
			{
				color += applyPointLight(light, V, F0, gbuffer);
			}
			else if (light.lightType == 1) // Directional
			{
				continue;
			}
			else if (light.lightType == 2) // Spot
			{
				continue;
			}
		}
	}
}

GBuffer loadGBuffer(uint2 DTid)
{
	GBuffer gbuffer;
	gbuffer.albedo = tAlbedo.Load(int3(DTid, 0));
	float2 mr = tMetallicRoughness.Load(int3(DTid, 0)).xy;
	gbuffer.metallic = saturate(mr.x);
	gbuffer.roughness = saturate(mr.y);
	gbuffer.normal = tNormal.Load(int3(DTid, 0)).xyz;
	gbuffer.normal = normalize(gbuffer.normal * 2.0f - 1.0f);
	gbuffer.fragPos = tFragPos.Load(int3(DTid, 0));
	gbuffer.objectID = tObjectID.Load(int3(DTid, 0));
	return gbuffer;
}

void applyIBL(inout float3 color, GBuffer gbuffer)
{
	float3 V = normalize(cameraPosition - gbuffer.fragPos);
	float NdotV = clamp(dot(gbuffer.normal, V), 0.01, 0.99);

	float3 R = reflect(-V, gbuffer.normal);

	// Apply IBL rotation
	float3x3 irradianceRotation = getIrradianceMapRotation();
	float3 rotatedN = mul(irradianceRotation, gbuffer.normal);
	float3 rotatedR = mul(irradianceRotation, R);

	float3 F0 = lerp(float3(MIN_REFLECTANCE, MIN_REFLECTANCE, MIN_REFLECTANCE), gbuffer.albedo.rgb, gbuffer.metallic);
	// Metallic workflow: interpolate between dielectric 0.04 and albedo by metallic
	// IBL Diffuse
	float3 irradiance = tIrradianceMap.SampleLevel(linearSampler, rotatedN, 0).rgb;
	float3 F = fresnelSchlick(NdotV, F0, gbuffer.roughness);
	float3 kS = F;
	float3 kD = (1.0 - kS) * (1.0 - gbuffer.metallic);
	// Lambert diffuse with irradiance convolution already accounting for 1/PI; omit /PI if irradiance is pre-integrated.
	float3 diffuseIBL = irradiance * gbuffer.albedo.rgb * kD;

	// IBL Specular
	uint numMips = querySpecularTextureLevels();  // total mip levels
	float mipLevel = gbuffer.roughness * max(float(numMips) - 1.0f, 0.0f);
	float3 prefilteredColor = tPrefilteredMap.SampleLevel(linearSampler, rotatedR, mipLevel).rgb;
	float2 brdf = tBRDFLUT.SampleLevel(linearSampler, float2(NdotV, gbuffer.roughness), 0).rg;
	float3 specularIBL = prefilteredColor * (F0 * brdf.x + brdf.y);

	// Combine IBL
	color = (diffuseIBL + specularIBL) * IBLintensity;
}

void applyAO(inout float3 color, uint2 DTid)
{
	float ao = gtaoTexture.Load(int3(DTid, 0)).r;
	color *= ao;
}

void drawBackground(uint2 DTid)
{
	float3 background = tBackground.Load(int3(DTid, 0)).xyz;
	linearRGBtoSRGB(background);
	float4 finalBackground = float4(background, 1.0);
	aces(finalBackground.rgb);
	outColor[DTid] = finalBackground;
}

void drawWorldSpaceUI(uint2 DTid)
{
	float4 uiColor = worldSpaceUITexture.Load(int3(DTid, 0));
	uiColor.rgb *= uiColor.a * ICONS_TRANSPARENCY_FACTOR;
	outColor[DTid] += float4(uiColor.rgb, 0.0);
}

[numthreads(16, 16, 1)]
void CS(uint3 DTid : SV_DISPATCHTHREADID)
{
	GBuffer gbuffer = loadGBuffer(DTid.xy);
	float4 finalColor = float4(0.0, 0.0, 0.0, 1.0);

	// Early exit for background
	applyIBL(finalColor.rgb, gbuffer);
	if (gbuffer.albedo.a < 0.1)
	{
		drawBackground(DTid.xy);
	}

	applyLighting(finalColor.rgb, gbuffer);

	linearRGBtoSRGB(finalColor.rgb);
	aces(finalColor.rgb);
	applyDitheredNoise(DTid.xy, finalColor);

	applyAO(finalColor.rgb, DTid.xy);
	outColor[DTid.xy] = lerp(outColor[DTid.xy], finalColor, gbuffer.albedo.a);
	outColor[DTid.xy] += float4(outline(DTid, gbuffer.objectID), 0.0);
	[branch]
		if (drawWSUI == 1)
		{
			drawWorldSpaceUI(DTid.xy);
		}
}
