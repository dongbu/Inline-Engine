#pragma once

#include "../GraphicsNode.hpp"

#include "../Scene.hpp"
#include "../DirectionalLight.hpp"

namespace inl::gxeng::nodes {


/// <summary>
/// Get reference to a Scene identified by its name.
/// Inputs: name of the scene.
/// Outputs: list of mesh entities, overlay entities and directional lights.
/// </summary>
/// <remarks>
/// Throws an exception if the scene cannot be found, never returns nulls.
/// </remarks>
class GetSceneByName :
	virtual public GraphicsNode,
	virtual public GraphicsTask,
	virtual public exc::InputPortConfig<std::string>,
	virtual public exc::OutputPortConfig<const EntityCollection<MeshEntity>*, const EntityCollection<OverlayEntity>*, const EntityCollection<DirectionalLight>*>
{
public:
	GetSceneByName() {}

	void Update() override {}

	void Notify(exc::InputPortBase* sender) override {}

	void Initialize(EngineContext& context) override {
		GraphicsNode::SetTaskSingle(this);
	}

	void Setup(SetupContext& context) {
		// read scene name from input port
		std::string sceneName = this->GetInput<0>().Get();

		// look for specified camera
		const Scene* match = nullptr;
		for (auto scene : m_sceneList) {
			if (scene->GetName() == sceneName) {
				match = scene;
			}
		}

		// throw an error if scene is not found
		if (match == nullptr) {
			throw std::invalid_argument("[GetSceneByName] The scene called \"" + sceneName + "\" does not exist.");
		}

		// set scene parameters to output ports
		this->GetOutput<0>().Set(&match->GetMeshEntities());
		this->GetOutput<1>().Set(&match->GetOverlayEntities());
		this->GetOutput<2>().Set(&match->GetDirectionalLights());
	}

	void Execute(RenderContext& context) {}


	void SetSceneList(std::vector<const Scene*> scenes) {
		m_sceneList = std::move(scenes);
	}
	const std::vector<const Scene*>& GetSceneList() const {
		return m_sceneList;
	}
private:
	std::vector<const Scene*> m_sceneList;
};


} // namespace inl::gxeng::nodes
