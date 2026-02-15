#pragma once

#include <d3d11_4.h>
#include <wrl.h>

using namespace Microsoft::WRL;

struct GBufferTextures
{

	ComPtr<ID3D11ShaderResourceView> albedoSRV;
	ComPtr<ID3D11ShaderResourceView> metallicRoughnessSRV;
	ComPtr<ID3D11ShaderResourceView> normalSRV;
	ComPtr<ID3D11ShaderResourceView> viewNormalSRV;
	ComPtr<ID3D11ShaderResourceView> positionSRV;
	ComPtr<ID3D11ShaderResourceView> viewPositionSRV;
	ComPtr<ID3D11ShaderResourceView> objectIDSRV;

};
