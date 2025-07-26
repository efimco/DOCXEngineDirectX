#include "shaderManager.hpp"
#include <filesystem>
#include <iostream>
#include <d3dcompiler.h>

ShaderManager::ShaderManager(ComPtr<ID3D11Device>& device) : m_device(device) {}

bool ShaderManager::LoadVertexShader(const std::string& name, const std::wstring& filename, const std::string& entryPoint)
{

	ShaderInfo info;
	info.filename = filename;

	info.entryPoint = entryPoint;
	info.shaderModelVersion = "vs_5_0";
	info.lastModifiedTime = getFileModifiedTime(filename);

	if (compileShader(info, true))
	{
		m_vertexShaders[name] = std::move(info);
		std::wcout << L"Loaded vertex shader: " << filename << std::endl;
		return true;
	}

	std::wcout << L"Failed to load vertex shader: " << filename << std::endl;
	return false;
}

bool ShaderManager::LoadPixelShader(const std::string& name, const std::wstring& filename, const std::string& entryPoint)
{

	ShaderInfo info;
	info.filename = filename;

	info.entryPoint = entryPoint;
	info.shaderModelVersion = "ps_5_0";
	info.lastModifiedTime = getFileModifiedTime(filename);

	if (compileShader(info, false))
	{
		m_pixelShaders[name] = std::move(info);
		std::wcout << L"Loaded pixel shader: " << filename << std::endl;
		return true;
	}

	std::wcout << L"Failed to load pixel shader: " << filename << std::endl;
	return false;
}

ID3D11VertexShader* ShaderManager::getVertexShader(const std::string& name)
{
	auto it = m_vertexShaders.find(name);
	return (it != m_vertexShaders.end()) ? it->second.vertexShader.Get() : nullptr;
}

ID3D11PixelShader* ShaderManager::getPixelShader(const std::string& name)
{
	auto it = m_pixelShaders.find(name);
	return (it != m_pixelShaders.end()) ? it->second.pixelShader.Get() : nullptr;
}

ID3DBlob* ShaderManager::getVertexShaderBlob(const std::string& name)
{
	auto it = m_vertexShaders.find(name);
	return (it != m_vertexShaders.end()) ? it->second.blob.Get() : nullptr;
}

void ShaderManager::checkForChanges()
{
	// Check vertex shaders
	for (auto& [name, info] : m_vertexShaders)
	{
		auto currentTime = getFileModifiedTime(info.filename);
		if (currentTime > info.lastModifiedTime)
		{
			std::wcout << L"Recompiling vertex shader: " << info.filename << std::endl;
			if (compileShader(info, true))
			{
				info.lastModifiedTime = currentTime;
				std::wcout << L"Successfully recompiled vertex shader: " << info.filename << std::endl;
			}
			else
			{
				std::wcout << L"Failed to recompile vertex shader: " << info.filename << std::endl;
			}
		}
	}

	// Check pixel shaders
	for (auto& [name, info] : m_pixelShaders)
	{
		auto currentTime = getFileModifiedTime(info.filename);
		if (currentTime > info.lastModifiedTime)
		{
			std::wcout << L"Recompiling pixel shader: " << info.filename << std::endl;
			if (compileShader(info, false))
			{
				info.lastModifiedTime = currentTime;
				std::wcout << L"Successfully recompiled pixel shader: " << info.filename << std::endl;
			}
			else
			{
				std::wcout << L"Failed to recompile pixel shader: " << info.filename << std::endl;
			}
		}
	}
}

void ShaderManager::recompileAll()
{
	std::cout << "Recompiling all shaders..." << std::endl;

	for (auto& [name, info] : m_vertexShaders)
	{
		compileShader(info, true);
		info.lastModifiedTime = getFileModifiedTime(info.filename);
	}

	for (auto& [name, info] : m_pixelShaders)
	{
		compileShader(info, false);
		info.lastModifiedTime = getFileModifiedTime(info.filename);
	}
}


bool ShaderManager::compileShader(ShaderInfo& info, bool isVertexShader)
{
	ComPtr<ID3DBlob> shaderBlob;
	ComPtr<ID3DBlob> errorBlob;

	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT hr = D3DCompileFromFile(
		info.filename.c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		info.entryPoint.c_str(),
		info.shaderModelVersion.c_str(),
		flags,
		0,
		&shaderBlob,
		&errorBlob
	);

	if (FAILED(hr))
	{
		if (errorBlob)
		{
			std::cout << "Shader compilation error:\n"
				<< (char*)errorBlob->GetBufferPointer() << std::endl;
		}
		return false;
	}

	if (isVertexShader)
	{
		hr = m_device->CreateVertexShader(
			shaderBlob->GetBufferPointer(),
			shaderBlob->GetBufferSize(),
			nullptr,
			&info.vertexShader
		);
	}
	else
	{
		hr = m_device->CreatePixelShader(
			shaderBlob->GetBufferPointer(),
			shaderBlob->GetBufferSize(),
			nullptr,
			&info.pixelShader
		);
	}

	if (SUCCEEDED(hr))
	{
		info.blob = shaderBlob;
		return true;
	}

	return false;
}


std::filesystem::file_time_type ShaderManager::getFileModifiedTime(const std::wstring& filename)
{
	try
	{
		return std::filesystem::last_write_time(filename);
	}
	catch (const std::filesystem::filesystem_error&)
	{
		return (std::filesystem::file_time_type::min)();
	}
}
