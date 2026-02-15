#include "GTAOPass.hpp"
#include <random>
#include "glm/gtc/matrix_transform.hpp"

#include "shaderManager.hpp"
#include "appConfig.hpp"
#include "rtvCollector.hpp"

static constexpr uint32_t COMPUTE_THREAD_GROUP_SIZE = 16;
static constexpr uint32_t COMPUTE_THREAD_GROUP_SIZE_MONE = COMPUTE_THREAD_GROUP_SIZE - 1;
static constexpr uint32_t MAX_SAMPLES = 64;
static constexpr uint32_t NOISE_TEX_DIM = 8; // 4x4 = 16 texels for tiling noise


struct alignas(16) GTAOConstantBuffer
{
	glm::mat4 projection;
	glm::mat4 view;
	glm::uvec2 dimensions;
	glm::vec2 nearFarPlanes;
	uint32_t sampleCount;
	float aoRadius;
	float aoBias;
	float aoIntensity;
	uint32_t enableGTAO;
};

GTAOPass::GTAOPass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context) : BasePass(device, context)
{
	m_shaderManager = std::make_unique<ShaderManager>(device);
	m_shaderManager->LoadComputeShader("gtao", ShaderManager::GetShaderPath(L"GTAO.hlsl"), "CS");
	m_shaderManager->LoadComputeShader("gtaoBlur", ShaderManager::GetShaderPath(L"GTAO.hlsl"), "BlurCS");
	createOrResize();
}

void GTAOPass::draw(ComPtr<ID3D11ShaderResourceView> viewNormalSRV,
	ComPtr<ID3D11ShaderResourceView> viewPositionSRV,
	glm::mat4& projection,
	glm::mat4& view,
	glm::vec2 nearFarPlanes)
{
	beginDebugEvent(L"GTAO Pass");
	update(projection, view, nearFarPlanes);

	m_context->CSSetShader(m_shaderManager->getComputeShader("gtao"), nullptr, 0);

	m_context->CSSetShaderResources(0, 1, viewNormalSRV.GetAddressOf());
	m_context->CSSetShaderResources(1, 1, viewPositionSRV.GetAddressOf());
	m_context->CSSetShaderResources(2, 1, m_randomSamplesSRV.GetAddressOf());
	m_context->CSSetShaderResources(3, 1, m_randomRotationsSRV.GetAddressOf());

	m_context->CSSetSamplers(0, 1, m_samplerState.GetAddressOf());

	m_context->CSSetUnorderedAccessViews(0, 1, m_aoUAV.GetAddressOf(), nullptr);

	m_context->CSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

	uint32_t dispatchX = static_cast<uint32_t>(
		std::ceil((AppConfig::viewportWidth + COMPUTE_THREAD_GROUP_SIZE_MONE) / COMPUTE_THREAD_GROUP_SIZE));
	uint32_t dispatchY = static_cast<uint32_t>(
		std::ceil((AppConfig::viewportHeight + COMPUTE_THREAD_GROUP_SIZE_MONE) / COMPUTE_THREAD_GROUP_SIZE));
	if (dispatchX == 0)
		dispatchX = 1;
	if (dispatchY == 0)
		dispatchY = 1;
	m_context->Dispatch(dispatchX, dispatchY, 1);
	unbindShaderResources(0, 4);
	unbindComputeUAVs(0, 1);
	endDebugEvent();

	blurAO();
}

