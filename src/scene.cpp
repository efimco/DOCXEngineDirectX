#include "scene.hpp"

#include <consoleapi.h>
#include <iostream>
#include <ranges>

#include "bakerNode.hpp"
#include "camera.hpp"
#include "light.hpp"
#include "material.hpp"
#include "primitive.hpp"
#include "texture.hpp"

Scene::Scene(const std::string_view name, ComPtr<ID3D11Device> device)
{
	this->name = name;
	m_device = device;
	nodeHandle = SceneNodeHandle::generateHandle();
}

SceneNodeHandle Scene::findHandleOfNode(SceneNode* node) const
{
	if (node == this)
	{
		return nodeHandle;
	}
	if (auto prim = dynamic_cast<Primitive*>(node))
	{
		const auto it = std::ranges::find_if(
			m_primitives,
			[prim](const std::pair<SceneNodeHandle, Primitive*>& x)
			{
				return x.second == prim;
			});
		if (it != m_primitives.end())
		{
			return it->first;
		}
	}
	if (auto light = dynamic_cast<Light*>(node))
	{
		const auto it = std::ranges::find_if(
			m_lights,
			[light](const std::pair<SceneNodeHandle, Light*>& x)
			{
				return x.second == light;
			});
		if (it != m_lights.end())
		{
			return it->first;
		}
	}
	if (auto camera = dynamic_cast<Camera*>(node))
	{
		const auto it = std::ranges::find_if(
			m_cameras,
			[camera](const std::pair<SceneNodeHandle, Camera*>& x)
			{
				return x.second == camera;
			});
		if (it != m_cameras.end())
		{
			return it->first;
		}
	}
	if (auto baker = dynamic_cast<Baker*>(node))
	{
		const auto it = std::ranges::find_if(
			m_bakers,
			[baker](const std::pair<SceneNodeHandle, Baker*>& x)
			{
				return x.second == baker;
			});
		if (it != m_bakers.end())
		{
			return it->first;
		}
	}
	if (auto bakerNode = dynamic_cast<BakerNode*>(node))
	{
		const auto it = std::ranges::find_if(
			m_bakerNodes,
			[bakerNode](const std::pair<SceneNodeHandle, BakerNode*>& x)
			{
				return x.second == bakerNode;
			});
		if (it != m_bakerNodes.end())
		{
			return it->first;
		}
	}
	return SceneNodeHandle::invalidHandle();
}

SceneNode* Scene::getNodeByHandle(const SceneNodeHandle handle)
{
	if (handle == nodeHandle)
	{
		return this;
	}
	if (const auto it = m_primitives.find(handle); it != m_primitives.end())
	{
		return it->second;
	}
	if (const auto it = m_lights.find(handle); it != m_lights.end())
	{
		return it->second;
	}
	if (const auto it = m_cameras.find(handle); it != m_cameras.end())
	{
		return it->second;
	}
	if (const auto it = m_bakers.find(handle); it != m_bakers.end())
	{
		return it->second;
	}
	if (const auto it = m_bakerNodes.find(handle); it != m_bakerNodes.end())
	{
		return it->second;
	}
	return nullptr;
}


bool Scene::isNameUsed(const std::string_view name) const
{
	return m_nodeNames.contains(name);
}

uint32_t& Scene::getNameCounter(std::string_view name)
{
	auto it = m_nodeNames.find(name);
	if (it == m_nodeNames.end())
	{
		it = m_nodeNames.emplace(name, 0).first;
	}
	return it->second;
}

void Scene::addChild(std::unique_ptr<SceneNode>&& child, int index)
{
	if (const auto light = dynamic_cast<Light*>(child.get()))
	{
		addLight(light);
	}
	else if (const auto primitive = dynamic_cast<Primitive*>(child.get()))
	{
		addPrimitive(primitive);
	}
	else if (const auto camera = dynamic_cast<Camera*>(child.get()))
	{
		addCamera(camera);
	}
	else if (const auto baker = dynamic_cast<Baker*>(child.get()))
	{
		addBaker(baker);
	}
	else if (const auto node = dynamic_cast<BakerNode*>(child.get()))
	{
		addBakerNode(node);
	}
	assert(getNodeByHandle(child->nodeHandle) == child.get());
	SceneNode::addChild(std::move(child), index);
}

void Scene::reparentChild(SceneNode* child, SceneNode* parent, int index)
{
	// both child and parent should be registered in this scene
	assert(getNodeByHandle(child->nodeHandle) == child);
	assert(getNodeByHandle(parent->nodeHandle) == parent);
	assert(parent->canBecomeParent);

	// if the child does not have a parent - you should use 'adoptClonedNode'
	assert(child->parent);

	auto movedChild = child->parent->removeChild(child);
	parent->addChild(std::move(movedChild), index);
}

