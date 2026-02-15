#include "uiManager.hpp"

#include <commdlg.h>
#include <cstddef>
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <memory>
#include <ranges>
#include <windows.h>

#include "imgui.h"
#include "imgui_internal.h"
#define IM_VEC2_CLASS_EXTRA
#include "ImGuizmo.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "appConfig.hpp"
#include "inputEventsHandler.hpp"
#include "passes/GBuffer.hpp"

#include "bakerNode.hpp"
#include "camera.hpp"
#include "light.hpp"
#include "material.hpp"
#include "primitive.hpp"
#include "primitiveData.hpp"
#include "scene.hpp"
#include "texture.hpp"
#include "shaderManager.hpp"

#include "commands/commandManager.hpp"
#include "commands/nodeCommand.hpp"
#include "commands/nodeSnapshot.hpp"
#include "commands/commandGroup.hpp"
#include "commands/scopedTransaction.hpp"
#include "commands/normalBakerCommand.hpp"

#include "passes/bakerPass.hpp"

#include "utility/SelectionHelpers.hpp"

enum class Theme
{
	Light,
	Dark,
	Classic
};
static int currentTheme = static_cast<int>(Theme::Dark);



static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
static bool gUseSnap(false);
static glm::ivec3 gSnapTranslate = { 1, 1, 1 };
static glm::ivec3 gSnapRotate = { 15, 15, 15 };
static glm::ivec3 gSnapScale = { 1, 1, 1 };

UIManager::UIManager(
	ComPtr<ID3D11Device> device,
	ComPtr<ID3D11DeviceContext> deviceContext,
	const HWND& hwnd)
	: m_commandManager(std::make_unique<CommandManager>())
	, m_hwnd(hwnd)
	, m_mousePos{}
	, m_scene(nullptr),
	BasePass(device, deviceContext)
{
	m_device = device;
	m_context = deviceContext;
	m_rtvCollector = std::make_unique<RTVCollector>();
	ImGui_ImplWin32_EnableDpiAwareness();
	const float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST));

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	m_io = &ImGui::GetIO();
	m_io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	m_io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;	 // Enable Gamepad Controls
	m_io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;	 // Enable Docking
	m_io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;	 // Enable Multi-Viewport / Platform Windows

	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(main_scale);
	// Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting
	// Style + calling this again)
	style.FontScaleDpi = main_scale;
	// Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for
	// documentation purpose)
	m_io->ConfigDpiScaleFonts = true;
	// [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale
	// fonts but _NOT_ scale sizes/padding for now.
	m_io->ConfigDpiScaleViewports = true;
	// [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular
	// ones.
	if (m_io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Style: rounded corners
	style.WindowRounding = 8.0f;
	style.ChildRounding = 8.0f;
	style.FrameRounding = 2.0f;
	style.PopupRounding = 8.0f;
	style.ScrollbarRounding = 8.0f;
	style.GrabRounding = 4.0f;
	style.TabRounding = 4.0f;

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(m_device.Get(), m_context.Get());

	// сonfigure ImGui multi-viewport support to use modern flip model swap chains
	if (m_io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		DXGI_SWAP_CHAIN_DESC flipDesc = {};
		flipDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		flipDesc.SampleDesc.Count = 1;
		flipDesc.SampleDesc.Quality = 0;
		flipDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		flipDesc.BufferCount = 2;
		flipDesc.Windowed = TRUE;
		flipDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		flipDesc.Flags = 0;

		extern void ImGui_ImplDX11_SetSwapChainDescs(const DXGI_SWAP_CHAIN_DESC * desc_templates,
			int desc_templates_count);
		ImGui_ImplDX11_SetSwapChainDescs(&flipDesc, 1);
	}
	m_isMouseInViewport = false;
}

