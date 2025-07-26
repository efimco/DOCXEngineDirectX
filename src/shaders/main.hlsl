cbuffer ConstantBuffer : register(b0)
{
	float4x4 modelViewProjection;
	float4x4 inverseTransposedModel;
};

struct VertexInputType
{
	float3 position : POSITION;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
};

struct PixelInputType
{
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
};

PixelInputType VS(VertexInputType input)
{
	PixelInputType output;
	output.position = mul(float4(input.position, 1.0f), modelViewProjection);
	output.texCoord = input.texCoord;
	output.normal = normalize(mul((float3x3)inverseTransposedModel, input.normal));
	return output;
}

float4 PS(PixelInputType input) : SV_TARGET
{
	return float4(normalize(input.normal) * 0.5f + 0.5f, 1.0f);
}
