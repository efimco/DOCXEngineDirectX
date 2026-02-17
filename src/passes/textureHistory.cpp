#include "textureHistory.hpp"

#include "shaderManager.hpp"
#include "renderdoc/renderdoc_app.h"

TextureHistory::TextureHistory(
	ComPtr<ID3D11Device> device,
	ComPtr<ID3D11DeviceContext> context)
	: BasePass(device, context)
{
	m_constantBuffer = createConstantBuffer(sizeof(TextureHistoryCB));

	m_shaderManager->LoadComputeShader(
		"textureHistoryDifference",
		ShaderManager::GetShaderPath(L"textureHistoryDifference.hlsl"), "CS");
}

std::shared_ptr<TextureSnapshot> TextureHistory::createSnapshot(
	ComPtr<ID3D11Texture2D> texture)
{
	D3D11_TEXTURE2D_DESC texDesc;
	texture->GetDesc(&texDesc);

	std::shared_ptr<TextureSnapshot> result = std::make_shared<TextureSnapshot>();
	result->m_textureCopy = createTexture2D(
		texDesc.Width,
		texDesc.Height,
		texDesc.Format,
		D3D11_BIND_SHADER_RESOURCE);
	result->m_textureCopySRV = createShaderResourceView(result->m_textureCopy.Get(), SRVPreset::Texture2D);

	GridDims gridDims = makeGridDims(texDesc.Height, texDesc.Width);
	if (gridDims.numTiles > 0)
	{
		result->m_tileIndexBuffer = createStructuredBuffer(
			sizeof(uint32_t),
			gridDims.numTiles,
			SBPreset::Default);
		result->m_tileIndexUAV = createUnorderedAccessView(
			result->m_tileIndexBuffer.Get(),
			UAVPreset::StructuredBuffer,
			0,
			gridDims.numTiles);
		result->m_tileStagingBuffer = createStructuredBuffer(
			sizeof(uint32_t),
			gridDims.numTiles,
			SBPreset::CpuRead);
	}
	else
	{
		result->m_tileIndexBuffer = nullptr;
		result->m_tileIndexUAV = nullptr;
		result->m_tileStagingBuffer = nullptr;
	}

	m_context->CopyResource(result->m_textureCopy.Get(), texture.Get());
	return result;
}

