#include "renderer.hpp"
#include <memory>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

#include "appConfig.hpp"
#include "dxDevice.hpp"
#include "inputEventsHandler.hpp"
#include "shaderManager.hpp"
#include "uiManager.hpp"

#include "passes/FSQuad.hpp"
#include "passes/GBuffer.hpp"
#include "passes/ZPrePass.hpp"
#include "passes/bvhDebugPass.hpp"
#include "passes/deferedPass.hpp"
#include "passes/objectPicker.hpp"
#include "passes/pbrCubeMapPass.hpp"
#include "passes/previewGenerator.hpp"
#include "passes/rayTracePass.hpp"
#include "passes/wordSpaceUIPass.hpp"
#include "passes/bakerPass.hpp"
#include "passes/GTAOPass.hpp"


#include "utility/rdocHelpers.hpp"

#include "camera.hpp"
#include "light.hpp"
#include "scene.hpp"

#define DRAW_DEBUG_BVH 0

#if defined(_DEBUG) && ENABLE_RENDERDOC_CAPTURE
RENDERDOC_API_1_1_2* g_rdocAPI;
#endif

Renderer::Renderer(const HWND& hwnd)
{
#if defined(_DEBUG) && ENABLE_RENDERDOC_CAPTURE

	const HMODULE rdocModule = LoadLibraryA("..\\..\\thirdparty\\renderdoc.dll");
	if (rdocModule)
	{
		const auto RENDERDOC_GetAPI =
			reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(rdocModule, "RENDERDOC_GetAPI"));
		if (RENDERDOC_GetAPI)
		{
			RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, reinterpret_cast<void**>(&m_rdocAPI));
			g_rdocAPI = m_rdocAPI;
		}
	}
#endif
	m_dxDevice = std::make_unique<DXDevice>(hwnd);
	m_shaderManager = std::make_unique<ShaderManager>(m_dxDevice->getDevice());

	glm::vec3 cameraPosition(0.0f, 0.0f, -5.0f);
	m_camera = std::make_unique<Camera>(cameraPosition);

	m_view = m_camera->getViewMatrix();

	m_prevTime = std::chrono::system_clock::now();
	m_scene = std::make_unique<Scene>("Main Scene", m_dxDevice->getDevice());
	auto pointLight = std::make_unique<Light>(POINT_LIGHT, glm::vec3(0.0f, 1.0f, 1.0f));
	m_scene->addChild(std::move(pointLight));
	m_scene->setActiveCamera(m_camera.get());
	m_scene->addChild(std::move(m_camera));

	const std::wstring hdriPathW = AppConfig::GetResourcePath(L"citrus_orchard_road_puresky_4k.hdr");
	std::string hdriPath(hdriPathW.size(), '\0');
	std::transform(hdriPathW.begin(), hdriPathW.end(), hdriPath.begin(),
		[](wchar_t c) { return static_cast<char>(c); });

	std::cout << "Number of primitives loaded: " << m_scene->getPrimitiveCount() << std::endl;
	m_zPrePass = std::make_unique<ZPrePass>(m_dxDevice->getDevice(), m_dxDevice->getContext());
	m_gBuffer = std::make_unique<GBuffer>(m_dxDevice->getDevice(), m_dxDevice->getContext());
	m_objectPicker = std::make_unique<ObjectPicker>(m_dxDevice->getDevice(), m_dxDevice->getContext());
	m_fsquad = std::make_unique<FSQuad>(m_dxDevice->getDevice(), m_dxDevice->getContext());
	m_deferredPass = std::make_unique<DeferredPass>(m_dxDevice->getDevice(), m_dxDevice->getContext());
	m_cubeMapPass = std::make_unique<CubeMapPass>(m_dxDevice->getDevice(), m_dxDevice->getContext(), hdriPath);
	m_previewGenerator = std::make_unique<PreviewGenerator>(m_dxDevice->getDevice(), m_dxDevice->getContext());
	m_worldSpaceUIPass = std::make_unique<WorldSpaceUIPass>(m_dxDevice->getDevice(), m_dxDevice->getContext());
	m_rayTracePass = std::make_unique<RayTracePass>(m_dxDevice->getDevice(), m_dxDevice->getContext());
	m_bvhDebugPass = std::make_unique<BVHDebugPass>(m_dxDevice->getDevice(), m_dxDevice->getContext());
	m_bakerPass = std::make_unique<BakerPass>(m_dxDevice->getDevice(), m_dxDevice->getContext(), m_scene.get());
	m_gtaoPass = std::make_unique<GTAOPass>(m_dxDevice->getDevice(), m_dxDevice->getContext());

	m_uiManager = std::make_unique<UIManager>(m_dxDevice->getDevice(), m_dxDevice->getContext(), hwnd);
	resize();
}

