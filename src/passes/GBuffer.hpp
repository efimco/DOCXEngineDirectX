#pragma once

#include <d3d11_4.h>
#include <glm/glm.hpp>
#include <memory>
#include <wrl.h>

#include "GBufferTextures.hpp"
#include "basePass.hpp"

class Scene;
class Primitive;
class ShaderManager;
class Camera;
class RTVCollector;

using namespace Microsoft::WRL;

class GBuffer : public BasePass
{
public:
	GBuffer(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
	~GBuffer() override = default;

	void draw(const glm::mat4& view,
			  const glm::mat4& projection,
			  const glm::vec3& cameraPosition,
			  Scene* scene,
			  ComPtr<ID3D11DepthStencilView> dsv);
	void update(const glm::mat4& view,
				const glm::mat4& projection,
				const glm::vec3& cameraPosition,
				Scene* scene,
				float objectID,
				Primitive* prim) const;
	void createOrResize();
	GBufferTextures getGBufferTextures() const;
	ComPtr<ID3D11ShaderResourceView> getAlbedoSRV() const;
	ComPtr<ID3D11ShaderResourceView> getMetallicRoughnessSRV() const;
	ComPtr<ID3D11ShaderResourceView> getNormalSRV() const;
	ComPtr<ID3D11ShaderResourceView> getViewNormalSRV() const;
	ComPtr<ID3D11ShaderResourceView> getPositionSRV() const;
	ComPtr<ID3D11ShaderResourceView> getViewPositionSRV() const;
	ComPtr<ID3D11ShaderResourceView> getObjectIDSRV() const;
	ComPtr<ID3D11RenderTargetView> getObjectIDRTV() const;

private:
	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;

	ComPtr<ID3D11Texture2D> t_albedo;
	ComPtr<ID3D11Texture2D> t_metallicRoughness;
	ComPtr<ID3D11Texture2D> t_normal;
	ComPtr<ID3D11Texture2D> t_viewNormal;
	ComPtr<ID3D11Texture2D> t_position;
	ComPtr<ID3D11Texture2D> t_viewPosition;
	ComPtr<ID3D11Texture2D> t_objectID;

	ComPtr<ID3D11RenderTargetView> rtv_albedo;
	ComPtr<ID3D11RenderTargetView> rtv_metallicRoughness;
	ComPtr<ID3D11RenderTargetView> rtv_normal;
	ComPtr<ID3D11RenderTargetView> rtv_viewNormal;
	ComPtr<ID3D11RenderTargetView> rtv_position;
	ComPtr<ID3D11RenderTargetView> rtv_viewPosition;
	ComPtr<ID3D11RenderTargetView> rtv_objectID;

	ComPtr<ID3D11ShaderResourceView> srv_albedo;
	ComPtr<ID3D11ShaderResourceView> srv_metallicRoughness;
	ComPtr<ID3D11ShaderResourceView> srv_normal;
	ComPtr<ID3D11ShaderResourceView> srv_viewNormal;
	ComPtr<ID3D11ShaderResourceView> srv_position;
	ComPtr<ID3D11ShaderResourceView> srv_viewPosition;
	ComPtr<ID3D11ShaderResourceView> srv_objectID;

	ComPtr<ID3D11Buffer> m_constantbuffer;
	ComPtr<ID3D11InputLayout> m_inputLayout;

	ID3D11RenderTargetView* m_rtvs[7];
	std::unique_ptr<RTVCollector> m_rtvCollector;
};
