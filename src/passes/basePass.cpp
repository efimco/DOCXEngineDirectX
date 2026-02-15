#include "basePass.hpp"
#include <d3d11.h>
#include <iostream>
#include <vector>
#include <wrl/client.h>
#include "shaderManager.hpp"

BasePass::BasePass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
	: m_device(device), m_context(context), m_shaderManager(std::make_unique<ShaderManager>(device))
{
	m_context.As(&m_annotation);
}

ComPtr<ID3D11RasterizerState> BasePass::createRSState(const RasterizerPreset preset) const
{
	switch (preset)
	{
	case RasterizerPreset::Default:
		return createRSState(D3D11_CULL_NONE, D3D11_FILL_SOLID, true, false);
	case RasterizerPreset::NoCullNoClip:
		return createRSState(D3D11_CULL_NONE, D3D11_FILL_SOLID, false, false);
	case RasterizerPreset::BackCull:
		return createRSState(D3D11_CULL_BACK, D3D11_FILL_SOLID, false, false);
	case RasterizerPreset::Wireframe:
		return createRSState(D3D11_CULL_NONE, D3D11_FILL_WIREFRAME, true, true);
	case RasterizerPreset::FrontCull:
		return createRSState(D3D11_CULL_FRONT, D3D11_FILL_SOLID, false, false);
	default:
		return createRSState(D3D11_CULL_NONE, D3D11_FILL_SOLID, true, false);
	}
}

ComPtr<ID3D11RasterizerState> BasePass::createRSState(const D3D11_CULL_MODE cullMode,
	const D3D11_FILL_MODE fillMode,
	const bool depthClipEnable,
	const bool antialiasedLineEnable) const
{
	D3D11_RASTERIZER_DESC desc = {};
	desc.CullMode = cullMode;
	desc.FillMode = fillMode;
	desc.DepthClipEnable = depthClipEnable;
	desc.AntialiasedLineEnable = antialiasedLineEnable;
	desc.FrontCounterClockwise = false;
	desc.MultisampleEnable = false;
	desc.ScissorEnable = false;
	desc.DepthBias = 0;
	desc.DepthBiasClamp = 0.0f;
	desc.SlopeScaledDepthBias = 0.0f;

	ComPtr<ID3D11RasterizerState> state;
	m_device->CreateRasterizerState(&desc, &state);
	return state;
}

ComPtr<ID3D11DepthStencilState> BasePass::createDSState(DepthStencilPreset preset)
{
	switch (preset)
	{
	case DepthStencilPreset::WriteDepth:
		return createDSState(true, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_LESS);
	case DepthStencilPreset::ReadOnlyEqual:
		return createDSState(true, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_EQUAL);
	case DepthStencilPreset::ReadOnlyLessEqual:
		return createDSState(true, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_LESS_EQUAL);
	case DepthStencilPreset::Disabled:
		return createDSState(false, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_ALWAYS);
	default:
		return createDSState(true, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_LESS);
	}
}

ComPtr<ID3D11DepthStencilState> BasePass::createDSState(const bool depthEnable,
	const D3D11_DEPTH_WRITE_MASK writeMask,
	const D3D11_COMPARISON_FUNC depthFunc) const
{
	D3D11_DEPTH_STENCIL_DESC desc = {};
	desc.DepthEnable = depthEnable;
	desc.DepthWriteMask = writeMask;
	desc.DepthFunc = depthFunc;
	desc.StencilEnable = false;
	desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

	ComPtr<ID3D11DepthStencilState> state;
	m_device->CreateDepthStencilState(&desc, &state);
	return state;
}

