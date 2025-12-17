#pragma once

#include <memory>
#include <unordered_map>
#include <numeric>

namespace trayser
{

using ResourceHandle = uint32_t;

enum
{
	ResourceHandle_Invalid = std::numeric_limits<uint32_t>::max(),
};

template <typename T, uint32_t Capacity>
class ResourcePool
{
public:

private:
	using Hash = uint32_t;

public:
	void Init();
	void Destroy();

	template <typename... Args>
	[[nodiscard]] ResourceHandle Create(const std::string& hashable, Args&&... args);

	[[nodiscard]] const T& Get(ResourceHandle handle) const;
	void Free(const std::string& hashable);
	void Free(ResourceHandle& handle);

public:
	std::array<T, Capacity>						m_resources;
	std::array<bool, Capacity>					m_takenSpots;

private:
	std::vector<ResourceHandle>					m_freeSpots;
	std::vector<Hash>							m_handleToHash;
	std::unordered_map<Hash, ResourceHandle>	m_hashToHandle;
};

class Resources
{
public:
	template <typename T, typename... Args>
	std::shared_ptr<T> Create(const std::string& path, Args&&... args);

	template <typename T>
	std::shared_ptr<T> Get(ResourceHandle id);

	template <typename T>
	std::shared_ptr<T> Get(const std::string& hashable);

private:
	std::unordered_map<ResourceHandle, std::shared_ptr<void>> m_resources;
};

} // namespace trayser

#include <resources.inl>