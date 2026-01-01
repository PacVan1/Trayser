#pragma once

#include <loader.h>
#include <mikktspace.h>

namespace trayser
{

class ModelLoader
{
public:
	void Init();
	void Load(const char* path);
	void GenerateTangentSpace(LoadingMesh* mesh);

private:
	// Tangent space
	SMikkTSpaceInterface m_mtsInterface;
	SMikkTSpaceContext   m_mtsContext;
};

} // namespace trayser
