#pragma once

#include <d3d11.h>
#include <wrl.h>

using namespace Microsoft::WRL;
class DXDevice
{
public:
	DXDevice(HWND &hWindow);
	~DXDevice() = default;

	ComPtr<ID3D11DeviceContext> &getContext();
	ComPtr<IDXGISwapChain> &getSwapChain();
	ComPtr<ID3D11Device> &getDevice();

private:
	ComPtr<ID3D11Device> m_d3dDevice;
	ComPtr<ID3D11DeviceContext> m_d3dDeviceContext;
	ComPtr<IDXGISwapChain> m_dxgiSwapChain;
};