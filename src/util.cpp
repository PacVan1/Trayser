#include "util.h"

void vkutil::DeletionQueue::Push(std::function<void()>&& function)
{
	m_deletors.push_back(function);
}

void vkutil::DeletionQueue::Flush()
{
	// reverse iterate the deletion queue to execute all the functions
	for (auto it = m_deletors.rbegin(); it != m_deletors.rend(); it++) 
	{
		(*it)(); //call functors
	}

	m_deletors.clear();
}