void Scene::addPrimitive(Primitive* primitive)
{
	// Check if already registered (e.g., during reparenting)
	for (const auto& [handle, prim] : m_primitives)
	{
		if (prim == primitive)
			return;
	}
	validateName(primitive);

	SceneNodeHandle nodeHandle = SceneNodeHandle::generateHandle();
	primitive->nodeHandle = nodeHandle;
	m_primitives.emplace(nodeHandle, primitive);
}

void Scene::addLight(Light* light)
{
	// Check if already registered (e.g., during reparenting)
	for (const auto& [handle, l] : m_lights)
	{
		if (l == light)
			return;
	}
	validateName(light);

	SceneNodeHandle nodeHandle = SceneNodeHandle::generateHandle();
	light->nodeHandle = nodeHandle;
	m_lights.emplace(nodeHandle, light);
	setLightsDirty();
}

Light* Scene::getLightByID(const size_t id)
{
	const auto it = m_lights.find(SceneNodeHandle{ static_cast<int>(id) });
	if (it != m_lights.end())
	{
		return it->second;
	}
	return nullptr;
}

SceneUnorderedMap<Primitive*>& Scene::getPrimitives()
{
	return m_primitives;
}

Primitive* Scene::getPrimitiveByID(const size_t id)
{
	const auto it = m_primitives.find(SceneNodeHandle{ static_cast<int>(id) });
	if (it != m_primitives.end())
	{
		return it->second;
	}
	return nullptr;
}

SceneUnorderedMap<Light*>& Scene::getLights()
{
	return m_lights;
}

void Scene::addCamera(Camera* camera)
{
	// Check if already registered (e.g., during reparenting)
	for (const auto& [handle, c] : m_cameras)
	{
		if (c == camera)
			return;
	}
	validateName(camera);

	SceneNodeHandle nodeHandle = SceneNodeHandle::generateHandle();
	camera->nodeHandle = nodeHandle;
	m_cameras.emplace(nodeHandle, camera);
}

void Scene::addBaker(Baker* baker)
{
	validateName(baker);

	SceneNodeHandle nodeHandle = SceneNodeHandle::generateHandle();
	baker->nodeHandle = nodeHandle;
	m_bakers.emplace(nodeHandle, baker);

	addBakerNode(baker->lowPoly.get());
	addBakerNode(baker->highPoly.get());
}

void Scene::processPendingBakes()
{
	for (auto& [handle, baker] : m_bakers)
	{
		baker->processPendingBake();
	}
}

void Scene::checkTextureUpdates()
{
	for (auto& [name, texture] : m_textures)
	{
		if (!texture || texture->filepath.empty())
			continue;
		bool alreadyPending = std::ranges::any_of(m_pendingTextureReloads,
			[&name](const PendingTextureReload& p) { return p.name == name; });

		if (alreadyPending)
			continue;

		try
		{
			if (!std::filesystem::exists(texture->filepath))
				continue;
			auto ftime = std::filesystem::last_write_time(texture->filepath);
			if (ftime != texture->lastModifiedTime)
			{
				auto progress = std::make_shared<TextureLoadProgress>();
				auto future = Texture::loadTextureAsync(texture->filepath, texture->device, progress);
				m_pendingTextureReloads.push_back(PendingTextureReload{ name, std::move(future), progress });

				texture->lastModifiedTime = ftime;
			}
		}
		catch (const std::filesystem::filesystem_error&)
		{
			continue;
		}
		catch (const std::exception&)
		{
			continue;
		}
	}
}

void Scene::updateAsyncPendingTextureReloads() noexcept
{
	for (auto it = m_pendingTextureReloads.begin(); it != m_pendingTextureReloads.end();)
	{
		if (!it->progress->isCompleted)
		{
			++it;
			continue;
		}
		try
		{
			auto result = it->future.get();


			ComPtr<ID3D11DeviceContext> immContext;
			m_device->GetImmediateContext(&immContext);
			Texture::finalizeAsyncLoad(result, immContext);
			if (result.progress->hasFailed || !result.texture)
			{
				std::cerr << "Failed to reload texture " << it->name << ": " << result.progress->errorMessage << std::endl;
				continue;
			}

			auto existingTexture = m_textures[it->name];
			if (!existingTexture)
			{
				continue;
			}
			existingTexture->textureResource = result.texture->textureResource;
			existingTexture->srv = result.texture->srv;
			existingTexture->texDesc = result.texture->texDesc;
			std::cout << "Texture reloaded successfully: " << it->name << std::endl;
		}
		catch (const std::exception& e)
		{
			std::cerr << "Exception while reloading texture " << it->name << ": " << e.what() << std::endl;
		}
		it = m_pendingTextureReloads.erase(it);
	}
}