void GTAOPass::createOrResize()
{
	if (m_aoTexture != nullptr)
	{
		m_aoTexture.Reset();
		m_aoUAV.Reset();
		m_aoSRV.Reset();
	}
	if (m_samplerState == nullptr)
	{
		m_samplerState = createSamplerState(SamplerPreset::PointWrap);
	}

	m_aoTexture = createTexture2D(AppConfig::viewportWidth, AppConfig::viewportHeight,
		DXGI_FORMAT_R32_FLOAT,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	m_aoUAV = createUnorderedAccessView(m_aoTexture.Get(), UAVPreset::Texture2D);
	m_aoSRV = createShaderResourceView(m_aoTexture.Get(), SRVPreset::Texture2D);

	m_bluredAOTexture = createTexture2D(AppConfig::viewportWidth, AppConfig::viewportHeight,
		DXGI_FORMAT_R32_FLOAT,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	m_bluredAOUAV = createUnorderedAccessView(m_bluredAOTexture.Get(), UAVPreset::Texture2D);
	m_bluredAOSRV = createShaderResourceView(m_bluredAOTexture.Get(), SRVPreset::Texture2D);

	m_rtvCollector->addRTV("GTAOPass::AO", m_aoSRV.Get());
	m_rtvCollector->addRTV("GTAOPass::BluredAO", m_bluredAOSRV.Get());
}

ComPtr<ID3D11ShaderResourceView> GTAOPass::getAOResultSRV() const
{
	return m_bluredAOSRV != nullptr ? m_bluredAOSRV : m_aoSRV;
}

void GTAOPass::update(glm::mat4& projection, glm::mat4& view, glm::vec2 nearFarPlanes)
{
	if (m_randomSamplesBuffer == nullptr)
	{
		std::vector<glm::vec3> randomSamples = generateRandomSampleInHemisphere();
		m_randomSamplesBuffer = createStructuredBuffer(sizeof(glm::vec3), static_cast<UINT>(randomSamples.size()),
			SBPreset::Immutable, randomSamples.data());
		m_randomSamplesSRV = createShaderResourceView(m_randomSamplesBuffer.Get(), SRVPreset::StructuredBuffer, 0,
			static_cast<UINT>(randomSamples.size()));

		std::vector<glm::vec4> randomRotations = generateRandomRotations();
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = randomRotations.data();
		initData.SysMemPitch = static_cast<UINT>(NOISE_TEX_DIM * sizeof(glm::vec4));
		m_randomRotationsTexture = createTexture2D(NOISE_TEX_DIM, NOISE_TEX_DIM,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			D3D11_BIND_SHADER_RESOURCE,
			1,
			1,
			D3D11_USAGE_DEFAULT,
			0,
			0,
			&initData
		);
		m_randomRotationsSRV = createShaderResourceView(m_randomRotationsTexture.Get(), SRVPreset::Texture2D);
		m_rtvCollector->addRTV("GTAOPass::RandomRotations", m_randomRotationsSRV.Get());
	}

	if (m_constantBuffer == nullptr)
	{
		m_constantBuffer = createConstantBuffer(sizeof(GTAOConstantBuffer));
	}

	D3D11_MAPPED_SUBRESOURCE mappedResource = {};
	if (SUCCEEDED(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
	{
		GTAOConstantBuffer* cbData = static_cast<GTAOConstantBuffer*>(mappedResource.pData);
		cbData->projection = glm::transpose(projection);
		cbData->view = glm::transpose(view);
		cbData->dimensions = glm::uvec2(AppConfig::viewportWidth, AppConfig::viewportHeight);
		cbData->sampleCount = MAX_SAMPLES;
		cbData->nearFarPlanes = nearFarPlanes;
		cbData->aoRadius = AppConfig::aoRadius;
		cbData->aoBias = AppConfig::aoBias;
		cbData->aoIntensity = AppConfig::aoIntensity;
		cbData->enableGTAO = AppConfig::gtaoEnabled ? 1 : 0;
		m_context->Unmap(m_constantBuffer.Get(), 0);
	}
}

void GTAOPass::blurAO()
{
	beginDebugEvent(L"GTAO Pass::Blur");
	m_context->CSSetShader(m_shaderManager->getComputeShader("gtaoBlur"), nullptr, 0);
	m_context->CSSetShaderResources(4, 1, m_aoSRV.GetAddressOf());

	m_context->CSSetUnorderedAccessViews(1, 1, m_bluredAOUAV.GetAddressOf(), nullptr);

	uint32_t dispatchX = static_cast<uint32_t>(
		std::ceil((AppConfig::viewportWidth + COMPUTE_THREAD_GROUP_SIZE_MONE) / COMPUTE_THREAD_GROUP_SIZE));
	uint32_t dispatchY = static_cast<uint32_t>(
		std::ceil((AppConfig::viewportHeight + COMPUTE_THREAD_GROUP_SIZE_MONE) / COMPUTE_THREAD_GROUP_SIZE));
	if (dispatchX == 0)
		dispatchX = 1;
	if (dispatchY == 0)
		dispatchY = 1;
	m_context->Dispatch(dispatchX, dispatchY, 1);
	unbindShaderResources(4, 1);
	unbindComputeUAVs(1, 1);
	endDebugEvent();

}

float lerp(float a, float b, float f)
{
	return a + f * (b - a);
}

std::vector<glm::vec3> GTAOPass::generateRandomSampleInHemisphere()
{
	std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
	std::default_random_engine generator;
	generator.seed(std::chrono::system_clock::now().time_since_epoch().count());
	std::vector<glm::vec3> ssaoKernel;
	for (uint32_t i = 0; i < MAX_SAMPLES; ++i)
	{
		glm::vec3 sample(
			randomFloats(generator) * 2.0 - 1.0, // X: random in [-1, 1]
			randomFloats(generator) * 2.0 - 1.0, // Y: random in [-1, 1]
			randomFloats(generator)                 // Z: random in [0, 1]
		);
		sample = glm::normalize(sample);
		sample *= randomFloats(generator); // random length within hemisphere

		float scale = (float)i / float(MAX_SAMPLES);
		scale = lerp(0.1f, 1.0f, scale * scale);
		sample *= scale;
		ssaoKernel.push_back(sample);
	}
	return ssaoKernel;
}

std::vector<glm::vec4> GTAOPass::generateRandomRotations()
{
	std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
	std::default_random_engine generator;
	generator.seed(std::chrono::system_clock::now().time_since_epoch().count());
	std::vector<glm::vec4> randomRotations;
	for (uint32_t i = 0; i < NOISE_TEX_DIM * NOISE_TEX_DIM; ++i)
	{
		glm::vec4 rotation(
			randomFloats(generator) * 2.0f - 1.0f,  // X: random in [-1, 1]
			randomFloats(generator) * 2.0f - 1.0f, // Y: random in [-1, 1]
			0.0f, // Z: 0 for rotations around the normal
			0.0f  // W: padding for R32G32B32A32 format
		);
		randomRotations.push_back(rotation);
	}
	return randomRotations;
}
