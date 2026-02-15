#pragma once
#include <d3d11.h>
#include <d3d11_4.h>
#include <memory>
#include <wrl.h>

class ShaderManager;

enum class RasterizerPreset
{
	Default,	  // CullNone, Solid, DepthClip=true
	NoCullNoClip, // CullNone, Solid, DepthClip=false (GBuffer, WorldSpaceUI)
	BackCull,	  // CullBack, Solid (CubeMap)
	FrontCull,	  // CullFront, Solid (CubeMap)
	Wireframe	  // CullNone, Wireframe, AntialiasedLine (DebugBVH)
};

enum class DepthStencilPreset
{
	WriteDepth,		   // DepthWrite=All, Func=Less (ZPrePass)
	ReadOnlyEqual,	   // DepthWrite=Zero, Func=Equal (GBuffer, CubeMap)
	ReadOnlyLessEqual, // DepthWrite=Zero, Func=LessEqual (DebugBVH)
	Disabled		   // DepthEnable=false
};

enum class SamplerPreset
{
	LinearWrap,		 // Linear filter, Wrap
	LinearClamp,	 // Linear filter, Clamp
	PointWrap,		 // Point filter, Wrap
	PointClamp,		 // Point filter, Clamp
	AnisotropicWrap, // Anisotropic filter, Wrap

};

enum class BlendPreset
{
	Opaque,		 // BlendEnable=false
	AlphaBlend,	 // BlendEnable=true, SrcBlend=SrcAlpha, DestBlend=InvSrcAlpha
	Additive,	 // BlendEnable=true, SrcBlend=One, DestBlend=One
	Multiply,	 // BlendEnable=true, SrcBlend=Zero, DestBlend=SrcColor
	AlphaPremul, // BlendEnable=true, SrcBlend=One, DestBlend=InvSrcAlpha
};

enum class RTVPreset
{
	Texture2D,		// Standard 2D texture RTV
	Texture2DArray, // 2D texture array RTV
	Texture2DMS,	// 2D multisampled texture RTV
	Texture3D		// 3D texture RTV
};

enum class DSVPreset
{
	Texture2D,		  // Standard 2D depth-stencil view
	Texture2DReadOnly // Read-only depth-stencil view
};

enum class SRVPreset
{
	Texture2D,		 // Standard 2D texture SRV
	Texture2DArray,	 // 2D texture array SRV
	TextureCube,	 // Cube texture SRV
	StructuredBuffer // Structured buffer SRV
};

enum class UAVPreset
{
	Texture2D,		  // Standard 2D texture UAV
	Texture2DArray,	  // 2D texture array UAV
	Texture3D,		  // 3D texture UAV
	StructuredBuffer, // Structured buffer UAV
	RawBuffer		  // Raw buffer UAV (ByteAddressBuffer)
};

enum class SBPreset
{
	CpuRead,
	CpuWrite,
	Default,
	Immutable
};


using namespace Microsoft::WRL;

class BasePass
{
public:
	explicit BasePass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
	virtual ~BasePass() = default;

	BasePass(const BasePass& other) = delete;
	BasePass& operator=(const BasePass& other) = delete;

protected:
	ComPtr<ID3D11RasterizerState> createRSState(RasterizerPreset preset) const;
	ComPtr<ID3D11RasterizerState> createRSState(D3D11_CULL_MODE cullMode,
		D3D11_FILL_MODE fillMode,
		bool depthClipEnable = true,
		bool antialiasedLineEnable = false) const;

	ComPtr<ID3D11DepthStencilState> createDSState(DepthStencilPreset preset);
	ComPtr<ID3D11DepthStencilState> createDSState(bool depthEnable,
		D3D11_DEPTH_WRITE_MASK writeMask,
		D3D11_COMPARISON_FUNC depthFunc) const;
	ComPtr<ID3D11DepthStencilView> createDSView(ID3D11Texture2D* texture, DXGI_FORMAT format);

