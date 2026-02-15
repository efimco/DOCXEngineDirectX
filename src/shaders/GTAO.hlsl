cbuffer GTAOConstants : register(b0)
{
	float4x4 projection;
	float4x4 view;
	uint2 dimensions;
	float2 nearFarPlanes;
	uint sampleCount;
	float aoRadius;
	float aoBias;
	float aoIntensity;
};

Texture2D<float4> gViewNormal : register(t0);
Texture2D<float4> gViewPosition : register(t1);
StructuredBuffer<float3> kernel : register(t2);
Texture2D<float3> randomRotations : register(t3);

SamplerState pointSampler : register(s0);

RWTexture2D<float> gAO : register(u0);

Texture2D<float> gtaoTexture : register(t4);
RWTexture2D<float> gBlurredAO : register(u1);


[numthreads(16, 16, 1)]
void CS(uint3 DTid : SV_DISPATCHTHREADID)
{
	float2 uv = (DTid.xy + 0.5f) / float2(dimensions);

	float4 viewPos = gViewPosition.SampleLevel(pointSampler, uv, 0); // in [-1,1] range
	float3 normal = gViewNormal.SampleLevel(pointSampler, uv, 0).xyz; // currently in [0,1] range

	if (length(viewPos.xyz) == 0.0f)
	{
		gAO[DTid.xy] = 1.0f; // No geometry, so no occlusion
		return;
	}

	float2 noiseScale = float2(dimensions) / 4.0f; // randomRotations is a 4x4 tiling noise texture
	float3 randomVec = normalize(randomRotations.SampleLevel(pointSampler, uv * noiseScale, 0).xyz);

	float3 tangent = normalize(randomVec - normal * dot(randomVec, normal)); // Gram-Schmidt orthogonalization
	// float3 tangent = normalize(normal); // apply random rotation to tangent
	float3 bitangent = cross(normal, tangent);
	float3x3 TBN = float3x3(tangent, bitangent, normal);

	float occlusion = 0.0f;

	for (int i = 0; i < sampleCount; ++i)
	{
		float3 samplePos = mul(kernel[i], TBN); // transform from tangent space to view space
		samplePos = viewPos.xyz + samplePos * aoRadius; // scale by radius and translate to view space position

		float4 offset = float4(samplePos, 1.0f);
		offset = mul(offset, projection);
		offset.xyz /= offset.w;
		offset.x = offset.x * 0.5f + 0.5f;
		offset.z = offset.z * 0.5f + 0.5f;
		offset.y = -offset.y * 0.5f + 0.5f; // Flip Y: clip Y is up, UV V is down

		if (offset.x < 0.0f || offset.x > 1.0f || offset.y < 0.0f || offset.y > 1.0f)
			continue; // Skip samples that are outside the screen bounds

		float sampleDepth = gViewPosition.SampleLevel(pointSampler, offset.xy, 0).z; // get depth value of kernel sample
		float rangeCheck = smoothstep(0.0, 1.0, aoRadius / abs(viewPos.z - sampleDepth));
		occlusion += (sampleDepth >= samplePos.z + aoBias ? 1.0 : 0.0) * rangeCheck;
	}
	occlusion = 1.0f - (occlusion / float(sampleCount)); // Normalize and invert to get AO factor
	gAO[DTid.xy] = saturate(pow(occlusion, aoIntensity)); // Apply intensity curve
}

[numthreads(16, 16, 1)]
void BlurCS(uint3 DTid : SV_DISPATCHTHREADID)
{
	uint2 dimensions;
	gtaoTexture.GetDimensions(dimensions.x, dimensions.y);

	float2 uv = (DTid.xy + 0.5f) / float2(dimensions);
	float result = 0.0;
	int blurRadius = 4; // Adjust the blur radius as needed
	for (int x = -blurRadius; x < blurRadius; ++x)
	{
		for (int y = -blurRadius; y < blurRadius; ++y)
		{
			uint2 offset = DTid.xy + uint2(x, y);
			if (offset.x < 0 || offset.y < 0 || offset.x >= dimensions.x || offset.y >= dimensions.y)
				continue; // Skip out-of-bounds samples
			result += gtaoTexture.Load(int3(offset, 0)); // Sample neighboring pixels
		}
	}
	gBlurredAO[DTid.xy] = result / ((2.0 * blurRadius) * (2.0 * blurRadius));
}

