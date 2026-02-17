#pragma once

#include <memory>
#include <string_view>

#include <d3d11_4.h>
#include <string>
#include <glm/glm.hpp>
#include <wrl.h>

#include "utility/stringUnorderedMap.hpp"
#include "basePass.hpp"

using namespace Microsoft::WRL;

struct TextureSnapshot
{
	ComPtr<ID3D11Texture2D> m_textureCopy;
	ComPtr<ID3D11ShaderResourceView> m_textureCopySRV;

	ComPtr<ID3D11Buffer> m_tileIndexBuffer;
	ComPtr<ID3D11Buffer> m_tileStagingBuffer;
	ComPtr<ID3D11UnorderedAccessView> m_tileIndexUAV;
};

struct TextureDelta
{

	enum class Status
	{
		Unknown = 0,
		SnapshotEmpty,
		SnapshotResized,
		NoChanges,
		Success
	};

	Status m_status = Status::Unknown;
	uint32_t m_width = 0;
	uint32_t m_height = 0;

	ComPtr<ID3D11Texture2D> m_textureTiles;
	std::vector<uint16_t> m_tileIndices;
};

struct alignas(16) TextureHistoryCB
{
	uint32_t height;
	uint32_t width;
	uint32_t tileSize;
	uint32_t tileNumX;
	uint32_t tileNumY;
	uint32_t numTiles;
};

class TextureHistory : public BasePass
{
public:
	TextureHistory(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
	~TextureHistory() override = default;

	std::shared_ptr<TextureSnapshot> createSnapshot(
		ComPtr<ID3D11Texture2D> texture);

	std::shared_ptr<TextureDelta> createDelta(
		std::shared_ptr<TextureSnapshot> snapshot,
		ComPtr<ID3D11Texture2D> texture,
		ComPtr<ID3D11ShaderResourceView> textureSRV);

	std::shared_ptr<TextureDelta> createDelta(
		ComPtr<ID3D11Texture2D> texture,
		std::vector<uint16_t>& deltaIndices);

	void applyDelta(
		ComPtr<ID3D11Texture2D> texture,
		std::shared_ptr<TextureDelta> textureDelta);

private:
	void updateConstantBuffer(
		TextureHistoryCB& cb);

	struct GridDims
	{
		uint16_t tileNumX;
		uint16_t tileNumY;
		UINT numTiles;
	};

	struct TileDims
	{
		UINT offsetX;
		UINT offsetY;
		UINT sizeX;
		UINT sizeY;
	};

	static GridDims makeGridDims(UINT height, UINT width);
	static TileDims makeTileDims(UINT height, UINT width, UINT tileIndex);
	static D3D11_BOX makeTileBox(UINT height, UINT width, UINT tileIndex);

	//  ## Resources for paint history ##

	// Size of a history tile - cannot be smaller than 8
	static constexpr uint32_t k_textureHistoryTileSize = 64;
	static constexpr uint32_t k_textureHistoryTileSizeMOne = k_textureHistoryTileSize - 1;

	ComPtr<ID3D11Buffer> m_constantBuffer;
};
