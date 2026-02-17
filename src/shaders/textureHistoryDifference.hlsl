
cbuffer CB : register(b0)
{
	uint Height;
	uint Width;
	uint TileSize;
	uint TileNumX;
	uint TileNumY;
	uint NumTiles;
}

// For now we only read from rayDirectionBlend texture - those are F32 masks.
// If you in the future would ever need to diff textures with multiple components -
// convert this to a normal Texture2D
Texture2D<float> TextureA : register(t0);
Texture2D<float> TextureB : register(t1);

// An output buffer that receives tile indices - its preallocated to be NumTiles
RWStructuredBuffer<uint> TileIndices : register(u0);

// Shared variable to hold per-tile diff flag
groupshared uint tileDiff;

[numthreads(16, 16, 1)]
void CS(uint3 GTid : SV_GroupThreadID,
		uint3 Gid  : SV_GroupID)
{
	// Pixels per thread
	uint pxPerThread = TileSize / 16;

	// Tile origin in pixel coordinates
	uint2 tileOrigin = Gid.xy * TileSize;

	// Each thread handles an px x px tile
	uint2 boxMin = tileOrigin + GTid.xy * pxPerThread;
	uint2 boxMax = min(boxMin + pxPerThread, uint2(Width, Height));

	// Initialize shared tileDiff once per group
	if (all(GTid.xy == 0))
		tileDiff = 0;

	GroupMemoryBarrierWithGroupSync();

	bool threadDiff = false;
	for (uint y = boxMin.y; y < boxMax.y; ++y)
	{
		for (uint x = boxMin.x; x < boxMax.x; ++x)
		{
			uint3 pixelCoord = uint3(x, y, 0);
			float a = TextureA.Load(pixelCoord);
			float b = TextureB.Load(pixelCoord);
			// If any channel differs, mark tile as changed
			threadDiff = threadDiff | any(a != b);
		}
	}
	if (threadDiff)
	{
		InterlockedOr(tileDiff, 1);
	}

	GroupMemoryBarrierWithGroupSync();

	// Only one thread writes the tileDiff flag per tile
	if (all(GTid.xy == 0))
	{
		uint tileIndex = Gid.y * TileNumX + Gid.x;
		TileIndices[tileIndex] = tileDiff > 0 ? tileIndex : 0;
	}
}