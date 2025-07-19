#include "DXDevice.hpp"
#include <assert.h>
DXDevice::DXDevice(HWND &hWindow)

{
	D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_12_1};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);
	UINT deviceCreationFlags = 0;
#if defined(_DEBUG)
	// If the project is in a debug build, enable the debug layer.
	deviceCreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	{
		HRESULT hr = D3D11CreateDevice(
			nullptr,				  // IDXGIAdapter* pAdapter
			D3D_DRIVER_TYPE_HARDWARE, // D3D_DRIVER_TYPE DriverType
			nullptr,				  // HMODULE Software
			deviceCreationFlags,	  // UINT flags
			featureLevels,			  // CONST D3D_FEATURE_LEVEL* pFeatureLevels
			numFeatureLevels,		  // UINT FeatureLevels
			D3D11_SDK_VERSION,		  // UINT SDKVersion,
			&m_d3dDevice,			  //_COM_Outptr_opt_ ID3D11Device** ppDevice
			nullptr,				  // D3D_FEATURE_LEVEL* pFeatureLevel
			&m_d3dDeviceContext);	  // ID3D11DeviceContext** ppImmediateContext
		assert(SUCCEEDED(hr));
	}

	ComPtr<IDXGIDevice> dxgiDevice;
	{
		HRESULT hr = m_d3dDevice->QueryInterface(__uuidof(IDXGIDevice), &dxgiDevice);
		assert(SUCCEEDED(hr));
	}

	ComPtr<IDXGIAdapter> dxgiAdapter;
	{
		HRESULT hr = dxgiDevice->GetAdapter(&dxgiAdapter);
		assert(SUCCEEDED(hr));
	}

	ComPtr<IDXGIFactory> dxgiFactory;
	{
		HRESULT hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory), &dxgiFactory);
		assert(SUCCEEDED(hr));
	}

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};

	swapChainDesc.BufferDesc.Width = 0;
	swapChainDesc.BufferDesc.Height = 0;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = 0;
	swapChainDesc.OutputWindow = hWindow;
	swapChainDesc.Windowed = true;
	HRESULT hr = dxgiFactory->CreateSwapChain(m_d3dDevice.Get(), &swapChainDesc, &m_dxgiSwapChain);
	assert(SUCCEEDED(hr));
}

ComPtr<ID3D11DeviceContext> &DXDevice::getContext()
{
	return m_d3dDeviceContext;
}

ComPtr<IDXGISwapChain> &DXDevice::getSwapChain()
{
	return m_dxgiSwapChain;
}

ComPtr<ID3D11Device> &DXDevice::getDevice()
{
	return m_d3dDevice;
}
