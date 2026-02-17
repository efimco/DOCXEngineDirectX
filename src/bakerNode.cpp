#include "bakerNode.hpp"
#include <iostream>

#include "material.hpp"
#include "primitive.hpp"

#include "passes/bakerPass.hpp"

std::string_view BakerNode::getNodeTypename() const
{
	return "BakerNode";
}

LowPolyNode::LowPolyNode(const std::string_view nodeName)
{
	movable = false;
	name = nodeName;
	deletable = false;
}

HighPolyNode::HighPolyNode(const std::string_view nodeName)
{
	movable = false;
	name = nodeName;
	deletable = false;
}

std::unique_ptr<SceneNode> LowPolyNode::clone() const
{
	auto newNode = std::make_unique<LowPolyNode>(this->name);
	newNode->copyFrom(*this);
	return newNode;
}

std::string_view LowPolyNode::getNodeTypename() const
{
	return "LowPolyNode";
}

std::unique_ptr<SceneNode> HighPolyNode::clone() const
{
	auto newNode = std::make_unique<HighPolyNode>(this->name);
	newNode->copyFrom(*this);
	return newNode;
}

std::string_view HighPolyNode::getNodeTypename() const
{
	return "HighPolyNode";
}

Baker::Baker(
	const std::string_view nodeName,
	ComPtr<ID3D11Device> device,
	ComPtr<ID3D11DeviceContext> context,
	Scene* scene)
{
	name = nodeName;
	movable = false;
	canBecomeParent = false;
	m_device = device;
	m_context = context;
	lowPoly = std::make_unique<LowPolyNode>("LowPolyContainer");
	highPoly = std::make_unique<HighPolyNode>("HighPolyContainer");
	textureWidth = 1024;
	cageOffset = 0.1f;
	useSmoothedNormals = 0;
	m_scene = scene;
}

void Baker::bake()
{
	for (const auto& [materialName, bakerPass] : m_materialsBakerPasses)
	{
		std::cout << "Baking material: " << materialName << std::endl;
		bakerPass->bake(textureWidth, textureWidth, cageOffset, useSmoothedNormals);
	}
}

void Baker::requestBake()
{
	m_pendingBake = true;
}

void Baker::processPendingBake()
{
	updateState();
	if (m_pendingBake)
	{
		bake();
		m_pendingBake = false;
	}
	for (const auto& [materialName, bakerPass] : m_materialsBakerPasses)
	{
		if (bakerPass->needsRebake)
		{
			std::cout << "Baking material: " << materialName << std::endl;
			bakerPass->bake(textureWidth, textureWidth, cageOffset, useSmoothedNormals);
			bakerPass->needsRebake = false;
		}
		else
			continue;
	}
}

void Baker::copyFrom(const SceneNode& node)
{
	name = node.name;
	transform = node.transform;
	if (const auto bakerNode = dynamic_cast<const Baker*>(&node))
	{
		textureWidth = bakerNode->textureWidth;
		cageOffset = bakerNode->cageOffset;
		useSmoothedNormals = bakerNode->useSmoothedNormals;
		lowPoly = std::unique_ptr<LowPolyNode>(static_cast<LowPolyNode*>(bakerNode->lowPoly->clone().release()));
		highPoly = std::unique_ptr<HighPolyNode>(static_cast<HighPolyNode*>(bakerNode->highPoly->clone().release()));
		m_materialsToBake = bakerNode->m_materialsToBake;
		m_materialsPrimitivesMap = bakerNode->m_materialsPrimitivesMap;
		m_materialsBakerPasses.clear();
	}
}

