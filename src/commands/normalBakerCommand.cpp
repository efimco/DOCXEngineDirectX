#include "normalBakerCommand.hpp"

#include "bakerNode.hpp"
#include "bakerPass.hpp"
#include "scene.hpp"
#include "sceneNode.hpp"

namespace BKRCommand
{
	BlendMaskApplyDeltaCommand::BlendMaskApplyDeltaCommand(
		std::shared_ptr<TextureHistory> textureHistory,
		std::shared_ptr<TextureDelta> textureDelta,
		std::shared_ptr<BakerPass> bakerPass)
		: m_textureHistory(textureHistory)
		, m_textureDelta(textureDelta)
		, m_bakerPass(bakerPass)
	{
	}

	std::unique_ptr<CommandBase> BlendMaskApplyDeltaCommand::exec()
	{
		// Create a new delta map from the previous list of indices
		auto textureDelta = m_textureHistory->createDelta(
			m_bakerPass->getBlendTexture(),
			m_textureDelta->m_tileIndices);
		m_textureHistory->applyDelta(
			m_bakerPass->getBlendTexture(),
			m_textureDelta);
		m_bakerPass->needsRebake = true;
		return std::make_unique<BlendMaskApplyDeltaCommand>(m_textureHistory, textureDelta, m_bakerPass);
	}

	BlendMaskCreateDeltaCommand::BlendMaskCreateDeltaCommand(
		std::shared_ptr<TextureHistory> textureHistory,
		std::shared_ptr<TextureSnapshot> textureSnapshot,
		std::shared_ptr<BakerPass> bakerPass)
		: m_textureHistory(textureHistory)
		, m_textureSnapshot(textureSnapshot)
		, m_bakerPass(bakerPass)
	{
	}

	std::unique_ptr<CommandBase> BlendMaskCreateDeltaCommand::exec()
	{
		auto textureDelta = m_textureHistory->createDelta(
			m_textureSnapshot,
			m_bakerPass->getBlendTexture(),
			m_bakerPass->getBlendTextureSRV());
		return std::make_unique<BlendMaskApplyDeltaCommand>(m_textureHistory, textureDelta, m_bakerPass);
	}

	ToggleSmoothNormalsCommand::ToggleSmoothNormalsCommand(
		Scene* inScene,
		SceneNode* inSceneNode,
		bool newValue)
		: m_scene(inScene)
		, m_nodeHandle(inSceneNode->getNodeHandle())
		, m_newValue(newValue)
	{
	}

	std::unique_ptr<CommandBase> ToggleSmoothNormalsCommand::exec()
	{
		SceneNode* sceneNode = m_scene->getNodeByHandle(m_nodeHandle);
		assert(sceneNode);

		Baker* bakerNode = dynamic_cast<Baker*>(sceneNode);
		assert(bakerNode);

		bakerNode->useSmoothedNormals = m_newValue;
		bakerNode->requestBake();
		return std::make_unique<ToggleSmoothNormalsCommand>(m_scene, sceneNode, !m_newValue);
	}

	SelectOutputCommand::SelectOutputCommand(
		Scene* inScene,
		std::shared_ptr<BakerPass> bakerPass,
		std::string_view filename,
		std::string_view directory)
		: m_scene(inScene)
		, m_bakerPass(bakerPass)
		, m_filename(filename)
		, m_directory(directory)
	{
		m_breakHistory = true;
	}

	std::unique_ptr<CommandBase> SelectOutputCommand::exec()
	{
		std::string oldFilename = m_bakerPass->filename;
		std::string oldDirectory = m_bakerPass->directory;
		m_bakerPass->filename = m_filename;
		m_bakerPass->directory = m_directory;
		return std::make_unique<SelectOutputCommand>(m_scene, m_bakerPass, oldFilename, oldDirectory);
	}
}
