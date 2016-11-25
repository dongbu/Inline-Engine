#pragma once

//#include "ResourceView.hpp"

#include "../GraphicsApi_LL/IGraphicsApi.hpp"
#include "../GraphicsApi_LL/IResource.hpp"

#include <functional>

namespace inl {
namespace gxeng {


class MemoryManager;
class DescriptorReference;
class RenderTargetView;

//==================================

struct MemoryObjDesc {
	using Deleter = std::function<void(gxapi::IResource*)>;
	using UniqPtr = std::unique_ptr<gxapi::IResource, Deleter>;

	MemoryObjDesc() = default;
	MemoryObjDesc(gxapi::IResource* ptr, bool resident = true);

	UniqPtr resource;
	bool resident;
};


class MemoryObject {
public:
	using Deleter = MemoryObjDesc::Deleter;

public:
	static bool PtrLess(const MemoryObject& lhs, const MemoryObject& rhs);
	static bool PtrGreater(const MemoryObject& lhs, const MemoryObject& rhs);
	static bool PtrEqual(const MemoryObject& lhs, const MemoryObject& rhs);

public:
	explicit MemoryObject(nullptr_t);
	explicit MemoryObject(MemoryObjDesc&& desc);
	virtual ~MemoryObject() {}

	MemoryObject(const MemoryObject&) = default;
	MemoryObject(MemoryObject&&) = default;

	MemoryObject& operator=(const MemoryObject&) = default;
	MemoryObject& operator=(MemoryObject&&) = default;

	/// <summary> Returns true if and only if operands are referring to the same resource.
	/// Different MemoryObject instances are equal if one is the copy of the other. </summary>
	bool operator==(const MemoryObject&) const;

	void* GetVirtualAddress() const;
	gxapi::ResourceDesc GetDescription() const;

	/// <summary> Records the current state of the resource. Does not change resource state, only used for tracking it. </summary>
	void RecordState(unsigned subresource, gxapi::eResourceState newState);
	/// <summary> Records the state of all subresources. Does not change resource state, only used for tracking it. </summary>
	void RecordState(gxapi::eResourceState newState);
	/// <summary> Returns the current tracked state. </summary>
	gxapi::eResourceState ReadState(unsigned subresource) const;

	void _SetResident(bool value) noexcept;
	bool _GetResident() const noexcept;

	gxapi::IResource* _GetResourcePtr() noexcept;
	const gxapi::IResource* _GetResourcePtr() const noexcept;

protected:
	void InitResourceStates(gxapi::eResourceState initialState);

private:
	struct Contents {
		std::unique_ptr<gxapi::IResource, Deleter> resource;
		bool resident;

		std::vector<gxapi::eResourceState> subresourceStates;
	};

private:
	std::shared_ptr<Contents> m_contents;
};

//==================================


//==================================
// Vertex buffer, index buffer

class LinearBuffer : public MemoryObject {
public:
	uint64_t GetSize() const;
	using MemoryObject::MemoryObject;
};


class VertexBuffer : public LinearBuffer {
public:
	using LinearBuffer::LinearBuffer;
};


class IndexBuffer : public LinearBuffer {
public:
	explicit IndexBuffer(nullptr_t);
	IndexBuffer(MemoryObjDesc&& desc, size_t indexCount);

	size_t GetIndexCount() const;

protected:
	size_t m_indexCount;
};

//==================================


//==================================
// Const Buffers

class ConstBuffer : public LinearBuffer {
public:
	void* GetVirtualAddress() const;
	uint64_t GetSize() const;

protected:
	ConstBuffer(MemoryObjDesc&& desc, void* gpuVirtualPtr, uint32_t dataSize);

protected:
	void* m_gpuVirtualPtr;
	uint32_t m_dataSize;
};


class VolatileConstBuffer : public ConstBuffer {
public:
	VolatileConstBuffer(MemoryObjDesc&& desc, void* gpuVirtualPtr, uint32_t dataSize);
};


class PersistentConstBuffer : public ConstBuffer {
public:
	PersistentConstBuffer(MemoryObjDesc&& desc, void* gpuVirtualPtr, uint32_t dataSize);
};

//==================================


//==================================
// Textures

class Texture1D : public MemoryObject {
public:
	using MemoryObject::MemoryObject;

	uint64_t GetWidth() const;
	uint16_t GetArrayCount() const;
	gxapi::eFormat GetFormat() const;
};


class Texture2D : public MemoryObject {
public:
	using MemoryObject::MemoryObject;

	uint64_t GetWidth() const;
	uint64_t GetHeight() const;
	uint16_t GetArrayCount() const;
	uint32_t GetSubresourceIndex(uint32_t arrayIndex, uint32_t mipLevel) const;
	gxapi::eFormat GetFormat() const;
};


class Texture3D : public MemoryObject {
public:
	using MemoryObject::MemoryObject;

	uint64_t GetWidth() const;
	uint64_t GetHeight() const;
	uint16_t GetDepth() const;
	gxapi::eFormat GetFormat() const;
};


class TextureCube : public MemoryObject {
public:
	using MemoryObject::MemoryObject;

	uint64_t GetWidth() const;
	uint64_t GetHeight() const;
	gxapi::eFormat GetFormat() const;
};


//==================================

} // namespace gxeng
} // namespace inl