bool Baker::differsFrom(const SceneNode& node) const
{
	if (!SceneNode::differsFrom(node))
	{
		if (const auto baker = dynamic_cast<const Baker*>(&node))
		{
			bool lowPolyDiffers = lowPoly->differsFrom(*baker->lowPoly);
			bool highPolyDiffers = highPoly->differsFrom(*baker->highPoly);
			bool textureWidthDiffers = textureWidth != baker->textureWidth;
			bool cageOffsetDiffers = cageOffset != baker->cageOffset;
			bool useSmoothedNormalsDiffers = useSmoothedNormals != baker->useSmoothedNormals;
			bool materialsToBakeDiffers = m_materialsToBake != baker->m_materialsToBake;
			bool materialsPrimitivesMapDiffers = m_materialsPrimitivesMap != baker->m_materialsPrimitivesMap;
			bool materialsBakerPassesDiffers = m_materialsBakerPasses.size() != baker->m_materialsBakerPasses.size();

			return lowPolyDiffers || highPolyDiffers || textureWidthDiffers
				|| cageOffsetDiffers || useSmoothedNormalsDiffers || materialsToBakeDiffers
				|| materialsPrimitivesMapDiffers || materialsBakerPassesDiffers;
		}
	}
	return true;
}

std::unique_ptr<SceneNode> Baker::clone() const
{
	std::unique_ptr<Baker> baker = std::make_unique<Baker>(
		name,
		m_device,
		m_context,
		m_scene);
	baker->copyFrom(*this);
	return baker;
}


std::vector<std::shared_ptr<BakerPass>> Baker::getPasses()
{
	std::vector<std::shared_ptr<BakerPass>> passes;
	for (const auto& [materialName, bakerPass] : m_materialsBakerPasses)
	{
		passes.push_back(bakerPass);
	}
	return passes;
}


void Baker::collectMaterialsToBake()
{
	for (const auto& child : lowPoly->children)
	{
		const auto prim = dynamic_cast<Primitive*>(child.get());
		if (!prim)
			continue;
		if (!prim->material)
			continue;
		if (prim->material->name.empty())
			continue;
		if (std::ranges::find_if(m_materialsToBake, [&](const std::shared_ptr<Material>& mat)
			{ return mat->name == prim->material->name; }) == m_materialsToBake.end())
		{
			m_materialsToBake.push_back(prim->material);
		}
	}
}

void Baker::collectPrimitivesToBake()
{
	for (const auto& material : m_materialsToBake)
	{
		std::pair<std::vector<Primitive*>, std::vector<Primitive*>> lowHighPrimsPair;

		for (const auto& child : lowPoly->children)
		{
			const auto lowPolyPrim = dynamic_cast<Primitive*>(child.get());
			if (!lowPolyPrim || !lowPolyPrim->material || lowPolyPrim->material->name != material->name)
				continue;
			lowHighPrimsPair.first.push_back(lowPolyPrim);
		}
		for (const auto& highPolyChild : highPoly->children)
		{
			const auto highPolyPrim = dynamic_cast<Primitive*>(highPolyChild.get());
			if (!highPolyPrim)
				continue;
			lowHighPrimsPair.second.push_back(highPolyPrim);
		}
		m_materialsPrimitivesMap[material->name] = std::move(lowHighPrimsPair);
	}
}

void Baker::createOrUpdateBakerPasses()
{
	for (const auto& material : m_materialsToBake)
	{
		if (m_materialsBakerPasses.contains(material->name) &&
			m_materialsPrimitivesMap[material->name] == m_materialsBakerPasses[material->name]->getPrimitivesToBake())
			continue;
		if (m_materialsPrimitivesMap[material->name].first.empty())
		{
			m_materialsBakerPasses.erase(material->name);
			continue;
		}
		if (m_materialsBakerPasses[material->name] == nullptr)
		{
			auto bakerPass = std::make_unique<BakerPass>(
				m_device,
				m_context,
				m_scene);
			bakerPass->name = "BKR for " + material->name;
			m_materialsBakerPasses[material->name] = std::move(bakerPass);
		}
		m_materialsBakerPasses[material->name]->setPrimitivesToBake(m_materialsPrimitivesMap[material->name]);
	}

}

std::string_view Baker::getNodeTypename() const
{
	return "Baker";
}

void Baker::updateState()
{
	collectMaterialsToBake();
	collectPrimitivesToBake();
	createOrUpdateBakerPasses();
}

