struct VertexInputType
{
	float3 position : POS;
	float2 texCoord : TEX;
};

struct PixelInputType
{
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD;
};

cbuffer ConstantBuffer : register(b0)
{
	float4x4 modelViewProjection;
};

PixelInputType VS(VertexInputType input)
{
	PixelInputType output;
	float4 worldPosition = mul(float4(input.position, 1.0f), modelViewProjection);
	output.position = worldPosition;
	output.texCoord = input.texCoord;
	return output;
}

float4 PS(PixelInputType input) : SV_TARGET
{

	return float4(input.texCoord, 0.0f, 1.0f); // Assuming texCoord is defined in the context
}
