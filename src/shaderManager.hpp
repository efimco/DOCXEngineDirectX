#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <chrono>

#include "shaderInfo.hpp"

using namespace Microsoft::WRL;

class ShaderManager
{
public:
	ShaderManager(ComPtr<ID3D11Device>& device);

	bool LoadVertexShader(const std::string& name, const std::wstring& filename, const std::string& entryPoint = "VS");
	bool LoadPixelShader(const std::string& name, const std::wstring& filename, const std::string& entryPoint = "PS");

	ID3D11VertexShader* getVertexShader(const std::string& name);
	ID3D11PixelShader* getPixelShader(const std::string& name);
	ID3D11ComputeShader* getComputeShader(const std::string& name);
	ID3DBlob* getVertexShaderBlob(const std::string& name);

	void checkForChanges();

	void recompileAll();

	bool compileShader(ShaderInfo& info, bool isVertexShader);
	std::filesystem::file_time_type getFileModifiedTime(const std::wstring& filename);

private:
	ComPtr<ID3D11Device>& m_device;
	std::unordered_map<std::string, ShaderInfo> m_vertexShaders;
	std::unordered_map<std::string, ShaderInfo> m_pixelShaders;
	std::unordered_map<std::string, ShaderInfo> m_computeShaders;
};