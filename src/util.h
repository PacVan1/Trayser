#pragma once 

#include <functional>
#include <deque>

namespace vkutil
{

class DeletionQueue
{
public:
	void Push(std::function<void()>&& function);
	void Flush();

private:
	std::deque<std::function<void()>> m_deletors;
};

} // namespace vkutil