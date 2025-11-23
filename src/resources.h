#pragma once

#include <memory>
#include <unordered_map>

class Resources
{
public:
	using ResourceID = size_t;

public:
	template <typename T, typename... Args>
	std::shared_ptr<T> Create(const std::string& path, Args&&... args);

	template <typename T>
	std::shared_ptr<T> Get(ResourceID id);

	template <typename T>
	std::shared_ptr<T> Get(const std::string& hashable);

private:
	std::unordered_map<ResourceID, std::shared_ptr<void>> m_resources;
};

template <typename T, typename... Args>
inline std::shared_ptr<T> Resources::Create(const std::string& hashable, Args&&... args)
{	
	ResourceID id = std::hash<std::string>()(hashable);
	
	std::shared_ptr<T> resource = Get<T>(id);
	if (resource) return resource;

	resource = std::make_shared<T>(std::forward<Args>(args)...);
	m_resources[id] = resource;

	return resource;
}

template <typename T>
inline std::shared_ptr<T> Resources::Get(ResourceID id)
{
	auto it = m_resources.find(id);
	if (it != m_resources.end()) return std::static_pointer_cast<T>(it->second);
	return std::shared_ptr<T>();
}

template<typename T>
inline std::shared_ptr<T> Resources::Get(const std::string& hashable)
{
	return Get<T>(std::hash<std::string>()(hashable));
}
