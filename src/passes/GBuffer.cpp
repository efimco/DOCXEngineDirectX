#include "GBuffer.hpp"

#include <dxgiformat.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <winbase.h>

#include "appConfig.hpp"

#include "GBufferTextures.hpp"
#include "basePass.hpp"
#include "material.hpp"
#include "primitive.hpp"
#include "rtvCollector.hpp"
#include "scene.hpp"
#include "shaderManager.hpp"

static constexpr D3D11_INPUT_ELEMENT_DESC s_gBufferInputLayoutDesc[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

struct alignas(16) ConstantBufferData
{
	glm::mat4 modelViewProjection;
	glm::mat4 inverseTransposedModel;
	glm::mat4 model;
	glm::mat4 view;
	glm::vec3 cameraPosition;
	float objectID;
	glm::vec4 albedoColor;
	float metallicValue;
	float roughnessValue;
	uint32_t useAlbedoTexture;
	uint32_t useMetallicRoughnessTexture;
	uint32_t useNormalTexture;
	uint32_t flipY;
	float padding[2];
};

GBuffer::GBuffer(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context) : BasePass(device, context)
{
	m_device = device;
	m_context = context;
	m_rtvCollector = std::make_unique<RTVCollector>();
	m_rasterizerState = createRSState(RasterizerPreset::NoCullNoClip);
	m_depthStencilState = createDSState(DepthStencilPreset::ReadOnlyLessEqual);
	m_samplerState = createSamplerState(SamplerPreset::AnisotropicWrap);

	// Load shaders via inherited m_shaderManager
	m_shaderManager->LoadPixelShader("gBuffer", ShaderManager::GetShaderPath(L"gBuffer.hlsl"), "PS");
	m_shaderManager->LoadVertexShader("gBuffer", ShaderManager::GetShaderPath(L"gBuffer.hlsl"), "VS");

	createOrResize();

	{
		HRESULT hr = m_device->CreateInputLayout(s_gBufferInputLayoutDesc, ARRAYSIZE(s_gBufferInputLayoutDesc),
			m_shaderManager->getVertexShaderBlob("gBuffer")->GetBufferPointer(),
			m_shaderManager->getVertexShaderBlob("gBuffer")->GetBufferSize(),
			&m_inputLayout);
		assert(SUCCEEDED(hr));
	};
	m_constantbuffer = createConstantBuffer(sizeof(ConstantBufferData));
}

void GBuffer::draw(const glm::mat4& view,
	const glm::mat4& projection,
	const glm::vec3& cameraPosition,
	Scene* scene,
	ComPtr<ID3D11DepthStencilView> dsv)
{
	setViewport(AppConfig::viewportWidth, AppConfig::viewportHeight);

	beginDebugEvent(L"GBuffer Pass");

	m_context->RSSetState(m_rasterizerState.Get());

	m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
	m_context->OMSetRenderTargets(7, m_rtvs, dsv.Get());

	for (const auto rtv : m_rtvs)
	{
		m_context->ClearRenderTargetView(rtv, AppConfig::clearColor);
	}
	m_context->IASetInputLayout(m_inputLayout.Get());
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_context->VSSetShader(m_shaderManager->getVertexShader("gBuffer"), nullptr, 0);
	m_context->VSSetConstantBuffers(0, 1, m_constantbuffer.GetAddressOf());

	m_context->PSSetShader(m_shaderManager->getPixelShader("gBuffer"), nullptr, 0);
	m_context->PSSetConstantBuffers(0, 1, m_constantbuffer.GetAddressOf());
	m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

	static const UINT stride = sizeof(Vertex);
	static const UINT offset = 0;
	for (auto& [handle, prim] : scene->getPrimitives())
	{
		if (!prim->getVisibility())
			continue;
		const float objectID = static_cast<float>(static_cast<int32_t>(handle));
		update(view, projection, cameraPosition, scene, objectID, prim);
		m_context->IASetVertexBuffers(0, 1, prim->getVertexBuffer().GetAddressOf(), &stride, &offset);
		m_context->IASetIndexBuffer(prim->getIndexBuffer().Get(), DXGI_FORMAT_R32_UINT, 0);
		ID3D11ShaderResourceView* const* SRVs = prim->material->getSRVs();
		m_context->PSSetShaderResources(0, 3, SRVs);
		m_context->DrawIndexed(static_cast<UINT>(prim->getIndexData().size()), 0, 0);
		unbindShaderResources(0, 3);
	}
	unbindRenderTargets(7);
	endDebugEvent();
}

void GBuffer::update(const glm::mat4& view,
	const glm::mat4& projection,
	const glm::vec3& cameraPosition,
	Scene* scene,
	const float objectID,
	Primitive* prim) const
{
	const glm::mat4 mvp = projection * view * prim->getWorldMatrix();
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = m_context->Map(m_constantbuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(hr))
	{
		const auto cbData = static_cast<ConstantBufferData*>(mappedResource.pData);
		cbData->modelViewProjection = glm::transpose(mvp);
		cbData->inverseTransposedModel = glm::transpose(glm::inverse(prim->getWorldMatrix()));
		cbData->model = glm::transpose(prim->getWorldMatrix());
		cbData->view = glm::transpose(view);
		cbData->cameraPosition = cameraPosition;
		cbData->objectID = objectID;
		if (prim->material)
		{
			cbData->albedoColor = prim->material->albedoColor;
			cbData->metallicValue = prim->material->metallicValue;
			cbData->roughnessValue = prim->material->roughnessValue;
			cbData->useAlbedoTexture = prim->material->useAlbedo ? 1 : 0;
			cbData->useMetallicRoughnessTexture = prim->material->useMetallicRoughness ? 1 : 0;
			cbData->useNormalTexture = prim->material->useNormal ? 1 : 0;
			cbData->flipY = prim->material->flipY ? 1 : 0;
		}
		m_context->Unmap(m_constantbuffer.Get(), 0);
	}
}

void GBuffer::createOrResize()
{

	if (t_albedo != nullptr)
	{
		t_albedo.Reset();
		t_metallicRoughness.Reset();
		t_normal.Reset();
		t_viewNormal.Reset();
		t_position.Reset();

		rtv_albedo.Reset();
		rtv_metallicRoughness.Reset();
		rtv_normal.Reset();
		rtv_viewNormal.Reset();
		rtv_position.Reset();

		srv_albedo.Reset();
		srv_metallicRoughness.Reset();
		srv_normal.Reset();
		srv_viewNormal.Reset();
		srv_position.Reset();
	}

	// albedo
	t_albedo = createTexture2D(AppConfig::viewportWidth, AppConfig::viewportHeight, DXGI_FORMAT_R16G16B16A16_FLOAT);
	srv_albedo = createShaderResourceView(t_albedo.Get(), SRVPreset::Texture2D);
	rtv_albedo = createRenderTargetView(t_albedo.Get(), RTVPreset::Texture2D);

	// metallicRoughness
	t_metallicRoughness = createTexture2D(AppConfig::viewportWidth, AppConfig::viewportHeight, DXGI_FORMAT_R16G16_UNORM);
	srv_metallicRoughness = createShaderResourceView(t_metallicRoughness.Get(), SRVPreset::Texture2D);
	rtv_metallicRoughness = createRenderTargetView(t_metallicRoughness.Get(), RTVPreset::Texture2D);

	// normal
	t_normal = createTexture2D(AppConfig::viewportWidth, AppConfig::viewportHeight, DXGI_FORMAT_R32G32B32A32_FLOAT);
	srv_normal = createShaderResourceView(t_normal.Get(), SRVPreset::Texture2D);
	rtv_normal = createRenderTargetView(t_normal.Get(), RTVPreset::Texture2D);

	// view Normal

	t_viewNormal = createTexture2D(AppConfig::viewportWidth, AppConfig::viewportHeight, DXGI_FORMAT_R32G32B32A32_FLOAT);
	srv_viewNormal = createShaderResourceView(t_viewNormal.Get(), SRVPreset::Texture2D);
	rtv_viewNormal = createRenderTargetView(t_viewNormal.Get(), RTVPreset::Texture2D);

	// position
	t_position = createTexture2D(AppConfig::viewportWidth, AppConfig::viewportHeight, DXGI_FORMAT_R32G32B32A32_FLOAT);
	srv_position = createShaderResourceView(t_position.Get(), SRVPreset::Texture2D);
	rtv_position = createRenderTargetView(t_position.Get(), RTVPreset::Texture2D);

	// position
	t_viewPosition = createTexture2D(AppConfig::viewportWidth, AppConfig::viewportHeight, DXGI_FORMAT_R32G32B32A32_FLOAT);
	srv_viewPosition = createShaderResourceView(t_viewPosition.Get(), SRVPreset::Texture2D);
	rtv_viewPosition = createRenderTargetView(t_viewPosition.Get(), RTVPreset::Texture2D);

	// objectID
	t_objectID = createTexture2D(AppConfig::viewportWidth, AppConfig::viewportHeight, DXGI_FORMAT_R32_FLOAT);
	srv_objectID = createShaderResourceView(t_objectID.Get(), SRVPreset::Texture2D);
	rtv_objectID = createRenderTargetView(t_objectID.Get(), RTVPreset::Texture2D);

	// rtv assignment
	m_rtvs[0] = rtv_albedo.Get();
	m_rtvs[1] = rtv_metallicRoughness.Get();
	m_rtvs[2] = rtv_normal.Get();
	m_rtvs[3] = rtv_viewNormal.Get();
	m_rtvs[4] = rtv_position.Get();
	m_rtvs[5] = rtv_viewPosition.Get();
	m_rtvs[6] = rtv_objectID.Get();

	m_rtvCollector->addRTV("GBuffer::Albedo", srv_albedo.Get());
	m_rtvCollector->addRTV("GBuffer::MetallicRoughness", srv_metallicRoughness.Get());
	m_rtvCollector->addRTV("GBuffer::Normal", srv_normal.Get());
	m_rtvCollector->addRTV("GBuffer::ViewNormal", srv_viewNormal.Get());
	m_rtvCollector->addRTV("GBuffer::Position", srv_position.Get());
	m_rtvCollector->addRTV("GBuffer::ViewPosition", srv_viewPosition.Get());
	m_rtvCollector->addRTV("GBuffer::ObjectID", srv_objectID.Get());
}

GBufferTextures GBuffer::getGBufferTextures() const
{
	GBufferTextures gbufferTextures;
	gbufferTextures.albedoSRV = srv_albedo;
	gbufferTextures.metallicRoughnessSRV = srv_metallicRoughness;
	gbufferTextures.normalSRV = srv_normal;
	gbufferTextures.viewNormalSRV = srv_viewNormal;
	gbufferTextures.positionSRV = srv_position;
	gbufferTextures.objectIDSRV = srv_objectID;
	return gbufferTextures;
}

ComPtr<ID3D11ShaderResourceView> GBuffer::getAlbedoSRV() const
{
	return srv_albedo;
}

ComPtr<ID3D11ShaderResourceView> GBuffer::getMetallicRoughnessSRV() const
{
	return srv_metallicRoughness;
}

ComPtr<ID3D11ShaderResourceView> GBuffer::getNormalSRV() const
{
	return srv_normal;
}

ComPtr<ID3D11ShaderResourceView> GBuffer::getViewNormalSRV() const
{
	return srv_viewNormal;
}

ComPtr<ID3D11ShaderResourceView> GBuffer::getPositionSRV() const
{
	return srv_position;
}

ComPtr<ID3D11ShaderResourceView> GBuffer::getViewPositionSRV() const
{
	return srv_viewPosition;
}

ComPtr<ID3D11ShaderResourceView> GBuffer::getObjectIDSRV() const
{
	return srv_objectID;
}

ComPtr<ID3D11RenderTargetView> GBuffer::getObjectIDRTV() const
{
	return rtv_objectID;
}