UIManager::~UIManager()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void UIManager::draw(const ComPtr<ID3D11ShaderResourceView>& srv,
	Scene* scene,
	const glm::mat4& view,
	const glm::mat4& projection)
{
	beginDebugEvent(L"UIManager::draw");
	m_view = view;
	m_projection = projection;

	m_scene = scene;
	// Always check window validity before starting a new frame
	if (!IsWindow(m_hwnd) || !m_scene)
	{
		return;
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();


	showMainMenuBar();
	showInvisibleDockWindow();
	showSceneSettings();
	showBlendPaintWindow();
	showViewport(srv);
#ifdef _DEBUG
	showPassesWindow();
#endif
	drawSceneGraph();
	showProperties();
	showMaterialBrowser();
	processInputEvents();
	processNodeDeletion();
	processNodeDuplication();
	processUndoRedo();
	processThemeSelection();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	if (m_io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
	endDebugEvent();
}

uint32_t* UIManager::getMousePos()
{
	return m_mousePos;
}

void UIManager::showSceneSettings() const
{

	ImGui::Begin("SceneSettings");
	ImGui::Separator();

	constexpr float labelWidth = 150.0f;
	constexpr float fieldWidth = 80.0f;
	auto alignedDragFloat = [&](const char* label, float* value, float speed = 0.05f, float min = 0.0f, float max = 0.0f) {
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);
		ImGui::SameLine(labelWidth);
		ImGui::SetNextItemWidth(fieldWidth);
		ImGui::DragFloat((std::string("##") + label).c_str(), value, speed, min, max);
		};

	alignedDragFloat("Env Intensity", &AppConfig::IBLintensity, 0.02f, 0.0f, 10.0f);
	alignedDragFloat("Env Rotation", &AppConfig::IBLrotation);
	alignedDragFloat("Env Brightness", &AppConfig::backgroundIntensity, 0.02f, 0.0f, 1.0f);

	static char hdriPathBuffer[MAX_PATH] = "";
	ImGui::Text("HDRI Path");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 50.0f);
	bool hdriPathChanged = ImGui::InputText("##HDRI Path", hdriPathBuffer, MAX_PATH);
	ImGui::SameLine();
	if (ImGui::Button("..."))
	{
		const FileDialogResult result = openFileDialog(FileType::HDRI);
		if (result)
		{
			strncpy_s(hdriPathBuffer, MAX_PATH, result.fullPath.c_str(), _TRUNCATE);
			hdriPathChanged = true;
		}
	}
	if (hdriPathChanged)
	{
		m_scene->setEnvironmentMap(hdriPathBuffer);
		hdriPathChanged = false;
	}

	ImGui::Text("Blur");
	ImGui::SameLine();
	ImGui::Checkbox("##Blur Environment Map", &AppConfig::isBackgroundBlurred);
	if (AppConfig::isBackgroundBlurred)
	{

		alignedDragFloat("Blur Amount", &AppConfig::blurAmount, 0.02f, 0.0f, 1.0f);
	}

	ImGui::Separator();
	ImGui::Checkbox("Enable GTAO", &AppConfig::gtaoEnabled);
	ImGui::DragFloat("AO Radius", &AppConfig::aoRadius, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat("AO Intensity", &AppConfig::aoIntensity, 0.1f, 0.0f, 10.0f);
	ImGui::DragFloat("AO Bias", &AppConfig::aoBias, 0.001f, 0.0f, 1.0f);

#ifdef _DEBUG
	ImGui::Separator();
	ImGui::Checkbox("Regenerate Prefiltered Map", &AppConfig::regeneratePrefilteredMap);
#endif
	ImGui::Separator();

	// Debug BVH visualization
#ifdef DRAW_DEBUG_BVH
	ImGui::TextWrapped("Debug Visualization");
	ImGui::Checkbox("Show Leafs only", &AppConfig::showLeafsOnly);
	ImGui::SliderInt("BVH Max Depth", &AppConfig::maxBVHDepth, 0, 64, "%d");
	ImGui::SliderInt("BVH Min Depth", &AppConfig::minBVHDepth, 0, AppConfig::maxBVHDepth, "%d");
	ImGui::Separator();
#endif

	ImGui::Text("Theme: ");
	ImGui::Combo("Theme", &currentTheme, "Light\0Dark\0Classic\0");


	ImGui::TextWrapped("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / m_io->Framerate, m_io->Framerate);
	ImGui::End();
}

void UIManager::showMainMenuBar()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene", "Ctrl+N"))
			{
				m_selectedMaterial = nullptr;
				m_highlightedMaterial = nullptr;
				m_commandManager->clearUndoBuffer();
				m_commandManager->clearRedoBuffer();
				m_scene->clearScene();
				std::cout << "New scene triggered from menu\n";
			}
			if (ImGui::MenuItem("Open...", "Ctrl+O"))
			{
				const FileDialogResult result = openFileDialog(FileType::SCENE);
				if (result)
				{
					m_scene->loadScene(result.fullPath);
					std::cout << "Open scene triggered from menu\n";
				}
			}
			if (ImGui::MenuItem("Save", "Ctrl+S"))
			{
				const FileDialogResult result = openFileDialog(FileType::SCENE, true);
				if (result)
				{
					m_scene->saveScene(result.fullPath);
					std::cout << "Save scene triggered from menu\n";
				}
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Import Model...", "Ctrl+I"))
			{
				const FileDialogResult result = openFileDialog(FileType::MODEL);
				if (result)
				{
					m_scene->importModel(result.fullPath);
					std::cout << "Import model triggered from menu\n";
					m_importProgress = m_scene->getImportProgress();
				}
			}
			if (ImGui::MenuItem("Export...", "Ctrl+E"))
			{ /* TODO: Export */
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Exit", "Alt+F4"))
			{
				PostQuitMessage(0);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z", false, false))
			{
				m_commandManager->undo();
			}
			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, false))
			{
				m_commandManager->redo();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Duplicate", "Shift+D", false, false))
			{
				auto duplicationCommands = Selection::makeDuplicationCommands(m_scene, m_scene->getAllSelectedNodes());
				m_commandManager->commitCommand(std::move(duplicationCommands));
			}
			if (ImGui::MenuItem("Delete", "Del"))
			{
				auto removeCommands = Selection::makeRemoveCommands(m_scene, m_scene->getAllSelectedNodes());
				m_commandManager->commitCommand(std::move(removeCommands));
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Select All", "Ctrl+A"))
			{
			}
			if (ImGui::MenuItem("Deselect All", "Ctrl+D"))
			{
				m_scene->clearSelectedNodes();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("Viewport", nullptr, true))
			{ /* Toggle viewport */
			}
			if (ImGui::MenuItem("Material Browser", nullptr, true))
			{ /* Toggle material browser */
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Reset Layout"))
			{
				ImGui::LoadIniSettingsFromDisk("imgui.ini");
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Add"))
		{
			if (ImGui::BeginMenu("Mesh"))
			{
				auto getOrCreateDefaultMaterial = [this]() -> std::shared_ptr<Material> {
					auto mat = m_scene->getMaterial("Default");
					if (!mat)
					{
						mat = std::make_shared<Material>();
						mat->name = "Default";
						m_scene->addMaterial(mat);
					}
					return mat;
					};

				if (ImGui::MenuItem("Cube"))
				{
					auto cube = std::make_unique<Primitive>(m_device, BasePrimitiveType::CUBE, "Cube");
					std::vector<Vertex> vertices;
					std::vector<uint32_t> indices;
					Primitive::GenerateCube(vertices, indices);
					cube->setVertexData(std::move(vertices));
					cube->setIndexData(std::move(indices));
					cube->fillTriangles();
					cube->material = getOrCreateDefaultMaterial();
					m_scene->addChild(std::move(cube));
				}
				if (ImGui::MenuItem("Sphere"))
				{
					auto sphere = std::make_unique<Primitive>(m_device, BasePrimitiveType::SPHERE, "Sphere");
					std::vector<Vertex> vertices;
					std::vector<uint32_t> indices;
					Primitive::GenerateSphere(vertices, indices);
					sphere->setVertexData(std::move(vertices));
					sphere->setIndexData(std::move(indices));
					sphere->fillTriangles();
					sphere->material = getOrCreateDefaultMaterial();
					m_scene->addChild(std::move(sphere));
				}
				if (ImGui::MenuItem("Plane"))
				{
					auto plane = std::make_unique<Primitive>(m_device, BasePrimitiveType::PLANE, "Plane");
					std::vector<Vertex> vertices;
					std::vector<uint32_t> indices;
					Primitive::GeneratePlane(vertices, indices);
					plane->setVertexData(std::move(vertices));
					plane->setIndexData(std::move(indices));
					plane->fillTriangles();
					plane->material = getOrCreateDefaultMaterial();
					m_scene->addChild(std::move(plane));
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Light"))
			{
				auto lightPos = glm::vec3(0.0f, 1.0f, 0.0f);
				if (m_scene->getLights().size() > 0)
				{
					const auto lastAddedLight = std::prev(m_scene->getLights().end());
					lightPos = lastAddedLight->second->transform.position + glm::vec3(1.0f, 0.0f, 0.0f);
				}

				if (ImGui::MenuItem("Point Light"))
				{
					auto pointLight = std::make_unique<Light>(POINT_LIGHT, lightPos, "Point Light");
					m_scene->addChild(std::move(pointLight));
				}
				if (ImGui::MenuItem("Directional Light"))
				{
					auto dirLight = std::make_unique<Light>(DIRECTIONAL_LIGHT, lightPos, "Directional Light");
					m_scene->addChild(std::move(dirLight));
				}
				if (ImGui::MenuItem("Spot Light"))
				{
					auto spotLight = std::make_unique<Light>(SPOT_LIGHT, lightPos, "Spot Light");
					m_scene->addChild(std::move(spotLight));
				}
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Empty Node"))
			{ /* TODO: Add empty node */
			}
			if (ImGui::MenuItem("Baker"))
			{
				auto baker = std::make_unique<Baker>("Baker", m_device, m_context, m_scene);
				m_scene->addChild(std::move(baker));
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Render"))
		{
			if (ImGui::MenuItem("Render Image"))
			{ /* TODO: Render to file */
			}
			ImGui::Separator();
			if (ImGui::BeginMenu("Debug Views"))
			{
				if (ImGui::MenuItem("Albedo"))
				{ /* TODO: Show albedo only */
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("About"))
			{ /* TODO: Show about dialog */
			}
			if (ImGui::MenuItem("Documentation"))
			{ /* TODO: Open docs */
			}
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
}

void UIManager::showViewport(const ComPtr<ID3D11ShaderResourceView>& srv)
{
	constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("Viewport", nullptr, windowFlags);
	m_isMouseInViewport = ImGui::IsWindowHovered();


	constexpr auto uv0 = ImVec2(0, 0);
	constexpr auto uv1 = ImVec2(1, 1);
	static ImVec2 prevSize = ImGui::GetContentRegionAvail();
	ImVec2 size = ImGui::GetContentRegionAvail();
	size.x = static_cast<int>(size.x) % 2 == 0 ? size.x : size.x - 1;
	size.y = static_cast<int>(size.y) % 2 == 0 ? size.y : size.y - 1;

	AppConfig::viewportWidth = static_cast<int>(size.x);
	AppConfig::viewportHeight = static_cast<int>(size.y);

	if (size.x != prevSize.x || size.y != prevSize.y)
	{
		AppConfig::needsResize = true;
		prevSize = size;
	}

	const ImTextureRef tex = srv.Get();
	const auto mousePos =
		ImVec2(m_io->MousePos.x - ImGui::GetWindowPos().x, m_io->MousePos.y - ImGui::GetWindowPos().y);
	m_mousePos[0] = static_cast<uint32_t>(mousePos.x);
	m_mousePos[1] = static_cast<uint32_t>(mousePos.y);

	// Set up ImGuizmo to draw in this viewport
	const ImVec2 windowPos = ImGui::GetWindowPos();
	ImGuizmo::SetRect(windowPos.x, windowPos.y, size.x, size.y);

	ImGui::Image(tex, size, uv0, uv1);

	showChWSnappingOptions();
	showChWViewportOptions();
	if (m_importProgress)
	{
		showChWImportProgress(m_importProgress);
		if (m_importProgress->isCompleted.load())
		{
			m_importProgress = nullptr;
		}
	}


	processGizmo();

	ImGui::End();
	ImGui::PopStyleVar(3);
}


void UIManager::showChWSnappingOptions()
{
	// Snap toggle button in top right corner of viewport
	// Gizmo controls overlay
	ImGui::SetCursorPos(ImVec2(5.0f, 5.0f));
	ImGui::BeginChild("Snapping Options", ImVec2(200.0f, 150.0f), true, ImGuiWindowFlags_NoScrollbar);
	{
		if (ImGui::Button(gUseSnap ? "Snap: On" : "Snap: Off", ImVec2(-1.0f, 20.0f)))
		{
			gUseSnap = !gUseSnap;
		}

		if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
			mCurrentGizmoMode = ImGuizmo::LOCAL;
		ImGui::SameLine();
		if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
			mCurrentGizmoMode = ImGuizmo::WORLD;

		ImGui::PushItemWidth(-1.0f);
		ImGui::Text("[T]");
		ImGui::SameLine();
		ImGui::SliderInt("##SnapTranslate", &gSnapTranslate[0], 0, 100);
		ImGui::Text("[R]");
		ImGui::SameLine();
		;
		ImGui::SliderInt("##SnapRotate", &gSnapRotate[0], 1, 180);
		ImGui::Text("[S]");
		ImGui::SameLine();
		ImGui::SliderInt("##SnapScale", &gSnapScale[0], 0, 100);
		ImGui::PopItemWidth();
	}
	ImGui::EndChild();
}

void UIManager::showChWViewportOptions()
{
	// Viewport options child window
	ImGui::SetCursorPos(ImVec2(AppConfig::viewportWidth - 205.0f, 5.0f));
	ImGui::BeginChild("ViewportOptions", ImVec2(200.0f, 25.0f), false, ImGuiWindowFlags_NoScrollbar);
	{
		if (ImGui::RadioButton("Show UI Overlay", AppConfig::drawWSUI))
		{
			AppConfig::drawWSUI = !AppConfig::drawWSUI;
		}
	}
	ImGui::EndChild();
}

void UIManager::showChWImportProgress(std::shared_ptr<ImportProgress> progress)
{
	// Viewport options child window
	ImGui::SetCursorPos(ImVec2(AppConfig::viewportWidth - 300.0f, AppConfig::viewportHeight - 100.0f));
	ImGui::BeginChild("ImportProgress", ImVec2(250.0f, 150.0f), false, ImGuiWindowFlags_NoScrollbar);
	{
		ImGui::Text("Import Progress:");
		ImGui::ProgressBar(progress->progress.load(), ImVec2(-1.0f, 0.0f));
		ImGui::Text("%s", progress->getStage().c_str());
	}
	ImGui::EndChild();
}

void UIManager::showInvisibleDockWindow()
{
	constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_MenuBar;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("InvisibleDockSpaceWindow", nullptr, windowFlags);
	ImGui::PopStyleVar(3);

	const ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::End();
}

void UIManager::showMaterialBrowser()
{
	constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_AlwaysVerticalScrollbar;
	ImGui::Begin("Material Browser", nullptr, windowFlags);
	const std::vector<std::string> materialNames = m_scene->getMaterialNames();
	const ImVec2 contentRegion = ImGui::GetContentRegionAvail();
	constexpr float maxItemSize = 100.0f;
	const int itemsPerRow = static_cast<int>(floor(contentRegion.x / (maxItemSize + ImGui::GetStyle().ItemSpacing.x)));
	const float itemSize = (contentRegion.x - (itemsPerRow)*ImGui::GetStyle().ItemSpacing.x * 2) / itemsPerRow;
	for (int i = 0; i < materialNames.size(); i++)
	{
		const auto mat = m_scene->getMaterial(materialNames[i]);
		const ImTextureRef previewTexture = mat->preview.srv_preview.Get();
		ImGui::BeginGroup();
		{
			static ImVec4 outlineColor;

			if (m_selectedMaterial == mat)
			{
				outlineColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // green border
			}
			else if (m_highlightedMaterial == mat)
			{
				outlineColor = ImVec4(1.0f, 0.5f, 0.0f, 1.0f); // orange border
			}
			else
			{
				outlineColor = ImVec4(0.0f, 0.0f, 0.0f, 0.0f); // default black border
			}

			ImGui::PushStyleColor(ImGuiCol_Border, outlineColor);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 5.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
			if (ImGui::ImageButton(materialNames[i].c_str(), previewTexture, ImVec2(itemSize, itemSize)))
			{
				m_scene->setActiveNode(nullptr);
				m_scene->setReadBackID(-1.0f);
				if (m_selectedMaterial != mat)
				{
					m_selectedMaterial = mat;
					m_highlightedMaterial = nullptr;
				}
			}
			ImGui::PopStyleVar();
			ImGui::PopStyleVar();
			ImGui::PopStyleColor();

			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + itemSize);
			ImGui::TextWrapped("%s", materialNames[i].c_str());
			ImGui::PopTextWrapPos();
		}
		ImGui::EndGroup();
		if (itemsPerRow > 0 && (i + 1) % itemsPerRow != 0 && i < materialNames.size() - 1)
		{
			ImGui::SameLine();
		}
	}
	ImGui::End();
}

void UIManager::showBlendPaintWindow()
{
	if (!m_showBlendPaintWindow || !m_blendPaintPass)
		return;

	auto textureHistory = m_blendPaintPass->getTextureHistory();

	static bool wasPainting = false;

	constexpr float windowSize = 512.0f;
	constexpr float padding = 20.0f;
	ImGui::SetNextWindowSize(ImVec2(windowSize + padding, windowSize + padding + 60.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(
		ImVec2(256.0f, 256.0f + 60.0f),
		ImVec2(1024.0f, 1024.0f + 60.0f),
		[](ImGuiSizeCallbackData* data) { 			// Maintain square aspect ratio for the content area
			float targetSize = std::max(data->DesiredSize.x, data->DesiredSize.y - 60.0f);
			data->DesiredSize = ImVec2(targetSize, targetSize + 60.0f);
		}
	);

	std::string windowTitle = "Blend Paint - " + m_blendPaintPass->name + "###BlendPaint";

	if (ImGui::Begin(windowTitle.c_str(), &m_showBlendPaintWindow,
		ImGuiWindowFlags_NoCollapse))
	{
		ImVec2 contentSize = ImGui::GetContentRegionAvail();
		float imageSize = std::min(contentSize.x, contentSize.y - 30.0f); // Reserve space for buttons

		float offsetX = (contentSize.x - imageSize) * 0.5f;
		if (offsetX > 0)
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

		auto blendSRV = m_blendPaintPass->getBakedNormalSRV();
		ImVec2 imagePos = ImGui::GetCursorScreenPos();

		ImGui::InvisibleButton("##PaintArea", ImVec2(imageSize, imageSize)); //invisibleButton to capture input without moving window
		bool isHovered = ImGui::IsItemHovered();
		bool isActive = ImGui::IsItemActive();


		ImGui::GetWindowDrawList()->AddImage( // image on top of the invisible button
			blendSRV.Get(),
			imagePos,
			ImVec2(imagePos.x + imageSize, imagePos.y + imageSize)
		);

		bool isPainting = false;
		if (isHovered || isActive)
		{
			bool isActivelyPainting =
				ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
				ImGui::IsMouseDown(ImGuiMouseButton_Right);
			if (isActivelyPainting && !textureHistory->hasSnapshot(BKRCommand::k_blendPaintName))
			{
				textureHistory->startSnapshot(BKRCommand::k_blendPaintName, m_blendPaintPass->getBlendTexture());
			}

			ImVec2 mousePos = ImGui::GetMousePos();
			float localX = (mousePos.x - imagePos.x) / imageSize;
			float localY = (mousePos.y - imagePos.y) / imageSize;

			localX = std::clamp(localX, 0.0f, 1.0f); // Clamp to valid range
			localY = std::clamp(localY, 0.0f, 1.0f);

			if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				m_blendPaintPass->paintAtUV(localX, localY, 1.0f, m_blendBrushSize);
				isPainting = true;
			}
			else if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
			{
				m_blendPaintPass->paintAtUV(localX, localY, 0.0f, m_blendBrushSize);
				isPainting = true;
			}

			ImDrawList* drawList = ImGui::GetWindowDrawList(); // brush preview
			float brushRadius = (m_blendBrushSize / 100.0f) * imageSize * 0.5f;
			drawList->AddCircle(mousePos, brushRadius, IM_COL32(255, 255, 0, 200), 32, 2.0f);
		}

		if (wasPainting && !isPainting)
		{
			m_blendPaintPass->needsRebake = true;

			if (textureHistory->hasSnapshot(BKRCommand::k_blendPaintName))
			{
				m_commandManager->commitCommand(std::make_unique<BKRCommand::BlendMaskCreateDeltaCommand>(
					textureHistory,
					m_blendPaintPass));
				textureHistory->endSnapshot(BKRCommand::k_blendPaintName);
			}
		}
		wasPainting = isPainting;

		ImGui::Spacing();

		ImGui::SetNextItemWidth(150.0f);
		ImGui::SliderFloat("Brush Size", &m_blendBrushSize, 1.0f, 50.0f, "%.0f%%");

		ImGui::SameLine();
		if (ImGui::Button("Clear to White"))
		{
			textureHistory->startSnapshot(BKRCommand::k_blendPaintName, m_blendPaintPass->getBlendTexture());
			m_blendPaintPass->clearBlendTexture(1.0f);
			m_commandManager->commitCommand(std::make_unique<BKRCommand::BlendMaskCreateDeltaCommand>(
				textureHistory,
				m_blendPaintPass));
			textureHistory->endSnapshot(BKRCommand::k_blendPaintName);
			m_blendPaintPass->needsRebake = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear to Black"))
		{
			textureHistory->startSnapshot(BKRCommand::k_blendPaintName, m_blendPaintPass->getBlendTexture());
			m_blendPaintPass->clearBlendTexture(0.0f);
			m_commandManager->commitCommand(std::make_unique<BKRCommand::BlendMaskCreateDeltaCommand>(
				textureHistory,
				m_blendPaintPass));
			textureHistory->endSnapshot(BKRCommand::k_blendPaintName);
			m_blendPaintPass->needsRebake = true;
		}
	}
	ImGui::End();

	if (!m_showBlendPaintWindow)
	{
		m_blendPaintPass = nullptr;
	}
}

void UIManager::processInputEvents() const
{
	InputEvents::setMouseClicked(MouseButtons::LEFT_BUTTON, m_io->MouseClicked[0]);
	InputEvents::setMouseClicked(MouseButtons::MIDDLE_BUTTON, m_io->MouseClicked[2]);
	InputEvents::setMouseClicked(MouseButtons::RIGHT_BUTTON, m_io->MouseClicked[1]);

	InputEvents::setMouseDown(MouseButtons::LEFT_BUTTON, ImGui::IsMouseDown(ImGuiMouseButton_Left));
	InputEvents::setMouseDown(MouseButtons::MIDDLE_BUTTON, ImGui::IsMouseDown(ImGuiMouseButton_Middle));
	InputEvents::setMouseDown(MouseButtons::RIGHT_BUTTON, ImGui::IsMouseDown(ImGuiMouseButton_Right));

	InputEvents::setMouseInViewport(m_isMouseInViewport);

	InputEvents::setMouseDelta(m_io->MouseDelta.x, m_io->MouseDelta.y);
	InputEvents::setMouseWheel(m_io->MouseWheel);

	InputEvents::setKeyDown(KeyButtons::KEY_W, ImGui::IsKeyDown(ImGuiKey_W));
	InputEvents::setKeyDown(KeyButtons::KEY_A, ImGui::IsKeyDown(ImGuiKey_A));
	InputEvents::setKeyDown(KeyButtons::KEY_S, ImGui::IsKeyDown(ImGuiKey_S));
	InputEvents::setKeyDown(KeyButtons::KEY_D, ImGui::IsKeyDown(ImGuiKey_D));
	InputEvents::setKeyDown(KeyButtons::KEY_Q, ImGui::IsKeyDown(ImGuiKey_Q));
	InputEvents::setKeyDown(KeyButtons::KEY_E, ImGui::IsKeyDown(ImGuiKey_E));
	InputEvents::setKeyDown(KeyButtons::KEY_F, ImGui::IsKeyDown(ImGuiKey_F));
	InputEvents::setKeyDown(KeyButtons::KEY_LSHIFT, ImGui::IsKeyDown(ImGuiKey_LeftShift));
	InputEvents::setKeyDown(KeyButtons::KEY_LCTRL, ImGui::IsKeyDown(ImGuiKey_LeftCtrl));
	InputEvents::setKeyDown(KeyButtons::KEY_F12, ImGui::IsKeyDown(ImGuiKey_F12));
	InputEvents::setKeyDown(KeyButtons::KEY_F11, ImGui::IsKeyPressed(ImGuiKey_F11, false));
}

void UIManager::processGizmo()
{
	SceneNode* activeNode = m_scene->getActiveNode();
	if (activeNode == nullptr)
		return;
	if (!activeNode->movable)
		return;
	ImGuizmo::SetDrawlist();
	if (!ImGuizmo::IsUsing())
	{
		if (ImGui::IsKeyPressed(ImGuiKey_G))
			mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_R))
			mCurrentGizmoOperation = ImGuizmo::ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_S)) // r Key
			mCurrentGizmoOperation = ImGuizmo::SCALE;
		if (ImGui::IsKeyPressed(ImGuiKey_M))
			gUseSnap = !gUseSnap;
	}

	TScopedTransaction<Snapshot::SceneNodeTransform> nodeTransaction{ m_commandManager.get(), m_scene, activeNode };

	glm::mat4 worldMatrix = activeNode->getWorldMatrix();
	const auto matrix = glm::value_ptr(worldMatrix);
	const auto viewMatrix = const_cast<float*>(glm::value_ptr(m_view));
	const auto projectionMatrix = const_cast<float*>(glm::value_ptr(m_projection));
	glm::vec3 currentSnapValue;
	switch (mCurrentGizmoOperation)
	{
	case ImGuizmo::TRANSLATE:
		currentSnapValue = gSnapTranslate;
		break;
	case ImGuizmo::ROTATE:
		currentSnapValue = glm::vec3(gSnapRotate);
		break;
	case ImGuizmo::SCALE:
		currentSnapValue = gSnapScale;
		break;
	default:
		currentSnapValue = glm::vec3(0.0f);
		break;
	}
	ImGuizmo::Manipulate(viewMatrix, projectionMatrix, mCurrentGizmoOperation, mCurrentGizmoMode, matrix, nullptr,
		gUseSnap ? &currentSnapValue[0] : nullptr);
	if (ImGuizmo::IsUsing())
	{
		m_isMouseInViewport = false;

		// Convert the modified world matrix back to local space
		glm::mat4 newWorldMatrix = worldMatrix; // matrix was modified in-place
		glm::mat4 localMatrix;
		if (activeNode->parent && !dynamic_cast<Scene*>(activeNode->parent))
		{
			// Has a non-Scene parent: convert world -> local
			const glm::mat4 parentWorldInverse = glm::inverse(activeNode->parent->getWorldMatrix());
			localMatrix = parentWorldInverse * newWorldMatrix;
		}
		else
		{
			// No parent or parent is Scene: world == local
			localMatrix = newWorldMatrix;
		}
		float matrixTranslation[3], matrixRotation[3], matrixScale[3];
		ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(localMatrix), matrixTranslation, matrixRotation,
			matrixScale);
		activeNode->transform.position = glm::vec3(matrixTranslation[0], matrixTranslation[1], matrixTranslation[2]);
		activeNode->transform.rotation = glm::vec3(matrixRotation[0], matrixRotation[1], matrixRotation[2]);
		activeNode->transform.scale = glm::vec3(matrixScale[0], matrixScale[1], matrixScale[2]);
		if (gUseSnap && mCurrentGizmoOperation == ImGuizmo::TRANSLATE)
		{
			ImGuizmo::DrawGrid(viewMatrix, projectionMatrix, glm::value_ptr(newWorldMatrix), 100.0f);
		}
		if (dynamic_cast<Primitive*>(activeNode))
		{
		}
		if (dynamic_cast<Light*>(activeNode))
		{
			m_scene->setLightsDirty();
		}
	}
}

void UIManager::processNodeDuplication()
{
	SceneNode* activeNode = m_scene->getActiveNode();
	if (activeNode && ImGui::IsKeyPressed(ImGuiKey_D, false) && InputEvents::isKeyDown(KeyButtons::KEY_LSHIFT))
	{
		auto duplicationCommands = Selection::makeDuplicationCommands(m_scene, m_scene->getAllSelectedNodes());
		m_commandManager->commitCommand(std::move(duplicationCommands));
	}
}

void UIManager::processNodeDeletion()
{
	SceneNode* activeNode = m_scene->getActiveNode();
	if (!activeNode)
		return;
	if (!activeNode->deletable)
		return;
	if (ImGui::IsKeyPressed(ImGuiKey_Delete))
	{
		auto removeCommands = Selection::makeRemoveCommands(m_scene, m_scene->getAllSelectedNodes());
		m_commandManager->commitCommand(std::move(removeCommands));
	}
}

void UIManager::processUndoRedo() const
{
	// We put a merge fence when mouse is released
	const bool isMouseReleased =
		ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Right);
	if (isMouseReleased)
	{
		m_commandManager->setMergeFence();
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Z, false) && ImGui::IsKeyDown(ImGuiMod_Ctrl))
	{
		if (ImGui::IsKeyDown(ImGuiMod_Shift))
		{
			if (m_commandManager->hasRedoCommands())
			{
				m_commandManager->redo();
			}
		}
		else
		{
			if (m_commandManager->hasUndoCommands())
			{
				m_commandManager->undo();
			}
		}
	}
	else if (ImGui::IsKeyPressed(ImGuiKey_Y, false) && ImGui::IsKeyDown(ImGuiMod_Ctrl))
	{
		if (m_commandManager->hasRedoCommands())
		{
			m_commandManager->redo();
		}
	}
}

void UIManager::processThemeSelection() const
{
	switch (currentTheme)
	{
	case static_cast<int>(Theme::Light):
		ImGui::StyleColorsLight();
		break;
	case static_cast<int>(Theme::Dark):
		ImGui::StyleColorsDark();
		break;
	case static_cast<int>(Theme::Classic):
		ImGui::StyleColorsClassic();
		break;
	default:
		break;
	}
}

void UIManager::drawSceneGraph()
{
	lastNodesDrawn.clear();

	ImGui::Begin("Scene Graph");
	static ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
		ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY;

	const ImVec2 availableSize = ImGui::GetContentRegionAvail();
	if (ImGui::BeginTable("SceneGraph", 2, tableFlags, availableSize))
	{
		ImGuiMultiSelectIO* multiSelectIO = ImGui::BeginMultiSelect(
			ImGuiMultiSelectFlags_SelectOnClickRelease |
			ImGuiMultiSelectFlags_ClearOnEscape |
			ImGuiMultiSelectFlags_ClearOnClickVoid |
			ImGuiMultiSelectFlags_BoxSelect1d);
		handleNodeMultiSelection(multiSelectIO);

		ImGui::TableSetupColumn("(o)", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_IndentDisable, 25.0f);
		ImGui::TableSetupColumn(m_scene->name.c_str(), ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_IndentEnable);
		ImGui::TableHeadersRow();
		drawNode(m_scene);

		multiSelectIO = ImGui::EndMultiSelect();
		handleNodeMultiSelection(multiSelectIO);

		ImGui::EndTable();

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE"))
			{
				SceneNode* draggedNode = *static_cast<SceneNode**>(payload->Data);
				if (!m_scene->isNodeSelected(draggedNode))
				{
					bool isCtrlPressed = InputEvents::isKeyDown(KeyButtons::KEY_LCTRL);
					m_scene->setActiveNode(draggedNode, isCtrlPressed);
				}

				auto moveCommands = Selection::makeReparentCommands(m_scene, m_scene, m_scene->getAllSelectedNodes(), Selection::DropZone::AsChild);
				if (!moveCommands->isEmpty())
				{
					m_commandManager->commitCommand(std::move(moveCommands));
				}
			}
			ImGui::EndDragDropTarget();
		}
	}

	ImGui::End();
}

void UIManager::handleNodeSelection(SceneNode* node)
{
	bool isCtrlPressed = InputEvents::isKeyDown(KeyButtons::KEY_LCTRL);
	float readBackID = m_scene->getReadBackID();

	if (ImGui::IsItemToggledSelection())
	{
		const auto prim = dynamic_cast<Primitive*>(node);
		if (prim)
		{
			m_highlightedMaterial = prim->material;
		}
		m_selectedMaterial = nullptr;
		m_scene->setReadBackID(-1.0f);
	}
	else if (readBackID == 0)
	{
		m_scene->clearSelectedNodes();
		m_highlightedMaterial = nullptr;
		m_selectedMaterial = nullptr;
	}
	else if (readBackID > 0)
	{
		auto* selectedNode = m_scene->getNodeByHandle(SceneNodeHandle(static_cast<int32_t>(readBackID)));
		if (!selectedNode)
			return;
		m_scene->setActiveNode(selectedNode, isCtrlPressed);
		const auto prim = dynamic_cast<Primitive*>(selectedNode);
		if (prim)
		{
			m_highlightedMaterial = prim->material;
			m_selectedMaterial = nullptr;
		}
	}
}

void UIManager::handleNodeDragDrop(SceneNode* node)
{
	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("SCENE_NODE", &node, sizeof(SceneNode*));
		ImGui::Text("%s", node->name.c_str());
		ImGui::EndDragDropSource();
	}

	const ImVec2 itemMin = ImGui::GetItemRectMin();
	const ImVec2 itemMax = ImGui::GetItemRectMax();
	const float itemHeight = itemMax.y - itemMin.y;
	const float mouseY = ImGui::GetMousePos().y;

	const float lowerThreshold = itemMax.y - itemHeight * 0.25f;

	using DropZone = Selection::DropZone;
	DropZone dropZone = DropZone::AsChild;

	if (mouseY > lowerThreshold)
		dropZone = DropZone::Below;

	// If target is Scene (root), always treat as "AsChild" without position change
	const bool targetIsScene = dynamic_cast<Scene*>(node) != nullptr;
	if (targetIsScene)
		dropZone = DropZone::AsChild;

	if (ImGui::BeginDragDropTarget())
	{
		// Draw visual indicator for drop position (only for above/below)
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImU32 lineColor = IM_COL32(100, 150, 255, 255);
		const float lineThickness = 3.0f;

		if (dropZone == DropZone::Below)
		{
			drawList->AddLine(ImVec2(itemMin.x, itemMax.y), ImVec2(itemMax.x, itemMax.y), lineColor, lineThickness);
		}
		else
		{
			drawList->AddRect(itemMin, itemMax, lineColor, 0.0f, 0, lineThickness);
		}

		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE"))
		{
			SceneNode* draggedNode = *static_cast<SceneNode**>(payload->Data);
			if (!m_scene->isNodeSelected(draggedNode))
			{
				bool isCtrlPressed = InputEvents::isKeyDown(KeyButtons::KEY_LCTRL);
				m_scene->setActiveNode(draggedNode, isCtrlPressed);
			}
			if (!node->canBecomeParent)
			{
				ImGui::EndDragDropTarget();
				return;
			}
			auto moveCommand = Selection::makeReparentCommands(m_scene, node, m_scene->getAllSelectedNodes(), dropZone);
			if (!moveCommand->isEmpty())
			{
				m_commandManager->commitCommand(std::move(moveCommand));

				// Expand tree node upon drag and drop
				const char* icon = getNodeIcon(node);
				char label[256];
				snprintf(label, sizeof(label), "%s %s", icon, node->name.c_str());
				ImGui::TreeNodeSetOpen(ImGui::GetID(label), true);
			}
		}
		ImGui::EndDragDropTarget();
	}
}

void UIManager::drawNode(SceneNode* node)
{
	lastNodesDrawn.emplace_back(node);

	if (dynamic_cast<Scene*>(node))
	{
		for (const auto& child : node->children)
		{
			drawNode(child.get());
		}
		return;
	}

	ImGui::PushID(node);
	ImGui::TableNextRow();

	// Column 0: Visibility checkbox (reset indent to align all checkboxes)
	ImGui::TableNextColumn();

	bool isVisible = node->getVisibility();
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ImGui::GetFrameHeight() * 0.2f);
	if (ImGui::Checkbox("##visible", &isVisible))
	{
		node->setVisibility(isVisible);
	}
	ImGui::PopStyleVar();


	// Column 1: Tree node
	ImGui::TableNextColumn();

	const auto* baker = dynamic_cast<Baker*>(node);
	const bool isFolder = !node->children.empty() || baker;
	const bool isSelected = m_scene->isNodeSelected(node);
	const char* icon = getNodeIcon(node);

	// Build label once
	char label[256];
	snprintf(label, sizeof(label), "%s %s", icon, node->name.c_str());

	ImGuiSelectionUserData selectionUserData = lastNodesDrawn.size() - 1;
	ImGui::SetNextItemSelectionUserData(selectionUserData);

	ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_SpanAvailWidth;
	if (isSelected)
		nodeFlags |= ImGuiTreeNodeFlags_Selected;

	if (isFolder)
	{
		nodeFlags |= ImGuiTreeNodeFlags_OpenOnArrow;
		const bool open = ImGui::TreeNodeEx(label, nodeFlags);
		handleNodeDragDrop(node);
		handleNodeSelection(node);

		if (open)
		{
			for (const auto& child : node->children)
			{
				drawNode(child.get());
			}
			if (baker)
			{
				drawNode(baker->lowPoly.get());
				drawNode(baker->highPoly.get());
			}
			ImGui::TreePop();
		}
	}
	else
	{
		nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		ImGui::TreeNodeEx(label, nodeFlags);
		handleNodeDragDrop(node);
		handleNodeSelection(node);
	}
	ImGui::PopID();
}

const char* UIManager::getNodeIcon(SceneNode* node)
{
	if (dynamic_cast<Light*>(node))
	{
		return "[L]"; // Light
	}
	else if (dynamic_cast<Primitive*>(node))
	{
		return "[P]"; // Primitive
	}
	else if (dynamic_cast<Camera*>(node))
	{
		return "[C]"; // Camera
	}
	else if (dynamic_cast<Baker*>(node))
	{
		return "[BKR]";
	}
	else if (dynamic_cast<LowPolyNode*>(node))
	{
		return "[LP]";
	}
	else if (dynamic_cast<HighPolyNode*>(node))
	{
		return "[HP]";
	}
	else if (!node->children.empty())
	{
		return "[SN]"; // Folder
	}

	return "[D]"; // Default document icon
}

void UIManager::handleNodeMultiSelection(ImGuiMultiSelectIO* multiSelectIO)
{
	for (const ImGuiSelectionRequest& req : multiSelectIO->Requests)
	{
		if (req.Type == ImGuiSelectionRequestType_SetAll)
		{
			if (req.Selected)
			{
				for (SceneNode* node : lastNodesDrawn)
				{
					m_scene->setActiveNode(node, true);
				}
			}
			else
			{
				m_scene->clearSelectedNodes();
			}
		}
		if (req.Type == ImGuiSelectionRequestType_SetRange)
		{
			for (ImGuiSelectionUserData idx = req.RangeFirstItem; idx <= req.RangeLastItem; idx++)
			{
				if (req.Selected)
				{
					m_scene->setActiveNode(lastNodesDrawn[idx], true);
				}
				else
				{
					m_scene->deselectNode(lastNodesDrawn[idx]);
				}
			}
		}
	}
}

void UIManager::showPassesWindow()
{
	ImGui::Begin("Passes");
	auto& rtvMap = m_rtvCollector->getRTVMap();

	static std::vector<std::string> rtvNameStrings;
	static std::vector<const char*> rtvNames;
	static size_t lastMapSize = 0;

	// Only rebuild when map size changes
	if (rtvMap.size() != lastMapSize)
	{
		rtvNameStrings.clear();
		rtvNames.clear();
		rtvNameStrings.reserve(rtvMap.size());
		rtvNames.reserve(rtvMap.size());
		for (const auto& name : rtvMap | std::views::keys)
		{
			rtvNameStrings.push_back(name);
		}
		for (const auto& name : rtvNameStrings)
		{
			rtvNames.push_back(name.c_str());
		}
		lastMapSize = rtvMap.size();
	}

	static int currentItemIndex = 0;
	if (rtvNames.empty())
	{
		ImGui::End();
		return;
	}

	if (currentItemIndex >= static_cast<int>(rtvNames.size()))
		currentItemIndex = 0;

	ImGui::Combo("RTVList", &currentItemIndex, rtvNames.data(), static_cast<int>(rtvNames.size()));

	const ImTextureRef tex = rtvMap[rtvNameStrings[currentItemIndex]].Get();
	constexpr auto uv0 = ImVec2(0, 0);
	constexpr auto uv1 = ImVec2(1, 1);
	ImVec2 size = ImGui::GetContentRegionAvail();

	// get aspect ratio from the actual texture
	ComPtr<ID3D11Resource> resource;
	rtvMap[rtvNameStrings[currentItemIndex]]->GetResource(&resource);
	ComPtr<ID3D11Texture2D> texture;
	resource.As(&texture);
	D3D11_TEXTURE2D_DESC desc;
	texture->GetDesc(&desc);
	ImGui::Text("Texture: %ux%u, Format: %d", desc.Width, desc.Height, desc.Format);
	ImGui::Text("Format: %d (10=R16G16B16A16_FLOAT)", desc.Format);

	float aspectRatio = static_cast<float>(desc.Width) / static_cast<float>(desc.Height);

	size.x = size.y * aspectRatio;
	ImGui::Image(tex, size, uv0, uv1);
	ImGui::End();
}

void UIManager::showProperties()
{
	ImGui::Begin("Properties");
	if (m_selectedMaterial)
	{
		showMaterialProperties(m_selectedMaterial);
		ImGui::End();
		return;
	}
	else if (m_scene->getActiveNode())
	{
		ImGui::Text("Selected Node: %s", m_scene->getActiveNode()->name.c_str());
		if (dynamic_cast<Primitive*>(m_scene->getActiveNode()))
		{
			showPrimitiveProperties(dynamic_cast<Primitive*>(m_scene->getActiveNode()));
		}
		else if (dynamic_cast<Light*>(m_scene->getActiveNode()))
		{
			showLightProperties(dynamic_cast<Light*>(m_scene->getActiveNode()));
		}
		else if (dynamic_cast<Camera*>(m_scene->getActiveNode()))
		{
			showCameraProperties(dynamic_cast<Camera*>(m_scene->getActiveNode()));
		}
		else if (dynamic_cast<BakerNode*>(m_scene->getActiveNode()))
		{
			showBakerProperties(dynamic_cast<BakerNode*>(m_scene->getActiveNode()));
		}
	}
	ImGui::End();
}

void UIManager::showPrimitiveProperties(Primitive* primitive) const
{
	{
		TScopedTransaction<Snapshot::SceneNodeTransform> nodeTransaction{ m_commandManager.get(), m_scene, primitive };

		ImGui::DragFloat3("Position", &primitive->transform.position[0], 0.1f);
		ImGui::DragFloat3("Rotation", &primitive->transform.rotation[0], 0.1f);
		ImGui::DragFloat3("Scale", &primitive->transform.scale[0], 0.1f);
	}

	{
		const auto& materialNames = m_scene->getMaterialNames();
		int currentMaterialIndex = -1;
		// Find current material index
		if (currentMaterialIndex < 0 && primitive->material)
		{
			int index = 0;
			for (const auto& name : materialNames)
			{
				if (m_scene->getMaterial(name) == primitive->material)
				{
					currentMaterialIndex = index;
					break;
				}
				index++;
			}
		}

		// Save previous material index
		const int previousMaterialIndex = currentMaterialIndex;
		if (ImGui::BeginCombo("Material",
			currentMaterialIndex >= 0 && currentMaterialIndex < materialNames.size()
			? materialNames[currentMaterialIndex].c_str()
			: "Select Material"))
		{
			for (int n = 0; n < static_cast<int>(materialNames.size()); n++)
			{
				const bool isSelected = (currentMaterialIndex == n);
				ImGui::Image(m_scene->getMaterial(materialNames[n])->preview.srv_preview.Get(), ImVec2(32, 32));
				ImGui::SameLine();
				if (ImGui::Selectable(materialNames[n].c_str(), isSelected))
				{
					currentMaterialIndex = n;
					if (currentMaterialIndex != previousMaterialIndex)
					{
						TScopedTransaction<Snapshot::SceneNodeCopy> nodeTransaction{ m_commandManager.get(), m_scene,
																					primitive };
						const std::shared_ptr<Material> selectedMaterial = m_scene->getMaterial(materialNames[n]);
						primitive->material = selectedMaterial;
					}
				}
				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}
}

void UIManager::showMaterialProperties(std::shared_ptr<Material> material) const
{
	ImGui::Text("Name: %s", material->name.c_str());

	ImGui::Separator();

	ImGui::Text("Albedo: ");
	ImGui::Checkbox("##Use Albedo", &material->useAlbedo);
	ImGui::SameLine();
	if (material->albedo)
	{
		if (ImGui::ImageButton("##Albedo", material->albedo->srv.Get(), ImVec2(128, 128)))
		{
			FileDialogResult result = openFileDialog(FileType::IMAGE);
			if (result)
			{
				std::shared_ptr<Texture> newAlbedo = std::make_shared<Texture>(result.fullPath, m_device);
				m_scene->addTexture(newAlbedo);
				material->albedo = newAlbedo;
				material->needsPreviewUpdate = true;
			}
		}
	}
	ImGui::Text("Albedo Color: ");
	ImGui::SameLine();
	if (ImGui::ColorEdit4("##AlbedoColor", glm::value_ptr(material->albedoColor), ImGuiColorEditFlags_NoInputs))
	{
		material->needsPreviewUpdate = true;
	}

	ImGui::Separator();

	ImGui::Text("MetallicRoughness: ");
	ImGui::Checkbox("##Use MetallicRoughness", &material->useMetallicRoughness);
	if (material->metallicRoughness)
	{
		if (ImGui::ImageButton("##MetallicRoughness", material->metallicRoughness->srv.Get(), ImVec2(128, 128)))
		{
			FileDialogResult result = openFileDialog(FileType::IMAGE);
			if (result)
			{
				std::shared_ptr<Texture> newMetallicRoughness = std::make_shared<Texture>(result.fullPath, m_device);
				m_scene->addTexture(newMetallicRoughness);
				material->metallicRoughness = newMetallicRoughness;
				material->needsPreviewUpdate = true;
			}
		}
	}
	if (ImGui::DragFloat("Metallic", &material->metallicValue, 0.01f, 0.0f, 1.0f))
	{
		material->needsPreviewUpdate = true;
	}
	if (ImGui::DragFloat("Roughness", &material->roughnessValue, 0.01f, 0.04f, 1.0f))
	{
		material->needsPreviewUpdate = true;
	}

	ImGui::Separator();

	ImGui::Text("Normal: ");
	ImGui::Checkbox("##Use Normal", &material->useNormal);
	ImGui::SameLine();
	ImTextureRef normalTex = material->normal ? material->normal->srv.Get() : nullptr;

	if (ImGui::ImageButton("##Normal", normalTex, ImVec2(128, 128)))
	{
		FileDialogResult result = openFileDialog(FileType::IMAGE);
		if (result)
		{
			std::shared_ptr<Texture> newNormal = std::make_shared<Texture>(result.fullPath, m_device);
			m_scene->addTexture(newNormal);
			material->normal = newNormal;
			material->needsPreviewUpdate = true;
		}
	}
	ImGui::Checkbox("Flip Y", &material->flipY);

}

void UIManager::showLightProperties(Light* light) const
{
	TScopedTransaction<Snapshot::SceneNodeCopy> nodeTransaction{ m_commandManager.get(), m_scene, light };

	ImGui::Combo("Type", reinterpret_cast<int*>(&light->type), "Point Light\0Directional Light\0Spot Light\0");
	ImGui::DragFloat3("Position", &light->transform.position[0], 0.1f);
	ImGui::DragFloat3("Rotation", &light->transform.rotation[0], 0.1f);
	ImGui::ColorEdit3("Color", &light->color[0]);
	ImGui::DragFloat("Intensity", &light->intensity, 0.1f, 0.0f, 100.0f);
	ImGui::DragFloat("Radius", &light->radius, 0.1f, 0.0f, 100.0f);
}


void UIManager::showCameraProperties(Camera* camera) const
{
	TScopedTransaction<Snapshot::SceneNodeCopy> nodeTransaction{ m_commandManager.get(), m_scene, camera };

	ImGui::DragFloat3("Position", &camera->orbitPivot[0], 0.1f);
	ImGui::DragFloat3("Rotation", &camera->transform.rotation[0], 0.1f);
	ImGui::DragFloat("Fov", &camera->fov, 0.1f, 1.0f, 120.0f);
	ImGui::DragFloat("Near Clip", &camera->nearPlane, 0.01f, 0.01f, 100.0f);
	ImGui::DragFloat("Far Clip", &camera->farPlane, 1.0f, 10.0f, 10000.0f);
}

void UIManager::showBakerProperties(BakerNode* bakerNode)
{
	auto baker = dynamic_cast<Baker*>(bakerNode);
	if (!baker)
	{
		return;
	}

	static bool showErrorPopup = false;
	static std::string errorMessage;

	ImGui::Text("Baking Settings:");
	static bool cageWasChanged = false;


	bool isPainting = false;
	if (ImGui::DragFloat("Cage Offset", &baker->cageOffset, 0.01f, 0.0f, 5.0f))
	{
		isPainting = true;
	}

	if (cageWasChanged && !isPainting)
	{
		baker->requestBake();
	}
	cageWasChanged = isPainting;

	bool checkboxValue = baker->useSmoothedNormals;
	if (ImGui::Checkbox("Use Smoothed Normals", &checkboxValue))
	{
		m_commandManager->commitCommand(
			std::make_unique<BKRCommand::ToggleSmoothNormalsCommand>(m_scene, baker, checkboxValue));
	}
	constexpr uint32_t minVal = 2;
	constexpr uint32_t maxVal = 4096;
	ImGui::DragScalar("Texture Size", ImGuiDataType_U32, &baker->textureWidth, 2.0f, &minVal, &maxVal);

	for (auto pass : baker->getPasses())
	{
		ImGui::PushID(pass.get());

		ImGui::Text("%s", pass->name.c_str());

		// To interact with ImGui those strings are treated as c-strings.
		// Avoid using ops that might change size, like assignment operator or '.append()'.
		std::string filenameBuffer;
		std::string directoryBuffer;

		static constexpr size_t k_maxFilenameLength = 2048;
		static constexpr size_t k_maxDirectoryLength = 2048;
		filenameBuffer.resize(std::max(k_maxFilenameLength, pass->filename.size()));
		directoryBuffer.resize(std::max(k_maxDirectoryLength, pass->directory.size()));

		std::ranges::copy(pass->filename, filenameBuffer.begin());
		std::ranges::copy(pass->directory, directoryBuffer.begin());

		// Directory input with browse button
		ImGui::Text("Directory:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100.0f);
		bool dirChanged = ImGui::InputText("##Dir", directoryBuffer.data(), directoryBuffer.size());

		ImGui::SameLine();
		if (ImGui::Button("..."))
		{
			FileDialogResult result = openFileDialog(FileType::UNKNOWN, true);
			if (result)
			{
				filenameBuffer.clear();
				directoryBuffer.clear();
				filenameBuffer.resize(std::max(k_maxFilenameLength, result.filename.size()));
				directoryBuffer.resize(std::max(k_maxDirectoryLength, result.directory.size()));
				std::ranges::copy(result.filename, filenameBuffer.begin());
				std::ranges::copy(result.directory, directoryBuffer.begin());
				dirChanged = true;
			}
		}

		// Filename input
		ImGui::Text("Filename:");
		ImGui::SameLine();
		bool nameChanged = ImGui::InputText("##Filename", filenameBuffer.data(), filenameBuffer.size());

		if (dirChanged || nameChanged)
		{
			if (strnlen_s(filenameBuffer.data(), filenameBuffer.size()) > 0)
			{
				if (char* foundChar = strchr(filenameBuffer.data(), '.'))
				{
					(*foundChar) = 0;
				}
				strncat_s(filenameBuffer.data(), filenameBuffer.size(), ".png", sizeof("png"));
			}

			m_commandManager->commitCommand(
				std::make_unique<BKRCommand::SelectOutputCommand>(
					m_scene, pass, filenameBuffer.c_str(), directoryBuffer.c_str()));
		}

		if (pass->bakedNormalExists())
		{
			if (ImGui::Button("Preview"))
			{
				pass->previewBakedNormal();
			}
		}

		if (pass->getBlendTextureSRV())
		{
			ImGui::SameLine();
			if (ImGui::Button("Paint Blend Rays"))
			{
				m_blendPaintPass = pass;
				m_showBlendPaintWindow = true;
			}
		}

		ImGui::Separator();
		ImGui::PopID();
	}


	if (ImGui::Button("Start Bake"))
	{
		bool hasInvalidPath = false;
		std::string invalidPassName;

		if (baker->getPasses().empty())
		{
			showErrorPopup = true;
			errorMessage = "No Primitives Assigned\n\nPlease assign at least one primitive to bake.";
			return;
		}
		for (auto& pass : baker->getPasses())
		{
			if (pass->directory.empty() || pass->filename.empty())
			{
				hasInvalidPath = true;
				invalidPassName = pass->name;
				break;
			}

			if (!std::filesystem::exists(pass->directory)) // Also check if directory exists
			{
				hasInvalidPath = true;
				invalidPassName = pass->name + " (directory doesn't exist: " + pass->directory + ")";
				break;
			}
		}

		if (hasInvalidPath)
		{
			errorMessage = "Invalid output path for: " + invalidPassName + "\n\nPlease set a valid directory and filename before baking.";
			showErrorPopup = true;
		}
		else
		{
			baker->requestBake();
		}
	}

	// Center the popup on the viewport window
	ImGuiWindow* viewportWindow = ImGui::FindWindowByName("Viewport");
	assert(viewportWindow);

	ImVec2 center(viewportWindow->Pos.x + viewportWindow->Size.x * 0.5f,
		viewportWindow->Pos.y + viewportWindow->Size.y * 0.5f);

	if (showErrorPopup)
	{
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(450.0f, 0.0f), ImGuiCond_Always);
		ImGui::OpenPopup("Error##BakeError");
		showErrorPopup = false;
	}

	if (ImGui::BeginPopupModal("Error##BakeError", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextWrapped("%s", errorMessage.c_str());
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		float buttonWidth = 120.0f;
		float windowWidth = ImGui::GetWindowSize().x;
		ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);

		if (ImGui::Button("OK", ImVec2(buttonWidth, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

FileDialogResult UIManager::openFileDialog(const FileType outFileType, bool saveFile)
{
	OPENFILENAME ofn;
	char fileName[260] = { 0 };
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFile = fileName;
	ofn.nMaxFile = sizeof(fileName);

	if (outFileType == FileType::IMAGE)
	{
		ofn.lpstrFilter = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.hdr;*.exr\0All Files\0*.*\0";
	}
	else if (outFileType == FileType::HDRI)
	{
		ofn.lpstrFilter = "HDRI Files\0*.hdr\0";
	}
	else if (outFileType == FileType::MODEL)
	{
		ofn.lpstrFilter = "Model Files\0*.gltf;*.glb;*.obj\0All Files\0*.*\0";
	}
	else
	{
		ofn.lpstrFilter = "All Files\0*.*\0";
	}
	ofn.nFilterIndex = 1;
	ofn.lpstrTitle = "Select a Texture";
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | (saveFile ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

	bool success = saveFile ? GetSaveFileNameA(&ofn) == TRUE : GetOpenFileNameA(&ofn) == TRUE;

	if (success)
	{
		FileDialogResult result;
		result.fullPath = std::string(ofn.lpstrFile);

		size_t lastSlash = result.fullPath.find_last_of("\\/");
		if (lastSlash != std::string::npos)
		{
			result.directory = result.fullPath.substr(0, lastSlash);
			result.filename = result.fullPath.substr(lastSlash + 1);
		}
		else
		{
			result.filename = result.fullPath;
		}

		return result;
	}

	return FileDialogResult();
}