Renderer::~Renderer() = default;

void Renderer::draw()
{
	if (InputEvents::isKeyDown(KeyButtons::KEY_F11))
	{
		AppConfig::captureNextFrame = true;
	}
#if defined(_DEBUG) && ENABLE_RENDERDOC_CAPTURE
	if (m_rdocAPI && AppConfig::captureNextFrame)
	{
		m_rdocAPI->StartFrameCapture(m_dxDevice->getDevice().Get(), nullptr);
	}
#endif
	if (AppConfig::needsResize)
	{
		resize();
		m_zPrePass->createOrResize();
		m_gBuffer->createOrResize();
		m_worldSpaceUIPass->createOrResize();
		m_deferredPass->createOrResize();
		m_fsquad->createOrResize();
		m_cubeMapPass->createOrResize();
		m_worldSpaceUIPass->createOrResize();
		m_gtaoPass->createOrResize();
#if DRAW_DEBUG_BVH
		m_bvhDebugPass->createOrResize();
#endif
		AppConfig::needsResize = false;
	}

	// --- CPU Updates ---
	m_scene->updateState();

	if (m_scene->isEnvironmentMapDirty()) 	// Check for environment map changes
	{
		m_cubeMapPass->setHDRIPath(m_scene->getEnvironmentMapPath());
		m_scene->clearEnvironmentMapDirty();
	}

	const std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
	m_deltaTime = currentTime - m_prevTime;
	m_prevTime = currentTime;
	AppConfig::deltaTime = m_deltaTime.count();

	m_scene->getActiveCamera()->processMovementControls(m_scene->getActiveNode());
	m_view = m_scene->getActiveCamera()->getViewMatrix();
	const auto vWidth = static_cast<float>(AppConfig::viewportWidth);
	const auto vHeight = static_cast<float>(AppConfig::viewportHeight);
	const float aspectRatio = vWidth / vHeight;
	m_projection = glm::perspectiveLH(glm::radians(m_scene->getActiveCamera()->fov),
		aspectRatio,
		m_scene->getActiveCamera()->nearPlane,
		m_scene->getActiveCamera()->farPlane);

	static int frameCount = 0;
	if (++frameCount % 60 == 0) // Check every 60 frames
	{
		m_shaderManager->checkForChanges();
	}

	// --- GPU Work ---
	m_zPrePass->draw(m_view, m_projection, m_scene.get());
	m_gBuffer->draw(m_view, m_projection,
		m_scene->getActiveCamera()->transform.position,
		m_scene.get(),
		m_zPrePass->getDSV());
	m_gtaoPass->draw(m_gBuffer->getViewNormalSRV(),
		m_gBuffer->getViewPositionSRV(),
		m_projection,
		m_view,
		glm::vec2(m_scene->getActiveCamera()->nearPlane, m_scene->getActiveCamera()->farPlane));
	m_previewGenerator->generatePreview(m_scene.get());
	m_cubeMapPass->draw(m_view, m_projection);
	m_worldSpaceUIPass->draw(m_view, m_projection, m_scene.get(), m_gBuffer->getObjectIDRTV());
	m_deferredPass->draw(m_view, m_projection, m_scene->getActiveCamera()->transform.position,
		m_scene.get(),
		m_gBuffer->getGBufferTextures(),
		m_zPrePass->getDepthSRV(),
		m_cubeMapPass->getBackgroundSRV(),
		m_cubeMapPass->getIrradianceSRV(),
		m_cubeMapPass->getPrefilteredSRV(),
		m_cubeMapPass->getBRDFLutSRV(),
		m_worldSpaceUIPass->getSRV(),
		m_gtaoPass->getAOResultSRV());

#if DRAW_DEBUG_BVH
	m_bvhDebugPass->draw(m_scene.get(), m_view, m_projection);
#endif
	m_fsquad->draw(m_deferredPass->getFinalSRV());

	m_dxDevice->getContext()->OMSetRenderTargets(1, m_backBufferRTV.GetAddressOf(), m_depthStencilView.Get());
	m_dxDevice->getContext()->ClearRenderTargetView(m_backBufferRTV.Get(), AppConfig::clearColor);
	m_dxDevice->getContext()->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	m_dxDevice->getContext()->RSSetState(m_rasterizerState.Get());
	m_dxDevice->getContext()->OMSetDepthStencilState(m_depthStencilState.Get(), 0);

	m_uiManager->draw(m_fsquad->getSRV(), m_scene.get(), m_view, m_projection);
	m_objectPicker->dispatchPick(m_gBuffer->getObjectIDSRV(), m_uiManager->getMousePos(), m_scene.get());
	m_dxDevice->getSwapChain()->Present(0, 0);
#if defined(_DEBUG) && ENABLE_RENDERDOC_CAPTURE
	if (m_rdocAPI && AppConfig::captureNextFrame)
	{
		m_rdocAPI->EndFrameCapture(nullptr, nullptr);

		if (m_rdocAPI->GetNumCaptures() > 0)
		{
			const uint32_t idx = m_rdocAPI->GetNumCaptures() - 1;
			char path[512];
			uint32_t pathLen = sizeof(path);
			uint64_t timestamp;
			if (m_rdocAPI->GetCapture(idx, path, &pathLen, &timestamp))
			{
				m_rdocAPI->LaunchReplayUI(1, path); // 1 = connect to this instance
				AppConfig::captureNextFrame = false;
			}
		}
	}
#endif
}