void Scene::addBakerNode(BakerNode* node)
{
	validateName(node);

	SceneNodeHandle nodeHandle = SceneNodeHandle::generateHandle();
	node->nodeHandle = nodeHandle;
	m_bakerNodes.emplace(nodeHandle, node);
}

size_t Scene::getPrimitiveCount() const
{
	return m_primitives.size();
}

std::shared_ptr<Texture> Scene::getTexture(const std::string_view name)
{
	const auto it = m_textures.find(name);
	if (it != m_textures.end())
	{
		return it->second;
	}
	return nullptr;
}

void Scene::addTexture(std::shared_ptr<Texture> texture)
{
	m_textures[texture->name] = std::move(texture);
}

std::shared_ptr<Material> Scene::getMaterial(const std::string_view name)
{
	const auto it = m_materials.find(name);
	if (it != m_materials.end())
	{
		return it->second;
	}
	return nullptr;
}

std::vector<std::string> Scene::getMaterialNames() const
{
	std::vector<std::string> materialNames;
	for (const auto& pair : m_materials)
	{
		materialNames.push_back(pair.first);
	}
	return materialNames;
}

SceneNode* Scene::getActiveNode() const
{
	return m_activeNode;
}

int32_t Scene::getActiveNodeID() const
{
	if (m_activeNode)
	{
		return static_cast<int32_t>(findHandleOfNode(m_activeNode));
	}
	return -1;
}


void Scene::setActiveNode(SceneNode* node, const bool addToSelection)
{
	if (!addToSelection)
	{
		clearSelectedNodes();
	}
	m_selectedNodes.emplace(node);

	m_activeNode = node;
}

void Scene::deselectNode(SceneNode* node)
{
	if (m_activeNode == node)
	{
		m_activeNode = nullptr;
	}
	m_selectedNodes.erase(node);
}

void Scene::clearSelectedNodes()
{
	m_selectedNodes.clear();
	m_activeNode = nullptr;
}

bool Scene::isNodeSelected(SceneNode* node) const
{
	return !m_selectedNodes.empty() && m_selectedNodes.contains(node);
}

void Scene::addMaterial(std::shared_ptr<Material> material)
{
	m_materials.emplace(material->name, material);
}

bool Scene::areLightsDirty() const
{
	return m_lightsAreDirty;
}

void Scene::setLightsDirty(const bool dirty)
{
	m_lightsAreDirty = dirty;
}

void Scene::setEnvironmentMap(const std::string& path)
{
	if (m_environmentMapPath != path)
	{
		m_environmentMapPath = path;
		m_environmentMapDirty = true;
	}
}

const std::string& Scene::getEnvironmentMapPath() const
{
	return m_environmentMapPath;
}

bool Scene::isEnvironmentMapDirty() const
{
	return m_environmentMapDirty;
}

void Scene::clearEnvironmentMapDirty()
{
	m_environmentMapDirty = false;
}

void Scene::setActiveCamera(Camera* camera)
{
	m_activeCamera = camera;
}

void Scene::deleteNode(SceneNode* node)
{
	node->nodeHandle = SceneNodeHandle::invalidHandle();

	while (!node->children.empty())
	{
		deleteNode(node->children.back().get());
	}
	if (m_nodeNames.contains(node->name))
	{
		if (m_nodeNames[node->name] == 0)
		{
			m_nodeNames.erase(node->name);
		}
		else
		{
			m_nodeNames[node->name]--;
		}
	}

	const SceneNodeHandle nodeHandle = findHandleOfNode(node);
	if (dynamic_cast<Primitive*>(node))
	{
		m_primitives.erase(nodeHandle);
	}
	else if (dynamic_cast<Light*>(node))
	{
		m_lights.erase(nodeHandle);
		setLightsDirty();
	}
	else if (const auto camera = dynamic_cast<Camera*>(node))
	{
		if (m_cameras.size() <= 1)
		{
			return;
		}
		m_cameras.erase(nodeHandle);
		if (m_activeCamera == camera)
		{
			m_activeCamera = nullptr;
		}
	}
	else if (dynamic_cast<Baker*>(node))
	{
		deleteNode(static_cast<Baker*>(node)->lowPoly.get());
		deleteNode(static_cast<Baker*>(node)->highPoly.get());
		m_bakers.erase(nodeHandle);
	}
	else if (dynamic_cast<BakerNode*>(node))
	{
		m_bakerNodes.erase(nodeHandle);
	}
	std::unique_ptr<SceneNode> ptr;
	if (node->parent)
	{
		ptr = node->parent->removeChild(node);
	}
	if (m_activeNode == node)
	{
		m_activeNode = nullptr;
	}
	m_selectedNodes.erase(node);
}

