#include "dxCompiler.hpp"
#include <assert.h>
#include <d3dcompiler.h>
#include <iostream>

DXCompiler::DXCompiler(ComPtr<ID3D11Device> &device) : m_device(device) {}

void DXCompiler::CompileFromFile(LPCWSTR pFileName,
								 D3D_SHADER_MACRO *pDefines,
								 ID3DInclude *pInclude,
								 LPCSTR pEntrypoint,
								 LPCSTR pTarget,
								 UINT flags1,
								 UINT flags2,
								 ID3D11PixelShader **ppPixelShader)
{
	ComPtr<ID3DBlob> shaderCompileErrorsBlob;
	HRESULT hr = D3DCompileFromFile(pFileName, pDefines, pInclude, pEntrypoint, pTarget, flags1, flags2, &m_psBlob, &shaderCompileErrorsBlob);
	if (FAILED(hr))
	{
		const char *errorString;
		if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			errorString = "Could not compile shader; file not found";
		}
		else if (shaderCompileErrorsBlob)
		{
			errorString = (const char *)shaderCompileErrorsBlob->GetBufferPointer();
			std::cout << errorString << std::endl;
		}
	}
	assert(SUCCEEDED(hr));
	hr = m_device->CreatePixelShader(m_psBlob->GetBufferPointer(), m_psBlob->GetBufferSize(), nullptr, ppPixelShader);
	assert(SUCCEEDED(hr));
}

void DXCompiler::CompileFromFile(LPCWSTR pFileName,
								 D3D_SHADER_MACRO *pDefines,
								 ID3DInclude *pInclude,
								 LPCSTR pEntrypoint,
								 LPCSTR pTarget,
								 UINT flags1,
								 UINT flags2,
								 ID3D11VertexShader **ppVertexShader)
{
	ComPtr<ID3DBlob> shaderCompileErrorsBlob;
	HRESULT hr = D3DCompileFromFile(pFileName, pDefines, pInclude, pEntrypoint, pTarget, flags1, flags2, &m_vsBlob, &shaderCompileErrorsBlob);
	if (FAILED(hr))
	{
		const char *errorString;
		if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			errorString = "Could not compile shader; file not found";
		}
		else if (shaderCompileErrorsBlob)
		{
			errorString = (const char *)shaderCompileErrorsBlob->GetBufferPointer();
			std::cout << errorString << std::endl;
		}
	}
	assert(SUCCEEDED(hr));
	hr = m_device->CreateVertexShader(m_vsBlob->GetBufferPointer(), m_vsBlob->GetBufferSize(), nullptr, ppVertexShader);
	assert(SUCCEEDED(hr));
}

ComPtr<ID3DBlob> &DXCompiler::getVsBlob()
{
	return m_vsBlob;
}

ComPtr<ID3DBlob> &DXCompiler::getPsBlob()
{
	return m_psBlob;
}

ComPtr<ID3DBlob> &DXCompiler::getCsBlob()
{
	return m_csBlob;
}