void Renderer::resize()
{
	m_backBuffer.Reset();
	m_backBufferRTV.Reset();
	m_depthStencilBuffer.Reset();
	m_depthStencilView.Reset();

	m_dxDevice->getSwapChain()->ResizeBuffers(0, AppConfig::windowWidth, AppConfig::windowHeight,
		DXGI_FORMAT_UNKNOWN, 0);

	{
		const HRESULT hr = m_dxDevice->getSwapChain()->GetBuffer(0, __uuidof(ID3D11Texture2D), &m_backBuffer);
		assert(SUCCEEDED(hr));
	}

	{
		const HRESULT hr = m_dxDevice->getDevice()->CreateRenderTargetView(m_backBuffer.Get(), nullptr, &m_backBufferRTV);
		assert(SUCCEEDED(hr));
	}

	{
		D3D11_TEXTURE2D_DESC depthBufferDesc = {};
		depthBufferDesc.Width = AppConfig::windowWidth;
		depthBufferDesc.Height = AppConfig::windowHeight;
		depthBufferDesc.MipLevels = 1;
		depthBufferDesc.ArraySize = 1;
		depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthBufferDesc.SampleDesc.Count = 1;
		depthBufferDesc.SampleDesc.Quality = 0;
		depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthBufferDesc.CPUAccessFlags = 0;
		depthBufferDesc.MiscFlags = 0;

		const HRESULT hr = m_dxDevice->getDevice()->CreateTexture2D(&depthBufferDesc, nullptr, &m_depthStencilBuffer);
		assert(SUCCEEDED(hr));
	}
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
		depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Texture2D.MipSlice = 0;

		const HRESULT hr = m_dxDevice->getDevice()->CreateDepthStencilView(m_depthStencilBuffer.Get(),
			&depthStencilViewDesc, &m_depthStencilView);
		assert(SUCCEEDED(hr));
	}

	D3D11_VIEWPORT viewport = {};
	viewport.Width = static_cast<float>(AppConfig::windowWidth);
	viewport.Height = static_cast<float>(AppConfig::windowHeight);
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;

	m_dxDevice->getContext()->RSSetViewports(1, &viewport);
}
