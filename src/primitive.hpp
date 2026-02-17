#pragma once

#include <d3d11.h>
#include <d3d11shader.h>
#include <vector>

#include <d3d11_4.h>
#include <wrl.h>

#include "bvhNode.hpp"
#include "sceneNode.hpp"

using namespace Microsoft::WRL;

struct Material;
struct Triangle;
struct Vertex;

struct SharedPrimitiveData
{
	std::vector<Vertex> vertexData;
	std::vector<uint32_t> indexData;
	std::vector<Triangle> triangles;
	std::vector<uint32_t> triangleIndices;
	std::vector<BVH::Node> bvhNodes;

	ComPtr<ID3D11Buffer> indexBuffer;
	ComPtr<ID3D11Buffer> vertexBuffer;

	ComPtr<ID3D11Buffer> structuredTrisBuffer;
	ComPtr<ID3D11Buffer> structuredTrisIndicesBuffer;

	ComPtr<ID3D11ShaderResourceView> srv_structuredTrisBuffer;
	ComPtr<ID3D11ShaderResourceView> srv_structuredTrisIndicesBuffer;
};

enum class BasePrimitiveType
{
	EMPTY,
	CUBE,
	SPHERE,
	PLANE
};

class Primitive : public SceneNode
{
public:
	explicit Primitive(ComPtr<ID3D11Device> device, BasePrimitiveType type = BasePrimitiveType::EMPTY, std::string_view nodeName = "Primitive");
	~Primitive() override = default;
	Primitive(const Primitive&) = delete;
	Primitive(Primitive&&) = default;
	Primitive& operator=(const Primitive&) = delete;
	Primitive& operator=(Primitive&&) = delete;

	// Generate basic primitive geometry
	static void GenerateCube(std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices, float size = 1.0f);
	static void GenerateSphere(std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices, float radius = 0.5f, uint32_t segments = 32, uint32_t rings = 16);
	static void GeneratePlane(std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices, float width = 1.0f, float height = 1.0f);

	void setVertexData(std::vector<Vertex>&& vertexData) const;
	void setIndexData(std::vector<uint32_t>&& indexData) const;
	void fillTriangles();

	BVH::BBox getWorldBBox();

	const std::vector<uint32_t>& getIndexData() const;
	const std::vector<Vertex>& getVertexData() const;
	ComPtr<ID3D11Buffer> getIndexBuffer() const;
	ComPtr<ID3D11Buffer> getVertexBuffer() const;
	ComPtr<ID3D11ShaderResourceView> getTrisBufferSRV() const;
	ComPtr<ID3D11ShaderResourceView> getTrisIndicesBufferSRV() const;

	const std::vector<Triangle>& getTriangles() const;
	const std::vector<uint32_t>& getTriangleIndices() const;
	std::vector<BVH::Node>& getBVHNodes() const;
	std::vector<BVH::Node> getWorldSpaceBVHNodes();

	void copyFrom(const SceneNode& node) override;
	bool differsFrom(const SceneNode& node) const override;
	std::unique_ptr<SceneNode> clone() const override;

	std::shared_ptr<Material> material;

private:
	void computeTangents();
	void computeSmoothNormals();
	void buildBVH();
	void createGPUBuffers();
	std::shared_ptr<SharedPrimitiveData> m_sharedData;
	ComPtr<ID3D11Device> m_device;

protected:
	virtual std::string_view getNodeTypename() const;
	virtual void load(Nodes::LoadContext& loadContext, nlohmann::json& json);
	virtual void save(Nodes::SaveContext& saveContext, nlohmann::json& json) const;

};
