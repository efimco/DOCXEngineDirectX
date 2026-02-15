#pragma once

#include <d3d11_4.h>
#include <future>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <wrl.h>

#include "gltfImporter.hpp"
#include "sceneNode.hpp"
#include "sceneNodeHandle.hpp"
#include "utility/stringUnorderedMap.hpp"

class Primitive;
struct Texture;
struct Material;
class Light;
class Camera;
class Baker;
class BakerNode;

struct PendingTextureReload;

using namespace Microsoft::WRL;


class Scene : public SceneNode
{
public:
	explicit Scene(std::string_view name = "Default Scene", ComPtr<ID3D11Device> device = nullptr);
	~Scene() override = default;

	SceneNodeHandle findHandleOfNode(SceneNode* node) const;
	SceneNode* getNodeByHandle(SceneNodeHandle handle);
	bool isNameUsed(std::string_view name) const;
	uint32_t& getNameCounter(std::string_view name);

	void addChild(std::unique_ptr<SceneNode>&& child, int index = -1) override;
	void reparentChild(SceneNode* child, SceneNode* parent, int index = -1);

	SceneUnorderedMap<Primitive*>& getPrimitives();
	Primitive* getPrimitiveByID(size_t id);
	size_t getPrimitiveCount() const;


	Light* getLightByID(size_t id);
	SceneUnorderedMap<Light*>& getLights();


	std::shared_ptr<Texture> getTexture(std::string_view name);
	void addTexture(std::shared_ptr<Texture> texture);

	std::shared_ptr<Material> getMaterial(std::string_view name);
	void addMaterial(std::shared_ptr<Material> material);
	std::vector<std::string> getMaterialNames() const;

	SceneNode* getActiveNode() const;
	int32_t getActiveNodeID() const;
	void setActiveNode(SceneNode* node, bool addToSelection = false);
	void deselectNode(SceneNode* node);
	void clearSelectedNodes();
	bool isNodeSelected(SceneNode* node) const;
	const std::unordered_set<SceneNode*>& getAllSelectedNodes() const { return m_selectedNodes; }

	bool areLightsDirty() const;
	void setLightsDirty(bool dirty = true);

	void setEnvironmentMap(const std::string& path);
	const std::string& getEnvironmentMapPath() const;
	bool isEnvironmentMapDirty() const;
	void clearEnvironmentMapDirty();

	void setActiveCamera(Camera* camera);
	Camera* getActiveCamera() const;

	void deleteNode(SceneNode* node);
	SceneNode* adoptClonedNode(
		std::unique_ptr<SceneNode>&& clonedNode,
		SceneNodeHandle preferredHandle = SceneNodeHandle::invalidHandle(),
		bool shouldValidateName = true);

	void setReadBackID(float readBackID);
	float getReadBackID();

	void validateName(SceneNode* node);
	void updateState();

	void importModel(const std::string& filepath);
	std::shared_ptr<ImportProgress> getImportProgress() const noexcept;

	void saveScene(std::string_view filepath);
	void loadScene(std::string_view filepath);
	void clearScene();

private:
	void updateAsyncImport() noexcept; // called each frame
	void processPendingBakes();
	void checkTextureUpdates();
	void updateAsyncPendingTextureReloads() noexcept; 
	void addLight(Light* light);
	void addPrimitive(Primitive* primitive);
	void addCamera(Camera* camera);
	void addBaker(Baker* baker);
	void addBakerNode(BakerNode* node);
	float m_readBackID;
	SceneUnorderedMap<Primitive*> m_primitives;
	SceneUnorderedMap<Light*> m_lights;
	SceneUnorderedMap<Camera*> m_cameras;
	SceneUnorderedMap<Baker*> m_bakers;
	SceneUnorderedMap<BakerNode*> m_bakerNodes;
	StringUnorderedMap<std::shared_ptr<Texture>> m_textures;   // path + actual texture
	StringUnorderedMap<std::shared_ptr<Material>> m_materials; // path + actual material
	StringUnorderedMap<uint32_t> m_nodeNames;


	bool m_isImporting = false;
	std::future<AsyncImportResult> m_importFuture;
	std::shared_ptr<ImportProgress> m_importProgress;
	ComPtr<ID3D11Device> m_device;
	std::vector<PendingTextureReload> m_pendingTextureReloads;

	std::unordered_set<SceneNode*> m_selectedNodes;

	Camera* m_activeCamera = nullptr;
	SceneNode* m_activeNode = nullptr;

	bool m_lightsAreDirty = false;

	std::string m_environmentMapPath;
	bool m_environmentMapDirty = false;
};
