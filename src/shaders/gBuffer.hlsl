#include "BRDF.hlsl"

cbuffer ConstantBuffer : register(b0)
{
	float4x4 modelViewProjection;
	float4x4 inverseTransposedModel;
	float4x4 model;
	float4x4 view;
	float3 cameraPosition;
	float objectID;
	float4 albedoColor;
	float metallicValue;
	float roughnessValue;
	uint useAlbedoTexture;
	uint useMetallicRoughnessTexture;
	uint useNormalTexture;
	uint flipY;
	float2 padding;
};

Texture2D albedoTexture : register(t0);
Texture2D metallicRoughnessTexture : register(t1);
Texture2D normalTexture : register(t2);

SamplerState samplerState : register(s0);

struct VertexInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 texCoord : TEXCOORD;
	float3 tangent : TANGENT;
};

struct VertexOutput
{
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float3 viewNormal : NORMAL1;
	float3 tangent : TANGENT0;
	float4 fragPos : NORMAL2;
	float4 viewSpaceFragPos : NORMAL3;
};


struct PSOutput
{
	float4 albedo : SV_TARGET0;
	float2 metallicRoughness : SV_TARGET1;
	float4 normal : SV_TARGET2;
	float4 viewSpaceNormal : SV_TARGET3;
	float4 fragPos : SV_TARGET4;
	float4 viewSpaceFragPos : SV_TARGET5;
	float objectID : SV_TARGET6;
};

VertexOutput VS(VertexInput input)
{
	VertexOutput output;
	output.position = mul(float4(input.position, 1.0f), modelViewProjection);
	output.fragPos = mul(float4(input.position, 1.0f), model);
	output.viewSpaceFragPos = mul(output.fragPos, view);
	output.texCoord = input.texCoord;
	output.normal = normalize(mul((float3x3)inverseTransposedModel, input.normal));
	output.tangent = normalize(mul((float3x3)inverseTransposedModel, input.tangent));
	return output;
}

float3 getNormalFromMap(VertexOutput input, bool hasNormalMap = true)
{
	float3 tangentNormal = normalTexture.Sample(samplerState, input.texCoord).xyz * 2.0f - 1.0f;
	if (!hasNormalMap || !useNormalTexture)
	{
		float3 N = normalize(input.normal);
		return N;
	}
	// Invert Y for DirectX normal maps
	if (flipY == 1)
		tangentNormal.y = -tangentNormal.y;

	float3 N = normalize(input.normal);
	float3 T = normalize(input.tangent);
	float3 B = normalize(cross(N, T));
	float3x3 TBN = float3x3(T, B, N);

	return normalize(mul(tangentNormal, TBN));
}

PSOutput PS(VertexOutput input)
{
	uint albedoWidth, albedoHeight;
	albedoTexture.GetDimensions(albedoWidth, albedoHeight);
	bool hasAlbedoTexture = albedoWidth != 0 && albedoHeight != 0;

	uint metallicRoughnessWidth, metallicRoughnessHeight;
	metallicRoughnessTexture.GetDimensions(metallicRoughnessWidth, metallicRoughnessHeight);
	bool hasMetallicRoughnessTexture = metallicRoughnessWidth != 0 && metallicRoughnessHeight != 0;

	uint normalWidth, normalHeight;
	normalTexture.GetDimensions(normalWidth, normalHeight);
	bool hasNormalTexture = normalWidth != 0 && normalHeight != 0;

	PSOutput output;
	if (!hasAlbedoTexture || !useAlbedoTexture)
	{
		output.albedo = albedoColor;
	}
	else
	{
		output.albedo = albedoTexture.Sample(samplerState, input.texCoord);
	}

	output.fragPos = float4(input.fragPos.xyz, 1.0f);
	output.viewSpaceFragPos = float4(input.viewSpaceFragPos.xyz, 1.0f);
	float3 tangentNormal = getNormalFromMap(input, hasNormalTexture);
	float3 V = normalize(cameraPosition - input.fragPos.xyz);
	float3 selectionColor = float3(1.9, 0.5, 0.0);
	if (output.albedo.a < 0.1f)
	{
		discard;
	}
	float gammaFactor = 2.6f;
	output.albedo = float4(pow(output.albedo.rgb, float3(gammaFactor, gammaFactor, gammaFactor)), output.albedo.a);

	float roughness, metallic;
	if (!hasMetallicRoughnessTexture || !useMetallicRoughnessTexture)
	{
		output.metallicRoughness = float2(metallicValue, roughnessValue);
	}
	else
	{
		metallic = hasMetallicRoughnessTexture ? metallicRoughnessTexture.Sample(samplerState, input.texCoord).b : metallicValue;
		roughness = hasMetallicRoughnessTexture ? metallicRoughnessTexture.Sample(samplerState, input.texCoord).g : roughnessValue;
		output.metallicRoughness = float2(metallic, roughness);
	}

	// Encode normal from [-1,1] to [0,1] for storage in UNORM render target
	output.normal = float4(tangentNormal * 0.5f + 0.5f, 1.0f);
	float3 viewNormal = normalize(mul(tangentNormal, (float3x3)(view)));
	output.viewSpaceNormal = float4(viewNormal * 0.5f + 0.5f, 1.0f);


	output.objectID = objectID;
	return output;
}
