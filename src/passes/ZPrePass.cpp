#include "ZPrePass.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include "appConfig.hpp"

#include "material.hpp"
#include "primitive.hpp"
#include "rtvCollector.hpp"
#include "scene.hpp"
#include "shaderManager.hpp"
#include "texture.hpp"

struct alignas(16) ConstantBufferData
{
	glm::mat4 modelViewProjection;
};

static constexpr D3D11_INPUT_ELEMENT_DESC s_zPrePassInputLayoutDesc[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
};


ZPrePass::ZPrePass(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context) : BasePass(device, context)
{

	m_rtvCollector = std::make_unique<RTVCollector>();
	m_rasterizerState = createRSState(RasterizerPreset::NoCullNoClip);
	m_depthStencilState = createDSState(DepthStencilPreset::WriteDepth);

	createOrResize();

	m_shaderManager->LoadVertexShader("zPrePass", ShaderManager::GetShaderPath(L"ZPrePass.hlsl"), "VS");
	m_shaderManager->LoadPixelShader("zPrePass", ShaderManager::GetShaderPath(L"ZPrePass.hlsl"), "PS");

	{
		HRESULT hr = m_device->CreateInputLayout(s_zPrePassInputLayoutDesc, ARRAYSIZE(s_zPrePassInputLayoutDesc),
			m_shaderManager->getVertexShaderBlob("zPrePass")->GetBufferPointer(),
			m_shaderManager->getVertexShaderBlob("zPrePass")->GetBufferSize(),
			&m_inputLayout);
		assert(SUCCEEDED(hr));
	};

	m_constantbuffer = createConstantBuffer(sizeof(ConstantBufferData));

	m_samplerState = createSamplerState(SamplerPreset::LinearClamp);
}

void ZPrePass::draw(const glm::mat4& view, const glm::mat4& projection, Scene* scene)
{

	beginDebugEvent(L"ZPrePass");
	m_context->RSSetState(m_rasterizerState.Get());
	setViewport(AppConfig::viewportWidth, AppConfig::viewportHeight);

	m_context->OMSetRenderTargets(0, nullptr, dsv.Get());
	m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);

	m_context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	m_context->IASetInputLayout(m_inputLayout.Get());
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_context->VSSetShader(m_shaderManager->getVertexShader("zPrePass"), nullptr, 0);
	m_context->PSSetShader(m_shaderManager->getPixelShader("zPrePass"), nullptr, 0);
	m_context->VSSetConstantBuffers(0, 1, m_constantbuffer.GetAddressOf());
	m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

	static const UINT stride = sizeof(Vertex);
	static const UINT offset = 0;

	for (auto& [handle, prim] : scene->getPrimitives())
	{
		if (!prim->getVisibility())
			continue;
		update(view, projection, prim);

		// Bind albedo texture for alpha testing
		if (prim->material && prim->material->albedo)
		{
			ID3D11ShaderResourceView* albedoSRV = prim->material->albedo->srv.Get();
			m_context->PSSetShaderResources(0, 1, &albedoSRV);
		}

		m_context->IASetVertexBuffers(0, 1, prim->getVertexBuffer().GetAddressOf(), &stride, &offset);
		m_context->IASetIndexBuffer(prim->getIndexBuffer().Get(), DXGI_FORMAT_R32_UINT, 0);
		m_context->DrawIndexed(static_cast<UINT>(prim->getIndexData().size()), 0, 0);
	}
	unbindRenderTargets(1);
	unbindShaderResources(0, 1);
	endDebugEvent();
}

ComPtr<ID3D11DepthStencilView> ZPrePass::getDSV()
{
	return dsv;
}

void ZPrePass::update(const glm::mat4& view, const glm::mat4& projection, Primitive* prim) const
{
	const glm::mat4 mvp = projection * view * prim->getWorldMatrix();
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = m_context->Map(m_constantbuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(hr))
	{
		auto* cbData = static_cast<ConstantBufferData*>(mappedResource.pData);
		cbData->modelViewProjection = glm::transpose(mvp);
		m_context->Unmap(m_constantbuffer.Get(), 0);
	}
}

void ZPrePass::createOrResize()
{
	if (t_depth != nullptr)
	{
		t_depth.Reset();
		dsv.Reset();
		srv_depth.Reset();
	}

	t_depth = createTexture2D(AppConfig::viewportWidth, AppConfig::viewportHeight, DXGI_FORMAT_R32_TYPELESS);
	dsv = createDepthStencilView(t_depth.Get(), DSVPreset::Texture2D);
	srv_depth = createShaderResourceView(t_depth.Get(), SRVPreset::Texture2D);

	m_rtvCollector->addRTV("ZPrePass::depth", srv_depth.Get());
}

ComPtr<ID3D11ShaderResourceView> ZPrePass::getDepthSRV() const
{
	return srv_depth;
}