	ComPtr<ID3D11SamplerState> createSamplerState(SamplerPreset preset);
	ComPtr<ID3D11SamplerState> createSamplerState(D3D11_FILTER filter,
		D3D11_TEXTURE_ADDRESS_MODE addressMode);

	ComPtr<ID3D11BlendState> createBlendState(BlendPreset preset);
	ComPtr<ID3D11BlendState> createBlendState(bool blendEnable,
		D3D11_BLEND srcBlend,
		D3D11_BLEND destBlend,
		D3D11_BLEND_OP blendOp,
		D3D11_BLEND srcBlendAlpha,
		D3D11_BLEND destBlendAlpha,
		D3D11_BLEND_OP blendOpAlpha,
		UINT sampleMask = 0xFFFFFFFF) const;

	// Texture creation helpers
	ComPtr<ID3D11Texture2D> createTexture2D(UINT width,
		UINT height,
		DXGI_FORMAT format,
		UINT bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
		UINT mipLevels = 1,
		UINT arraySize = 1,
		D3D11_USAGE usage = D3D11_USAGE_DEFAULT,
		UINT cpuAccessFlags = 0,
		UINT miscFlags = 0,
		const D3D11_SUBRESOURCE_DATA* initialData = nullptr) const;

	// Buffer creation helpers
	ComPtr<ID3D11Buffer> createConstantBuffer(UINT byteSize) const;
	ComPtr<ID3D11Buffer> createVertexBuffer(UINT byteSize, const void* initialData = nullptr) const;
	ComPtr<ID3D11Buffer> createIndexBuffer(UINT byteSize, const void* initialData = nullptr) const;
	ComPtr<ID3D11Buffer> createStructuredBuffer(UINT elementSize,
		UINT elementCount,
		SBPreset preset,
		const void* initialData = nullptr) const;

	// View creation helpers - with presets
	ComPtr<ID3D11RenderTargetView> createRenderTargetView(ID3D11Texture2D* texture,
		RTVPreset preset,
		UINT mipSlice = 0,
		UINT arraySlice = 0) const;
	ComPtr<ID3D11DepthStencilView> createDepthStencilView(ID3D11Texture2D* texture,
		DSVPreset preset,
		UINT mipSlice = 0,
		UINT arraySlice = 0) const;

	ComPtr<ID3D11ShaderResourceView> createShaderResourceView(ID3D11Texture2D* texture, // for texture
		SRVPreset preset,
		UINT mostDetailedMip = 0,
		UINT mipLevels = 1) const;
	ComPtr<ID3D11ShaderResourceView> createShaderResourceView(ID3D11Buffer* buffer, // for buffer
		SRVPreset preset,
		UINT firstElement = 0,
		UINT numElements = 0) const;

	ComPtr<ID3D11UnorderedAccessView> createUnorderedAccessView(ID3D11Texture2D* texture, // for texture
		UAVPreset preset,
		UINT mipSlice = 0) const;
	ComPtr<ID3D11UnorderedAccessView> createUnorderedAccessView(ID3D11Buffer* buffer, // for buffer
		UAVPreset preset,
		UINT firstElement = 0,
		UINT numElements = 0) const;

	void setViewport(UINT width, UINT height) const;
	void unbindRenderTargets(UINT count) const;
	void unbindShaderResources(UINT startSlot, UINT count);
	void unbindComputeUAVs(UINT startSlot, UINT count) const;

	// Debug annotation (for graphics debuggers like RenderDoc/PIX)
	void beginDebugEvent(const wchar_t* name) const;
	void endDebugEvent() const;


	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;
	std::unique_ptr<ShaderManager> m_shaderManager;

	ComPtr<ID3D11RasterizerState> m_rasterizerState;
	ComPtr<ID3D11DepthStencilState> m_depthStencilState;
	ComPtr<ID3D11SamplerState> m_samplerState;
	ComPtr<ID3D11BlendState> m_blendState;

private:
	ComPtr<ID3DUserDefinedAnnotation> m_annotation;
};