ComPtr<ID3D11SamplerState> BasePass::createSamplerState(const SamplerPreset preset)
{
	switch (preset)
	{
	case SamplerPreset::LinearWrap:
		return createSamplerState(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
	case SamplerPreset::LinearClamp:
		return createSamplerState(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
	case SamplerPreset::PointWrap:
		return createSamplerState(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_WRAP);
	case SamplerPreset::PointClamp:
		return createSamplerState(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
	case SamplerPreset::AnisotropicWrap:
		return createSamplerState(D3D11_FILTER_ANISOTROPIC, D3D11_TEXTURE_ADDRESS_WRAP);
	default:
		return createSamplerState(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
	}
}

ComPtr<ID3D11DepthStencilView> BasePass::createDSView(ID3D11Texture2D* texture, DXGI_FORMAT format)
{
	D3D11_DEPTH_STENCIL_VIEW_DESC desc = {};
	desc.Format = format;
	desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	desc.Texture2D.MipSlice = 0;

	ComPtr<ID3D11DepthStencilView> view;
	m_device->CreateDepthStencilView(texture, &desc, &view);
	return view;
}

ComPtr<ID3D11SamplerState> BasePass::createSamplerState(const D3D11_FILTER filter,
	const D3D11_TEXTURE_ADDRESS_MODE addressMode)
{
	D3D11_SAMPLER_DESC desc = {};
	desc.Filter = filter;
	desc.AddressU = addressMode;
	desc.AddressV = addressMode;
	desc.AddressW = addressMode;
	desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	desc.MinLOD = 0;
	desc.MaxLOD = D3D11_FLOAT32_MAX;
	desc.MaxAnisotropy = (filter == D3D11_FILTER_ANISOTROPIC) ? 16 : 1;

	ComPtr<ID3D11SamplerState> state;
	m_device->CreateSamplerState(&desc, &state);
	return state;
}

ComPtr<ID3D11BlendState> BasePass::createBlendState(BlendPreset preset)
{
	switch (preset)
	{
	case BlendPreset::Opaque:
		return createBlendState(false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD,
			D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD);
	case BlendPreset::AlphaBlend:
		return createBlendState(true, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
			D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD);
	case BlendPreset::Additive:
		return createBlendState(true, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD,
			D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD);
	case BlendPreset::Multiply:
		return createBlendState(true, D3D11_BLEND_DEST_COLOR, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD,
			D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD);
	default:
		return createBlendState(false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD,
			D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD);
	}
}

ComPtr<ID3D11BlendState> BasePass::createBlendState(bool blendEnable,
	D3D11_BLEND srcBlend,
	D3D11_BLEND destBlend,
	D3D11_BLEND_OP blendOp,
	D3D11_BLEND srcBlendAlpha,
	D3D11_BLEND destBlendAlpha,
	D3D11_BLEND_OP blendOpAlpha,
	UINT sampleMask) const
{
	D3D11_BLEND_DESC desc = {};
	desc.RenderTarget[0].BlendEnable = blendEnable;
	desc.RenderTarget[0].SrcBlend = srcBlend;
	desc.RenderTarget[0].DestBlend = destBlend;
	desc.RenderTarget[0].BlendOp = blendOp;
	desc.RenderTarget[0].SrcBlendAlpha = srcBlendAlpha;
	desc.RenderTarget[0].DestBlendAlpha = destBlendAlpha;
	desc.RenderTarget[0].BlendOpAlpha = blendOpAlpha;
	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	desc.AlphaToCoverageEnable = false;
	desc.IndependentBlendEnable = false;

	ComPtr<ID3D11BlendState> state;
	m_device->CreateBlendState(&desc, &state);
	return state;
}

ComPtr<ID3D11Texture2D> BasePass::createTexture2D(UINT width,
	UINT height,
	DXGI_FORMAT format,
	UINT bindFlags,
	UINT mipLevels,
	UINT arraySize,
	D3D11_USAGE usage,
	UINT cpuAccessFlags,
	UINT miscFlags,
	const D3D11_SUBRESOURCE_DATA* initialData) const
{
	// Detect typeless depth formats and adjust bind flags accordingly
	UINT finalBindFlags = bindFlags;
	switch (format)
	{
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		// For typeless depth formats, ensure depth stencil binding is included
		finalBindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		break;
	default:
		break;
	}

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = mipLevels;
	desc.ArraySize = arraySize;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = usage;
	desc.BindFlags = finalBindFlags;
	desc.CPUAccessFlags = cpuAccessFlags;
	desc.MiscFlags = miscFlags;

	ComPtr<ID3D11Texture2D> texture;
	HRESULT hr = m_device->CreateTexture2D(&desc, initialData, &texture);
	if (FAILED(hr))
	{
		std::cerr << "Failed to create Texture2D: " << hr << std::endl;
		return nullptr;
	}
	return texture;
}

ComPtr<ID3D11Buffer> BasePass::createConstantBuffer(UINT byteSize) const
{
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = byteSize;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;

	ComPtr<ID3D11Buffer> buffer;
	HRESULT hr = m_device->CreateBuffer(&desc, nullptr, &buffer);
	if (FAILED(hr))
	{
		return nullptr;
	}
	return buffer;
}

ComPtr<ID3D11Buffer> BasePass::createVertexBuffer(UINT byteSize, const void* initialData) const
{
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = byteSize;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = initialData;

	ComPtr<ID3D11Buffer> buffer;
	HRESULT hr = m_device->CreateBuffer(&desc, initialData ? &initData : nullptr, &buffer);
	if (FAILED(hr))
	{
		return nullptr;
	}
	return buffer;
}

ComPtr<ID3D11Buffer> BasePass::createIndexBuffer(UINT byteSize, const void* initialData) const
{
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = byteSize;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = initialData;

	ComPtr<ID3D11Buffer> buffer;
	HRESULT hr = m_device->CreateBuffer(&desc, initialData ? &initData : nullptr, &buffer);
	if (FAILED(hr))
	{
		return nullptr;
	}
	return buffer;
}

ComPtr<ID3D11Buffer> BasePass::createStructuredBuffer(UINT elementSize,
	UINT elementCount,
	SBPreset preset,
	const void* initialData) const
{
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = elementSize * elementCount;

	// Determine usage based on CPU access needs
	switch (preset)
	{
	case SBPreset::CpuRead:
	{
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		break;
	}
	case SBPreset::CpuWrite:
	{
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		break;
	}
	case SBPreset::Immutable:
	{
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		break;
	}
	default:
	{
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		desc.CPUAccessFlags = 0;
		break;
	}
	}

	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = elementSize;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = initialData;

	ComPtr<ID3D11Buffer> buffer;
	HRESULT hr = m_device->CreateBuffer(&desc, initialData ? &initData : nullptr, &buffer);
	if (FAILED(hr))
	{
		return nullptr;
	}
	return buffer;
}

ComPtr<ID3D11RenderTargetView> BasePass::createRenderTargetView(ID3D11Texture2D* texture, RTVPreset preset, UINT mipSlice, UINT arraySlice) const
{
	D3D11_RENDER_TARGET_VIEW_DESC desc = {};

	D3D11_TEXTURE2D_DESC texDesc;
	texture->GetDesc(&texDesc);
	desc.Format = texDesc.Format;

	switch (preset)
	{
	case RTVPreset::Texture2D:
	{
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = mipSlice;
		break;
	}
	case RTVPreset::Texture2DArray:
	{
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MipSlice = mipSlice;
		desc.Texture2DArray.FirstArraySlice = arraySlice;
		desc.Texture2DArray.ArraySize = 1;
		break;
	}
	case RTVPreset::Texture2DMS:
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
		break;
	case RTVPreset::Texture3D:
	{

		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
		desc.Texture3D.MipSlice = mipSlice;
		desc.Texture3D.FirstWSlice = 0;
		desc.Texture3D.WSize = -1;
		break;
	}
	default:
		return nullptr;
	}

	ComPtr<ID3D11RenderTargetView> rtv;
	HRESULT hr = m_device->CreateRenderTargetView(texture, &desc, &rtv);
	if (FAILED(hr))
	{
		return nullptr;
	}
	return rtv;
}

ComPtr<ID3D11DepthStencilView> BasePass::createDepthStencilView(ID3D11Texture2D* texture, DSVPreset preset, UINT mipSlice, UINT arraySlice) const
{
	D3D11_DEPTH_STENCIL_VIEW_DESC desc = {};

	D3D11_TEXTURE2D_DESC texDesc;
	texture->GetDesc(&texDesc);

	// Convert typeless depth formats to proper DSV formats
	switch (texDesc.Format)
	{
	case DXGI_FORMAT_R16_TYPELESS:
		desc.Format = DXGI_FORMAT_D16_UNORM;
		break;
	case DXGI_FORMAT_R24G8_TYPELESS:
		desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		break;
	case DXGI_FORMAT_R32_TYPELESS:
		desc.Format = DXGI_FORMAT_D32_FLOAT;
		break;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		break;
	default:
		desc.Format = texDesc.Format;
		break;
	}

	switch (preset)
	{
	case DSVPreset::Texture2D:
		desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = mipSlice;
		desc.Flags = 0;
		break;
	case DSVPreset::Texture2DReadOnly:
		desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = mipSlice;
		desc.Flags = D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL;
		break;
	default:
		return nullptr;
	}

	ComPtr<ID3D11DepthStencilView> dsv;
	HRESULT hr = m_device->CreateDepthStencilView(texture, &desc, &dsv);
	if (FAILED(hr))
	{
		return nullptr;
	}
	return dsv;
}

ComPtr<ID3D11ShaderResourceView> BasePass::createShaderResourceView(ID3D11Texture2D* texture, SRVPreset preset, UINT mostDetailedMip, UINT mipLevels) const
{
	D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};

	D3D11_TEXTURE2D_DESC texDesc;
	texture->GetDesc(&texDesc);

	// Convert typeless depth formats to proper SRV formats
	switch (texDesc.Format)
	{
	case DXGI_FORMAT_R16_TYPELESS:
		desc.Format = DXGI_FORMAT_R16_UNORM;
		break;
	case DXGI_FORMAT_R24G8_TYPELESS:
		desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		break;
	case DXGI_FORMAT_R32_TYPELESS:
		desc.Format = DXGI_FORMAT_R32_FLOAT;
		break;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
		break;
	default:
		desc.Format = texDesc.Format;
		break;
	}

	switch (preset)
	{
	case SRVPreset::Texture2D:
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MostDetailedMip = mostDetailedMip;
		desc.Texture2D.MipLevels = mipLevels == 0 ? texDesc.MipLevels : mipLevels;
		break;
	case SRVPreset::Texture2DArray:
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MostDetailedMip = mostDetailedMip;
		desc.Texture2DArray.MipLevels = mipLevels == 0 ? texDesc.MipLevels : mipLevels;
		desc.Texture2DArray.FirstArraySlice = 0;
		desc.Texture2DArray.ArraySize = texDesc.ArraySize;
		break;
	case SRVPreset::TextureCube:
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		desc.TextureCube.MostDetailedMip = mostDetailedMip;
		desc.TextureCube.MipLevels = mipLevels == 0 ? texDesc.MipLevels : mipLevels;
		break;
	default:
		return nullptr;
	}

	ComPtr<ID3D11ShaderResourceView> srv;
	HRESULT hr = m_device->CreateShaderResourceView(texture, &desc, &srv);
	if (FAILED(hr))
	{
		return nullptr;
	}
	return srv;
}

ComPtr<ID3D11ShaderResourceView> BasePass::createShaderResourceView(ID3D11Buffer* buffer, SRVPreset preset, UINT firstElement, UINT numElements) const
{
	D3D11_BUFFER_DESC bufDesc;
	buffer->GetDesc(&bufDesc);

	D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;

	switch (preset)
	{
	case SRVPreset::StructuredBuffer:
		desc.Buffer.FirstElement = firstElement;
		desc.Buffer.NumElements = numElements == 0 ? (bufDesc.ByteWidth / bufDesc.StructureByteStride) : numElements;
		break;
	default:
		return nullptr;
	}

	ComPtr<ID3D11ShaderResourceView> srv;
	HRESULT hr = m_device->CreateShaderResourceView(buffer, &desc, &srv);
	if (FAILED(hr))
	{
		return nullptr;
	}
	return srv;
}

ComPtr<ID3D11UnorderedAccessView> BasePass::createUnorderedAccessView(ID3D11Texture2D* texture, UAVPreset preset, UINT mipSlice) const
{
	D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {};

	D3D11_TEXTURE2D_DESC texDesc;
	texture->GetDesc(&texDesc);
	desc.Format = texDesc.Format;

	switch (preset)
	{
	case UAVPreset::Texture2D:
		desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = mipSlice;
		break;
	case UAVPreset::Texture2DArray:
		desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MipSlice = mipSlice;
		desc.Texture2DArray.FirstArraySlice = 0;
		desc.Texture2DArray.ArraySize = texDesc.ArraySize;
		break;
	case UAVPreset::Texture3D:
		desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
		desc.Texture3D.MipSlice = mipSlice;
		desc.Texture3D.FirstWSlice = 0;
		desc.Texture3D.WSize = -1;
		break;
	default:
		return nullptr;
	}

	ComPtr<ID3D11UnorderedAccessView> uav;
	HRESULT hr = m_device->CreateUnorderedAccessView(texture, &desc, &uav);
	if (FAILED(hr))
	{
		return nullptr;
	}
	return uav;
}

ComPtr<ID3D11UnorderedAccessView> BasePass::createUnorderedAccessView(ID3D11Buffer* buffer, UAVPreset preset, UINT firstElement, UINT numElements) const
{
	D3D11_BUFFER_DESC bufDesc;
	buffer->GetDesc(&bufDesc);

	D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {};
	desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;

	switch (preset)
	{
	case UAVPreset::StructuredBuffer:
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.Buffer.FirstElement = firstElement;
		desc.Buffer.NumElements = numElements == 0 ? (bufDesc.ByteWidth / bufDesc.StructureByteStride) : numElements;
		desc.Buffer.Flags = 0;
		break;
	case UAVPreset::RawBuffer:
		desc.Format = DXGI_FORMAT_R32_TYPELESS;
		desc.Buffer.FirstElement = firstElement;
		desc.Buffer.NumElements = numElements == 0 ? (bufDesc.ByteWidth / 4) : numElements;
		desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
		break;
	default:
		return nullptr;
	}

	ComPtr<ID3D11UnorderedAccessView> uav;
	HRESULT hr = m_device->CreateUnorderedAccessView(buffer, &desc, &uav);
	if (FAILED(hr))
	{
		return nullptr;
	}
	return uav;
}

void BasePass::setViewport(const UINT width, const UINT height) const
{
	D3D11_VIEWPORT viewport = {};
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	m_context->RSSetViewports(1, &viewport);
}

void BasePass::unbindRenderTargets(const UINT count) const
{
	const std::vector<ID3D11RenderTargetView*> nullRTVs(count, nullptr);
	m_context->OMSetRenderTargets(count, nullRTVs.data(), nullptr);
}

void BasePass::unbindShaderResources(const UINT startSlot, const UINT count)
{
	const std::vector<ID3D11ShaderResourceView*> nullSRVs(count, nullptr);
	m_context->PSSetShaderResources(startSlot, count, nullSRVs.data());
	m_context->VSSetShaderResources(startSlot, count, nullSRVs.data());
	m_context->CSSetShaderResources(startSlot, count, nullSRVs.data());
}

void BasePass::unbindComputeUAVs(const UINT startSlot, const UINT count) const
{
	if (count == 0)
		return;
	const std::vector<ID3D11UnorderedAccessView*> nullUAVs(count, nullptr);
	m_context->CSSetUnorderedAccessViews(startSlot, count, nullUAVs.data(), nullptr);
}

void BasePass::beginDebugEvent(const wchar_t* name) const
{
	if (m_annotation)
	{
		m_annotation->BeginEvent(name);
	}
}

void BasePass::endDebugEvent() const
{
	if (m_annotation)
	{
		m_annotation->EndEvent();
	}
}
