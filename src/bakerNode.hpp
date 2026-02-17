#pragma once

#include <d3d11_4.h>
#include <wrl.h>

#include "sceneNode.hpp"

using namespace Microsoft::WRL;

class BakerPass;
struct Material;
class Primitive;
class Scene;

class BakerNode : public SceneNode
{
public:
	explicit BakerNode() = default;
	~BakerNode() override = default;

protected:
	virtual std::string_view getNodeTypename() const;

};

class LowPolyNode : public BakerNode
{
public:
	explicit LowPolyNode(const std::string_view nodeName);
	~LowPolyNode() override = default;
	std::unique_ptr<SceneNode> clone() const override;

protected:
	virtual std::string_view getNodeTypename() const;

};

class HighPolyNode : public BakerNode
{
public:
	explicit HighPolyNode(const std::string_view nodeName);
	~HighPolyNode() override = default;
	std::unique_ptr<SceneNode> clone() const override;

protected:
	virtual std::string_view getNodeTypename() const;

};

class Baker : public BakerNode
{
public:
	explicit Baker(
		std::string_view nodeName,
		ComPtr<ID3D11Device> device,
		ComPtr<ID3D11DeviceContext> context,
		Scene* scene);
	~Baker() override = default;

	void bake();
	void requestBake();
	void processPendingBake();

	void copyFrom(const SceneNode& node) override;
	bool differsFrom(const SceneNode& node) const override;
	std::unique_ptr<SceneNode> clone() const override;

	std::vector<std::shared_ptr<BakerPass>> getPasses();

	std::unique_ptr<LowPolyNode> lowPoly;
	std::unique_ptr<HighPolyNode> highPoly;

	uint32_t textureWidth;
	float cageOffset;
	uint32_t useSmoothedNormals;

private:
	void updateState();
	void collectMaterialsToBake();
	void collectPrimitivesToBake();
	void createOrUpdateBakerPasses();
	bool m_pendingBake = false;

private:
	Scene* m_scene = nullptr;
	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;
	std::vector<std::shared_ptr<Material>> m_materialsToBake;
	std::unordered_map<std::string, std::pair<std::vector<Primitive*>, std::vector<Primitive*>>> m_materialsPrimitivesMap;
	std::unordered_map<std::string, std::shared_ptr<BakerPass>> m_materialsBakerPasses;

protected:
	virtual std::string_view getNodeTypename() const;

};
