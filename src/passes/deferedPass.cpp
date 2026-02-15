#include "deferedPass.hpp"


#include "GBufferTextures.hpp"
#include "appConfig.hpp"
#include "basePass.hpp"
#include "light.hpp"
#include "rtvCollector.hpp"
#include "scene.hpp"
#include "shaderManager.hpp"

static constexpr uint32_t COMPUTE_THREAD_GROUP_SIZE = 16;
static constexpr uint32_t MAX_LIGHTS = 100;

struct alignas(16) DeferredConstantBuffer
{
	float IBLrotationY;
	float IBLintensity;
	float selectedID;
	int numLights;
	float cameraPosition[3];
	int drawWSUI;
};

DeferredPass::DeferredPass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context) : BasePass(device, context)
{
	m_rtvCollector = std::make_unique<RTVCollector>();
	m_shaderManager->LoadComputeShader("deferred", ShaderManager::GetShaderPath(L"deferred.hlsl"), "CS");
	m_lightsBuffer = createStructuredBuffer(sizeof(LightData), MAX_LIGHTS, SBPreset::CpuWrite);
	m_lightsSRV = createShaderResourceView(m_lightsBuffer.Get(), SRVPreset::StructuredBuffer, 0, MAX_LIGHTS);
	m_samplerState = createSamplerState(SamplerPreset::LinearClamp);
	m_constantBuffer = createConstantBuffer(sizeof(DeferredConstantBuffer));
	createOrResize();
}

void DeferredPass::createOrResize()
{
	if (m_finalTexture != nullptr)
	{
		m_finalTexture.Reset();
		m_finalRTV.Reset();
		m_finalSRV.Reset();
	}

	m_finalTexture = createTexture2D(AppConfig::viewportWidth, AppConfig::viewportHeight,
									 DXGI_FORMAT_R8G8B8A8_UNORM,
									 D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	m_finalRTV = createRenderTargetView(m_finalTexture.Get(), RTVPreset::Texture2D);
	m_finalSRV = createShaderResourceView(m_finalTexture.Get(), SRVPreset::Texture2D);
	m_finalUAV = createUnorderedAccessView(m_finalTexture.Get(), UAVPreset::Texture2D);

	m_rtvCollector->addRTV("DeferredPass::finalOutput", m_finalSRV.Get());
}

void DeferredPass::draw(const glm::mat4& view,
						const glm::mat4& projection,
						const glm::vec3& cameraPosition,
						Scene* scene,
						const GBufferTextures& gbufferTextures,
						ComPtr<ID3D11ShaderResourceView> depthSRV,
						ComPtr<ID3D11ShaderResourceView> backgroundSRV,
						ComPtr<ID3D11ShaderResourceView> irradianceSRV,
						ComPtr<ID3D11ShaderResourceView> prefilteredSRV,
						ComPtr<ID3D11ShaderResourceView> brdfLutSRV,
						ComPtr<ID3D11ShaderResourceView> worldSpaceUISRV,
						ComPtr<ID3D11ShaderResourceView> GTAOSRV)
{
	beginDebugEvent(L"Deffered Pass");
	if (scene->areLightsDirty())
	{
		updateLights(scene);
		scene->setLightsDirty(false);
	}

	D3D11_MAPPED_SUBRESOURCE mappedResource = {};
	HRESULT hr = m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(hr))
	{
		auto* cbData = static_cast<DeferredConstantBuffer*>(mappedResource.pData);
		cbData->IBLrotationY = AppConfig::IBLrotation;
		cbData->IBLintensity = AppConfig::IBLintensity;
		cbData->selectedID = static_cast<float>(scene->getActiveNodeID());
		cbData->cameraPosition[0] = cameraPosition.x;
		cbData->cameraPosition[1] = cameraPosition.y;
		cbData->cameraPosition[2] = cameraPosition.z;
		cbData->drawWSUI = AppConfig::drawWSUI ? 1 : 0;
		cbData->numLights = static_cast<int>(scene->getLights().size());
		m_context->Unmap(m_constantBuffer.Get(), 0);
	}

	m_context->CSSetShader(m_shaderManager->getComputeShader("deferred"), nullptr, 0);

	// Set SRVs
	ID3D11ShaderResourceView* srvs[] = {gbufferTextures.albedoSRV.Get(),
										gbufferTextures.metallicRoughnessSRV.Get(),
										gbufferTextures.normalSRV.Get(),
										gbufferTextures.positionSRV.Get(),
										gbufferTextures.objectIDSRV.Get(),
										depthSRV.Get(),
										m_lightsSRV.Get(),
										backgroundSRV.Get(),
										irradianceSRV.Get(),
										prefilteredSRV.Get(),
										brdfLutSRV.Get(),
										worldSpaceUISRV.Get(),
										GTAOSRV.Get()};
	m_context->CSSetShaderResources(0, 13, srvs);
	m_context->CSSetSamplers(0, 1, m_samplerState.GetAddressOf());
	m_context->CSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

	// Set UAVs
	ID3D11UnorderedAccessView* uavs[] = {m_finalUAV.Get()};
	m_context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

	// Dispatch compute shader
	uint32_t dispatchX = static_cast<uint32_t>(
		std::ceil((AppConfig::viewportWidth + COMPUTE_THREAD_GROUP_SIZE - 1) / COMPUTE_THREAD_GROUP_SIZE));
	uint32_t dispatchY = static_cast<uint32_t>(
		std::ceil((AppConfig::viewportHeight + COMPUTE_THREAD_GROUP_SIZE - 1) / COMPUTE_THREAD_GROUP_SIZE));
	if (dispatchX == 0)
		dispatchX = 1;
	if (dispatchY == 0)
		dispatchY = 1;
	m_context->Dispatch(dispatchX, dispatchY, 1);

	// Unbind SRVs and UAVs
	ID3D11ShaderResourceView* nullSRVs[12] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
											  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
	m_context->CSSetShaderResources(0, 12, nullSRVs);
	ID3D11UnorderedAccessView* nullUAVs[1] = {nullptr};
	m_context->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);

	endDebugEvent();
}


ComPtr<ID3D11ShaderResourceView> DeferredPass::getFinalSRV() const
{
	return m_finalSRV;
}

ComPtr<ID3D11RenderTargetView> DeferredPass::getFinalRTV() const
{
	return m_finalRTV;
}

void DeferredPass::updateLights(Scene* scene) const
{
	D3D11_MAPPED_SUBRESOURCE mappedResource = {};
	HRESULT hr = m_context->Map(m_lightsBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(hr))
	{
		auto* lightData = static_cast<LightData*>(mappedResource.pData);
		const auto& lights = scene->getLights();
		int32_t i = 0;
		for (auto& [handle, light] : lights)
		{
			lightData[i] = light->getLightData();
			lightData[i].objectID = static_cast<float>(static_cast<int32_t>(handle));
			++i;
		}
		m_context->Unmap(m_lightsBuffer.Get(), 0);
	}
}
