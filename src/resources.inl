#include "resources.h"
template <typename T, typename... Args>
inline std::shared_ptr<T> trayser::Resources::Create(const std::string& hashable, Args&&... args)
{
	ResourceHandle id = std::hash<std::string>()(hashable);

	std::shared_ptr<T> resource = Get<T>(id);
	if (resource) return resource;

	resource = std::make_shared<T>(std::forward<Args>(args)...);
	m_resources[id] = resource;

	return resource;
}

template <typename T>
inline std::shared_ptr<T> trayser::Resources::Get(ResourceHandle id)
{
	auto it = m_resources.find(id);
	if (it != m_resources.end()) return std::static_pointer_cast<T>(it->second);
	return std::shared_ptr<T>();
}

template<typename T>
inline std::shared_ptr<T> trayser::Resources::Get(const std::string& hashable)
{
	return Get<T>(std::hash<std::string>()(hashable));
}

template<typename T, size_t Capacity>
inline void trayser::ResourcePool<T, Capacity>::Init()
{
	m_freeSpots.resize(Capacity);
	std::iota(m_freeSpots.begin(), m_freeSpots.end(), 0);
	m_handleToHash.resize(Capacity, 0);
}

template<typename T, size_t Capacity>
inline void trayser::ResourcePool<T, Capacity>::Destroy()
{
	for (int i = 0; i < m_resources.size(); i++)
	{
		if (m_takenSpots[i])
		{
			ResourceHandle handle = i;
			Free(handle);
		}
	}
}

template<typename T, size_t Capacity>
template<typename ...Args>
inline trayser::ResourceHandle trayser::ResourcePool<T, Capacity>::Create(const std::string& hashable, Args&& ...args)
{
	Hash hash = std::hash<std::string>()(hashable);

	auto it = m_hashToHandle.find(hash);
	if (it != m_hashToHandle.end())
		return it->second;

	if (m_freeSpots.empty())
		return ResourceHandle_Invalid;

	ResourceHandle handle = m_freeSpots.back();
	m_freeSpots.pop_back();

	// Reconstruct element using the placement new operator
	new (&m_resources[handle]) T(std::forward<Args>(args)...);

	m_handleToHash[handle] = hash;
	m_takenSpots[handle] = true;
	m_hashToHandle[hash] = handle;

	return handle;
}

template<typename T, size_t Capacity>
inline const T& trayser::ResourcePool<T, Capacity>::Get(ResourceHandle handle) const
{
	return m_resources[handle];
}

template<typename T, size_t Capacity>
inline void trayser::ResourcePool<T, Capacity>::Free(const std::string& hashable)
{
	Hash hash = std::hash<std::string>()(hashable);

	// If the resource is not created, cancel out
	auto it = m_hashToHandle.find(hash);
	if (it == m_hashToHandle.end())
		return;

	ResourceHandle handle = m_hashToHandle[hash];
	m_hashToHandle.erase(hash);

	m_resources[handle].~T();
	m_freeSpots.push_back(handle);
	m_takenSpots[handle] = false;
}

template<typename T, size_t Capacity>
inline void trayser::ResourcePool<T, Capacity>::Free(ResourceHandle& handle)
{
	if (handle == ResourceHandle_Invalid)
		return;

	m_hashToHandle.erase(m_handleToHash[handle]);
	m_freeSpots.push_back(handle);
	m_resources[handle].~T();
	m_takenSpots[handle] = false;

	handle = ResourceHandle_Invalid;
}