std::shared_ptr<TextureDelta> TextureHistory::createDelta(
	std::shared_ptr<TextureSnapshot> snapshot,
	ComPtr<ID3D11Texture2D> texture,
	ComPtr<ID3D11ShaderResourceView> textureSRV)
{
	assert(snapshot != nullptr);

	D3D11_TEXTURE2D_DESC texDesc;
	texture->GetDesc(&texDesc);

	auto makeEmptyResult = [height = texDesc.Height, width = texDesc.Width](TextureDelta::Status status)
	{
		std::shared_ptr<TextureDelta> result = std::make_shared<TextureDelta>();
		result->m_status = status;
		result->m_height = height;
		result->m_width  = width;
		return result;
	};

	if (snapshot->m_textureCopy == nullptr)
	{
		return makeEmptyResult(TextureDelta::Status::SnapshotEmpty);
	}

	D3D11_TEXTURE2D_DESC snapshotTexDesc;
	snapshot->m_textureCopy->GetDesc(&snapshotTexDesc);
	if (snapshotTexDesc.Width != texDesc.Width || snapshotTexDesc.Height != texDesc.Height)
	{
		return makeEmptyResult(TextureDelta::Status::SnapshotResized);
	}

	GridDims gridDims = makeGridDims(texDesc.Height, texDesc.Width);
	TextureHistoryCB textureHistoryCB;
	textureHistoryCB.width    = texDesc.Width;
	textureHistoryCB.height   = texDesc.Height;
	textureHistoryCB.tileSize = k_textureHistoryTileSize;
	textureHistoryCB.tileNumX = gridDims.tileNumX;
	textureHistoryCB.tileNumY = gridDims.tileNumY;
	textureHistoryCB.numTiles = gridDims.numTiles;
	updateConstantBuffer(textureHistoryCB);

	m_context->CSSetShader(m_shaderManager->getComputeShader("textureHistoryDifference"), nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
	m_context->CSSetShaderResources(0, 1, textureSRV.GetAddressOf());
	m_context->CSSetShaderResources(1, 1, snapshot->m_textureCopySRV.GetAddressOf());
	m_context->CSSetUnorderedAccessViews(0, 1, snapshot->m_tileIndexUAV.GetAddressOf(), nullptr);
	m_context->Dispatch(textureHistoryCB.tileNumX, textureHistoryCB.tileNumY, 1);

	unbindShaderResources(0, 1);
	unbindShaderResources(1, 1);
	unbindComputeUAVs(0, 1);

	m_context->CopyResource(snapshot->m_tileStagingBuffer.Get(), snapshot->m_tileIndexBuffer.Get());

	std::vector<uint16_t> deltaIndices;
	deltaIndices.reserve(gridDims.numTiles);
	D3D11_MAPPED_SUBRESOURCE mapped{};
	{
		HRESULT hr = m_context->Map(snapshot->m_tileStagingBuffer.Get(), 0, D3D11_MAP_READ, 0, &mapped);
		assert(SUCCEEDED(hr));
		const uint32_t* tileDiff = static_cast<const uint32_t*>(mapped.pData);
		std::copy_if(
			tileDiff, tileDiff + gridDims.numTiles,
			std::back_inserter(deltaIndices),
			[](uint32_t i) { return i != 0; }
		);
		m_context->Unmap(snapshot->m_tileStagingBuffer.Get(), 0);
	}

	if (deltaIndices.empty())
	{
		return makeEmptyResult(TextureDelta::Status::NoChanges);
	}

	return createDelta(snapshot->m_textureCopy, deltaIndices);
}

std::shared_ptr<TextureDelta> TextureHistory::createDelta(
	ComPtr<ID3D11Texture2D> texture,
	std::vector<uint16_t>& deltaIndices)
{
	D3D11_TEXTURE2D_DESC texDesc;
	texture->GetDesc(&texDesc);

	std::shared_ptr<TextureDelta> result = std::make_shared<TextureDelta>();
	result->m_height = texDesc.Height;
	result->m_width  = texDesc.Width;

	if (deltaIndices.empty())
	{
		result->m_status = TextureDelta::Status::NoChanges;
		return result;
	}

	result->m_textureTiles = createTexture2D(
		k_textureHistoryTileSize,
		k_textureHistoryTileSize,
		DXGI_FORMAT_R32_FLOAT,
		0,
		1,
		(UINT)deltaIndices.size());
	result->m_tileIndices = deltaIndices;

	uint16_t subresourceIndex = 0;
	for (uint16_t tileIndex : result->m_tileIndices)
	{
		D3D11_BOX tileBox = makeTileBox(texDesc.Height, texDesc.Width, tileIndex);
		m_context->CopySubresourceRegion(
			result->m_textureTiles.Get(),
			D3D11CalcSubresource(0, subresourceIndex, 1),
			0, 0, 0,
			texture.Get(),
			0,
			&tileBox
		);
		++subresourceIndex;
	}

	result->m_status = TextureDelta::Status::Success;
	return result;
}

void TextureHistory::applyDelta(
	ComPtr<ID3D11Texture2D> texture,
	std::shared_ptr<TextureDelta> textureDelta)
{
	D3D11_TEXTURE2D_DESC texDesc;
	texture->GetDesc(&texDesc);
	if (texDesc.Width != textureDelta->m_width || texDesc.Height != textureDelta->m_height)
	{
		return;
	}

	uint16_t subresourceIndex = 0;
	for (uint16_t tileIndex : textureDelta->m_tileIndices)
	{
		TileDims tileDims = makeTileDims(texDesc.Height, texDesc.Width, tileIndex);
		D3D11_BOX box {
			0, 0, 0,
			tileDims.sizeX, tileDims.sizeY, 1 };
		m_context->CopySubresourceRegion(
			texture.Get(),
			0,
			tileDims.offsetX, tileDims.offsetY, 0,
			textureDelta->m_textureTiles.Get(),
			D3D11CalcSubresource(0, subresourceIndex, 1),
			&box
		);
		++subresourceIndex;
	}
}

void TextureHistory::updateConstantBuffer(
	TextureHistoryCB& cb)
{
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (SUCCEEDED(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, &cb, sizeof(TextureHistoryCB));
		m_context->Unmap(m_constantBuffer.Get(), 0);
	}
}

TextureHistory::GridDims TextureHistory::makeGridDims(UINT height, UINT width)
{
	GridDims result;
	result.tileNumX = (width  + k_textureHistoryTileSizeMOne) / k_textureHistoryTileSize;
	result.tileNumY = (height + k_textureHistoryTileSizeMOne) / k_textureHistoryTileSize;
	result.numTiles = result.tileNumX * result.tileNumY;
	return result;
}

TextureHistory::TileDims TextureHistory::makeTileDims(UINT height, UINT width, UINT tileIndex)
{
	GridDims gridDims = makeGridDims(height, width);
	UINT modX = width  % k_textureHistoryTileSize;
	UINT modY = height % k_textureHistoryTileSize;
	UINT numX = tileIndex % gridDims.tileNumX;
	UINT numY = tileIndex / gridDims.tileNumX;

	TileDims tileDims;
	tileDims.offsetX = numX * k_textureHistoryTileSize;
	tileDims.offsetY = numY * k_textureHistoryTileSize;
	tileDims.sizeX = (numX != gridDims.tileNumX - 1) || modX == 0 ? k_textureHistoryTileSize : modX;
	tileDims.sizeY = (numY != gridDims.tileNumY - 1) || modY == 0 ? k_textureHistoryTileSize : modY;
	return tileDims;
}

D3D11_BOX TextureHistory::makeTileBox(UINT height, UINT width, UINT tileIndex)
{
	TileDims tileDims = makeTileDims(height, width, tileIndex);
	return D3D11_BOX {
		tileDims.offsetX,                  // left
		tileDims.offsetY,                  // top
		0,                                 // front
		tileDims.offsetX + tileDims.sizeX, // right
		tileDims.offsetY + tileDims.sizeY, // bottom
		1                                  // back
	};
}
