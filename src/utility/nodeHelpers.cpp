#include "nodeHelpers.hpp"

#include "bakerNode.hpp"
#include "camera.hpp"
#include "light.hpp"
#include "primitive.hpp"
#include "sceneNode.hpp"

namespace Nodes
{
	void saveVec2(Json& json, std::string_view name, const glm::vec2& vec)
	{
		auto vecObj = nlohmann::json::object();
		vecObj.emplace("x", vec.x);
		vecObj.emplace("y", vec.y);
		json.emplace(name, vecObj);
	}

	void saveVec3(Json& json, std::string_view name, const glm::vec3& vec)
	{
		auto vecObj = nlohmann::json::object();
		vecObj.emplace("x", vec.x);
		vecObj.emplace("y", vec.y);
		vecObj.emplace("z", vec.z);
		json.emplace(name, vecObj);
	}

	void saveXForm(Json& json, std::string_view name, const Transform& transform)
	{
		auto transformObj = nlohmann::json::object();
		saveVec3(transformObj, "position", transform.position);
		saveVec3(transformObj, "rotation", transform.rotation);
		saveVec3(transformObj, "scale", transform.scale);
		json.emplace(name, transformObj);
	}

	void loadVec2(Json& json, std::string_view name, glm::vec2& vec)
	{
		auto& jsonObj = json.at(name.data());
		jsonObj.at("x").get_to(vec.x);
		jsonObj.at("y").get_to(vec.y);
	}

	void loadVec3(Json& json, std::string_view name, glm::vec3& vec)
	{
		auto& jsonObj = json.at(name.data());
		jsonObj.at("x").get_to(vec.x);
		jsonObj.at("y").get_to(vec.y);
		jsonObj.at("z").get_to(vec.z);
	}

	void loadXForm(Json& json, std::string_view name, Transform& vec)
	{
		auto& jsonObj = json.at(name.data());
		loadVec3(jsonObj, "position", vec.position);
		loadVec3(jsonObj, "rotation", vec.rotation);
		loadVec3(jsonObj, "scale", vec.scale);
	}

	std::unique_ptr<SceneNode> makeDynNode(LoadContext& context, std::string_view type)
	{
		if (type == "SceneNode")
		{
			return std::make_unique<SceneNode>();
		}
		if (type == "Camera")
		{
			return std::make_unique<Camera>(
				glm::vec3{});
		}
		if (type == "Light")
		{
			return std::make_unique<Light>(
				POINT_LIGHT,
				glm::vec3{});
		}
		if (type == "Primitive")
		{
			return std::make_unique<Primitive>(
				context.device);
		}
		if (type == "Baker")
		{
			return std::make_unique<Baker>(
				"Baker",
				context.device,
				context.context,
				context.scene);
		}
		if (type == "HighPolyNode")
		{
			return std::make_unique<HighPolyNode>(
				"HighPolyNode");
		}
		if (type == "LowPolyNode")
		{
			return std::make_unique<LowPolyNode>(
				"LowPolyNode");
		}
		return nullptr;
	}

	std::unique_ptr<SceneNode> loadNodeFromJson(LoadContext& context, Json& json)
	{
		std::string type = json.at("__type__").get<std::string>();
		std::unique_ptr<SceneNode> newNode = makeDynNode(context, type);
		if (newNode)
		{
			newNode->load(context, json);
		}
		return newNode;
	}

	void saveNodeToJson(SaveContext& context, Json& json)
	{
		context.node->save(context, json);
	}

}

