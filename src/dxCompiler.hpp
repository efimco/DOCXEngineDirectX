#pragma once
#include <d3d11.h>
#include <wrl.h>

using namespace Microsoft::WRL;
class DXCompiler
{
public:
	DXCompiler(ComPtr<ID3D11Device> &device);

	void CompileFromFile(LPCWSTR pFileName, D3D_SHADER_MACRO *pDefines,
						 ID3DInclude *pInclude,
						 LPCSTR pEntrypoint,
						 LPCSTR pTarget,
						 UINT flags1, UINT flags2,
						 ID3D11PixelShader **ppPixelShader);

	void CompileFromFile(LPCWSTR pFileName, D3D_SHADER_MACRO *pDefines,
						 ID3DInclude *pInclude,
						 LPCSTR pEntrypoint,
						 LPCSTR pTarget,
						 UINT flags1, UINT flags2,
						 ID3D11VertexShader **ppVertexShader);

	void CompileFromFile(LPCWSTR pFileName, D3D_SHADER_MACRO *pDefines,
						 ID3DInclude *pInclude,
						 LPCSTR pEntrypoint,
						 LPCSTR pTarget,
						 UINT flags1, UINT flags2,
						 ID3D11ComputeShader **ppComputeShader);
	ComPtr<ID3DBlob> &getVsBlob();
	ComPtr<ID3DBlob> &getPsBlob();
	ComPtr<ID3DBlob> &getCsBlob();

private:
	ComPtr<ID3D11Device> &m_device;
	ComPtr<ID3DBlob> m_vsBlob;
	ComPtr<ID3DBlob> m_psBlob;
	ComPtr<ID3DBlob> m_csBlob;
};