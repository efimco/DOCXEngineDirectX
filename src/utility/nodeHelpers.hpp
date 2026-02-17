#pragma once

#include <memory>
#include <json.hpp>

#include <d3d11_4.h>
#include <wrl.h>

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

struct Transform;
class Scene;
class SceneNode;
using Json = nlohmann::json;

using namespace Microsoft::WRL;

namespace Nodes
{

	struct LoadContext
	{
		std::string directory;
		ComPtr<ID3D11Device> device;
		ComPtr<ID3D11DeviceContext> context;
		Scene* scene = nullptr;
	};

	struct SaveContext
	{
		std::string directory;
		SceneNode* node = nullptr;
	};

	void saveVec2(Json& json, std::string_view name, const glm::vec2& vec);
	void saveVec3(Json& json, std::string_view name, const glm::vec3& vec);
	void saveXForm(Json& json, std::string_view name, const Transform& transform);

	void loadVec2(Json& json, std::string_view name, glm::vec2& vec);
	void loadVec3(Json& json, std::string_view name, glm::vec3& vec);
	void loadXForm(Json& json, std::string_view name, Transform& vec);

	std::unique_ptr<SceneNode> makeDynNode(LoadContext& context, std::string_view type);

	std::unique_ptr<SceneNode> loadNodeFromJson(LoadContext& context, Json& json);

	void saveNodeToJson(SaveContext& context, Json& json);

}
