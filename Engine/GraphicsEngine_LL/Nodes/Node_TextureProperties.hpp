#pragma once

#include "../GraphicsNode.hpp"
#include "../ResourceView.hpp"
#include "../PipelineTypes.hpp"

#include <cmath>


namespace inl::gxeng::nodes {

/// <summary>
/// Gets the parameters of a specific texture.
/// Input: the texture in question.
/// Outputs: width, height, format, texture array element count
/// </summary>
class TextureProperties :
	virtual public GraphicsNode,
	public GraphicsTask,
	public InputPortConfig<gxeng::Texture2D>,
	public OutputPortConfig<unsigned, unsigned, gxapi::eFormat, uint16_t>
{
public:
	static const char* Info_GetName() { return "TextureProperties"; }
	virtual void Update() override {}

	virtual void Notify(InputPortBase* sender) override {}

	virtual void Initialize(EngineContext& context) override {
		GraphicsNode::SetTaskSingle(this);
	}
	void Reset() override {
		GetInput(0)->Clear();
	}

	void Setup(SetupContext& context) override {
		const auto& texture = GetInput<0>().Get();

		GetOutput<0>().Set(texture.GetWidth());
		GetOutput<1>().Set(texture.GetHeight());
		GetOutput<2>().Set(texture.GetFormat());
		GetOutput<3>().Set(texture.GetArrayCount());
	}

	void Execute(RenderContext& context) override {}
};


} // namespace inl::gxeng::nodes
