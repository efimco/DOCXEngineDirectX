#pragma once

#include "glm/glm.hpp"
#include "wrl.h"

#include "BasePass.hpp"

using namespace Microsoft::WRL;

class RTVCollector;

class GTAOPass : public BasePass
{
public:
	GTAOPass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);

	void draw(ComPtr<ID3D11ShaderResourceView> viewNormalSRV,
		ComPtr<ID3D11ShaderResourceView> viewPositionSRV,
		glm::mat4& projection,
		glm::mat4& view,
		glm::vec2 nearFarPlanes);

	void createOrResize();

	ComPtr<ID3D11ShaderResourceView> getAOResultSRV() const;


private:

	void update(glm::mat4& projection, glm::mat4& view, glm::vec2 nearFarPlanes);
	std::vector<glm::vec3> generateRandomSampleInHemisphere();
	std::vector<glm::vec4> generateRandomRotations();

	ComPtr<ID3D11Buffer> m_randomSamplesBuffer;
	ComPtr<ID3D11ShaderResourceView> m_randomSamplesSRV;

	ComPtr<ID3D11Buffer> m_constantBuffer;

	ComPtr<ID3D11Texture2D> m_randomRotationsTexture;
	ComPtr<ID3D11ShaderResourceView> m_randomRotationsSRV;

	ComPtr<ID3D11SamplerState> m_samplerState;

	ComPtr<ID3D11Texture2D> m_aoTexture;
	ComPtr<ID3D11UnorderedAccessView> m_aoUAV;
	ComPtr<ID3D11ShaderResourceView> m_aoSRV;
	std::unique_ptr<RTVCollector> m_rtvCollector;
};