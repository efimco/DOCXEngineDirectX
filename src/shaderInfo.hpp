#pragma once
#include <string>
#include <filesystem>
#include <wrl.h>
#include <d3d11.h>

using namespace Microsoft::WRL;

struct ShaderInfo
{
	std::wstring filename;
	std::string entryPoint;
	std::filesystem::file_time_type lastModifiedTime;
	std::string shaderModelVersion;
	ComPtr<ID3D11VertexShader> vertexShader;
	ComPtr<ID3D11PixelShader> pixelShader;
	ComPtr<ID3D11ComputeShader> computeShader;
	ComPtr<ID3DBlob> blob;
};