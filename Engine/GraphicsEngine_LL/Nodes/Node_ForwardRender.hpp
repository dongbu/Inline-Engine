#pragma once

#include "../GraphicsNode.hpp"
#include "../Scene.hpp"
#include "../BasicCamera.hpp"
#include "../Mesh.hpp"
#include "../Material.hpp"
#include "../ConstBufferHeap.hpp"
#include "../PipelineTypes.hpp"
#include "GraphicsApi_LL/IPipelineState.hpp"
#include "GraphicsApi_LL/IGxapiManager.hpp"

#include <optional>

namespace inl::gxeng::nodes {

/// <summary>
/// Inputs: target, depth stencil, entities, camera, directional lights, shadow map, shadowMX, csmSplits, lightMVP
/// </summary>
class ForwardRender :
	virtual public GraphicsNode,
	virtual public GraphicsTask,
	
	virtual public exc::InputPortConfig<
		Texture2D,
		Texture2D,
		const EntityCollection<MeshEntity>*,
		const BasicCamera*,
		const EntityCollection<DirectionalLight>*,
		Texture2D,
		Texture2D,
		Texture2D,
		Texture2D>,
	virtual public exc::OutputPortConfig<Texture2D>
{
private:
	struct ScenarioDesc {
		Mesh::Layout layout;
		std::string shader;
	};
	struct ScenarioData {
		std::unique_ptr<gxapi::IPipelineState> pso;
		gxapi::eFormat renderTargetFormat = gxapi::eFormat::UNKNOWN;
		gxapi::eFormat depthStencilFormat = gxapi::eFormat::UNKNOWN;
		Binder binder;
		std::vector<int> offsets;
		size_t constantsSize;
	};
	struct VsConstants {
		mathfu::VectorPacked<float, 4> mvp[4];
		mathfu::VectorPacked<float, 4> mv[4];
		mathfu::VectorPacked<float, 4> m[4];
		mathfu::VectorPacked<float, 4> v[4];
		mathfu::VectorPacked<float, 4> p[4];
	};	
	struct LightConstants {
		alignas(16) mathfu::VectorPacked<float, 3> direction;
		alignas(16) mathfu::VectorPacked<float, 3> color;
	};

public:
	ForwardRender();

	void Update() override {}
	void Notify(exc::InputPortBase* sender) override {}

	void Initialize(EngineContext& context) override;
	void Reset() override;
	void Setup(SetupContext& context) override;
	void Execute(RenderContext& context) override;

private:
	static std::string GenerateVertexShader(const Mesh::Layout& layout);
	static std::string GeneratePixelShader(const MaterialShader& shader);
	Binder GenerateBinder(RenderContext& context, const std::vector<MaterialShaderParameter>& mtlParams, std::vector<int>& offsets, size_t& materialCbSize);
	std::unique_ptr<gxapi::IPipelineState> CreatePso(
		RenderContext& context,
		Binder& binder,
		ShaderStage& vs,
		ShaderStage& ps,
		gxapi::eFormat renderTargetFormat,
		gxapi::eFormat depthStencilFormat);

	ScenarioData& GetScenario(
		RenderContext& context,
		const Mesh::Layout& layout,
		const MaterialShader& shader,
		gxapi::eFormat renderTargetFormat,
		gxapi::eFormat depthStencilFormat);

protected:
	std::optional<Binder> m_binder;
	BindParameter m_transformBindParam;
	BindParameter m_sunBindParam;
	BindParameter m_albedoBindParam;
	BindParameter m_shadowMapBindParam;
	BindParameter m_shadowMXBindParam;
	BindParameter m_csmSplitsBindParam;

private:
	RenderTargetView2D m_rtv;
	DepthStencilView2D m_dsv;
	const EntityCollection<MeshEntity>* m_entities;
	const BasicCamera* m_camera;
	const EntityCollection<DirectionalLight>* m_directionalLights;

	TextureView2D m_shadowMapTexView;
	TextureView2D m_shadowMXTexView;
	TextureView2D m_csmSplitsTexView;
	TextureView2D m_lightMVPTexView;

private:
	struct ElementHash {
		size_t operator()(const Mesh::Layout& obj) const { return obj.GetElementHash(); }
		size_t operator()(const Mesh::Layout& lhs, const Mesh::Layout& rhs) const { return lhs.EqualElements(rhs); }
	};
	struct ScenarioHash {
		size_t operator()(const ScenarioDesc& obj) const { return obj.layout.GetLayoutHash() ^ std::hash<std::string>()(obj.shader); }
		size_t operator()(const ScenarioDesc& lhs, const ScenarioDesc& rhs) const { 
			return lhs.layout.EqualLayout(rhs.layout) && lhs.shader == rhs.shader;
		}
	};
	std::unordered_map<std::string, ShaderProgram> m_materialShaders; // maps MaterialShader codes to pixel shaders
	std::unordered_map<Mesh::Layout, ShaderProgram, ElementHash, ElementHash> m_vertexShaders; // maps Mesh layouts to vertex shaders
	std::unordered_map<ScenarioDesc, ScenarioData, ScenarioHash, ScenarioHash> m_scenarios; // maps mesh-mtlshader pairs to PSOs
};

} // namespace inl::gxeng::nodes
