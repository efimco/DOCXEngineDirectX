#pragma once

#include "sceneNodeHandle.hpp"
#include "snapshotBase.hpp"
#include "textureHistory.hpp"

class BakerPass;
class SceneNode;
class Scene;

namespace BKRCommand
{

	// This command would apply provided TextureDelta on exec()
	class BlendMaskApplyDeltaCommand final : public CommandBase
	{
	public:
		BlendMaskApplyDeltaCommand(
			std::shared_ptr<TextureHistory> textureHistory,
			std::shared_ptr<TextureDelta> textureDelta,
			std::shared_ptr<BakerPass> bakerPass);

	protected:
		virtual std::unique_ptr<CommandBase> exec() override;

		std::shared_ptr<TextureHistory> m_textureHistory;
		std::shared_ptr<TextureDelta> m_textureDelta;
		std::shared_ptr<BakerPass> m_bakerPass = nullptr;
	};

	// This command would create a delta for a blend mask in normal map baker,
	// undo operation on this command would restore texture snapshot.
	class BlendMaskCreateDeltaCommand final : public CommandBase
	{
	public:
		BlendMaskCreateDeltaCommand(
			std::shared_ptr<TextureHistory> textureHistory,
			std::shared_ptr<TextureSnapshot> textureSnapshot,
			std::shared_ptr<BakerPass> bakerPass);

	protected:
		virtual std::unique_ptr<CommandBase> exec() override;

		std::shared_ptr<TextureHistory> m_textureHistory;
		std::shared_ptr<TextureSnapshot> m_textureSnapshot;
		std::shared_ptr<BakerPass> m_bakerPass;
	};

	class ToggleSmoothNormalsCommand final : public CommandBase
	{
	public:
		ToggleSmoothNormalsCommand(
			Scene* inScene,
			SceneNode* inSceneNode,
			bool newValue);

	protected:
		std::unique_ptr<CommandBase> exec() override;

		Scene* m_scene = nullptr;
		SceneNodeHandle m_nodeHandle;
		bool m_newValue = false;
	};

	class SelectOutputCommand final : public CommandBase
	{
	public:
		SelectOutputCommand(
			Scene* inScene,
			std::shared_ptr<BakerPass> bakerPass,
			std::string_view filename,
			std::string_view directory);

	protected:
		std::unique_ptr<CommandBase> exec() override;

		Scene* m_scene = nullptr;
		std::shared_ptr<BakerPass> m_bakerPass;
		std::string m_filename;
		std::string m_directory;
	};

}