SceneNode* Scene::adoptClonedNode(
	std::unique_ptr<SceneNode>&& clonedNode,
	SceneNodeHandle preferredHandle,
	bool shouldValidateName)
{
	if (!preferredHandle.isValid())
	{
		preferredHandle = SceneNodeHandle::generateHandle();
	}

	// it's an error if a preferred handle is taken
	assert(getNodeByHandle(preferredHandle) == nullptr);
	// the parent of a cloned node must be nullptr
	assert(clonedNode->parent == nullptr);

	SceneNode* nodeClone = clonedNode.get();
	nodeClone->nodeHandle = preferredHandle;

	if (shouldValidateName)
	{
		validateName(nodeClone);
	}

	if (auto prim = dynamic_cast<Primitive*>(nodeClone))
	{
		m_primitives.emplace(preferredHandle, prim);
	}
	if (auto light = dynamic_cast<Light*>(nodeClone))
	{
		m_lights.emplace(preferredHandle, light);
		setLightsDirty();
	}
	if (auto camera = dynamic_cast<Camera*>(nodeClone))
	{
		m_cameras.emplace(preferredHandle, camera);
	}
	if (auto baker = dynamic_cast<Baker*>(nodeClone))
	{
		m_bakers.emplace(preferredHandle, baker);
		m_bakerNodes.emplace(SceneNodeHandle::generateHandle(), baker->lowPoly.get());
		m_bakerNodes.emplace(SceneNodeHandle::generateHandle(), baker->highPoly.get());
	}
	else if (auto bakerNode = dynamic_cast<BakerNode*>(nodeClone))
	{
		m_bakerNodes.emplace(preferredHandle, bakerNode);
	}

	SceneNode::addChild(std::move(clonedNode));
	return nodeClone;
}

void Scene::setReadBackID(float readBackID)
{
	m_readBackID = readBackID;
}

float Scene::getReadBackID()
{
	return m_readBackID;
}

Camera* Scene::getActiveCamera() const
{
	return m_activeCamera;
}


void Scene::validateName(SceneNode* node)
{
	const auto genericName = node->name.substr(0, node->name.find_last_of('.'));
	if (isNameUsed(genericName))
	{
		uint32_t& counter = getNameCounter(genericName);
		counter++;
		node->name = genericName + "." + std::to_string(counter);
	}
	else
	{
		getNameCounter(node->name) = 0;
	}
}

void Scene::updateState()
{
	processPendingBakes();
	checkTextureUpdates();
	updateAsyncPendingTextureReloads();
	updateAsyncImport();
}

void Scene::importModel(const std::string& filepath)
{
	if (filepath.empty())
	{
		std::cout << "No file path provided for model import." << std::endl;
		return;
	}
	if (m_isImporting)
	{
		std::cout << "Already importing a model, please wait." << std::endl;
		return;
	}

	m_isImporting = true;
	m_importProgress = std::make_shared<ImportProgress>();
	m_importFuture = GLTFModel::importModelAsync(filepath, m_device, m_importProgress);

	std::cout << "Started async import: " << filepath << std::endl;
}

void Scene::updateAsyncImport() noexcept
{
	if (m_isImporting)
	{
		if (m_importProgress)
		{
			const std::string stage = m_importProgress->getStage();
			if (!stage.empty())
			{
				std::cout << "Import progress: " << stage << std::endl;
			}
		}
		if (m_importProgress && (m_importProgress->isCompleted || m_importProgress->hasFailed))
		{
			try
			{
				auto importResult = m_importFuture.get();
				ComPtr<ID3D11DeviceContext> context;
				m_device->GetImmediateContext(&context);
				GLTFModel::finalizeAsyncImport(std::move(importResult), context, this);
				std::cout << "Model import completed." << std::endl;
			}
			catch (const std::exception& e)
			{
				std::cerr << "Model import failed: " << e.what() << std::endl;
			}
			m_isImporting = false;
			m_importProgress = nullptr;
		}
	}
}

std::shared_ptr<ImportProgress> Scene::getImportProgress() const noexcept
{
	return m_importProgress;
}

void Scene::saveScene(std::string_view filepath)
{
}

void Scene::loadScene(std::string_view filepath)
{
}

void Scene::clearScene()
{
	m_selectedNodes.clear();
	m_activeNode = nullptr;

	while (children.size() > 1)
	{
		SceneNode* nodeToDelete = nullptr;

		for (auto& child : children)
		{
			if (auto* cam = dynamic_cast<Camera*>(child.get()))
			{
				if (m_cameras.size() <= 1)
					continue;
			}
			nodeToDelete = child.get();
			break;
		}

		if (!nodeToDelete)
			break;

		deleteNode(nodeToDelete);
	}

	m_textures.clear();
	m_materials.clear();

}
