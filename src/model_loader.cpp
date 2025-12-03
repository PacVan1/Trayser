#include <pch.h>

#include <engine.h>
#include <model_loader.h>

namespace trayser::mikktspace
{

static int GetVertexPerFaceCount(const SMikkTSpaceContext* context, const int iFace)
{
    return 3;
}

static int GetVertexIndex(const SMikkTSpaceContext* context, int iFace, int iVert)
{
    return iFace * GetVertexPerFaceCount(context, iFace) + iVert;
}

static int GetFaceCount(const SMikkTSpaceContext* context)
{
    LoadingMesh* mesh = static_cast<LoadingMesh*>(context->m_pUserData);
    float fSize = (float)mesh->vertexCount / 3.f;
    int iSize = (int)mesh->vertexCount / 3;
    assert((fSize - (float)iSize) == 0.f);
    return iSize;
}

static void GetPosition(const SMikkTSpaceContext* context,
    float* outPos,
    const int iFace,
    const int iVert)
{
    auto mesh = (LoadingMesh*)context->m_pUserData;
    auto index = GetVertexIndex(context, iFace, iVert);
    auto vertices = mesh->vertices;

    outPos[0] = vertices[index].position.x;
    outPos[1] = vertices[index].position.y;
    outPos[2] = vertices[index].position.z;
}

static void GetNormal(const SMikkTSpaceContext* context,
    float* outNormal,
    const int iFace,
    const int iVert)
{
    auto mesh = (LoadingMesh*)context->m_pUserData;
    auto index = GetVertexIndex(context, iFace, iVert);
    auto vertices = mesh->vertices;

    outNormal[0] = vertices[index].normal.x;
    outNormal[1] = vertices[index].normal.y;
    outNormal[2] = vertices[index].normal.z;
}

static void GetTexCoord(const SMikkTSpaceContext* context,
    float* outUv,
    const int iFace,
    const int iVert)
{
    auto mesh = (LoadingMesh*)context->m_pUserData;
    auto index = GetVertexIndex(context, iFace, iVert);
    auto vertices = mesh->vertices;

    outUv[0] = vertices[index].uvX;
    outUv[1] = vertices[index].uvY;
}

static void SetTangent(
    const SMikkTSpaceContext* context,
    const float* tangentu,
    const float fSign,
    const int iFace,
    const int iVert)
{
    auto mesh = (LoadingMesh*)context->m_pUserData;
    auto index = GetVertexIndex(context, iFace, iVert);
    auto vertices = mesh->vertices;

    vertices[index].tangent.x = tangentu[0];
    vertices[index].tangent.y = tangentu[1];
    vertices[index].tangent.z = tangentu[2];
    vertices[index].handedness = -fSign;
}

} // namespace trayser::mikktspace

void trayser::ModelLoader::Init()
{
    m_mtsInterface.m_getNumFaces            = mikktspace::GetFaceCount;
    m_mtsInterface.m_getNumVerticesOfFace   = mikktspace::GetVertexPerFaceCount;
    m_mtsInterface.m_getNormal              = mikktspace::GetNormal;
    m_mtsInterface.m_getPosition            = mikktspace::GetPosition;
    m_mtsInterface.m_getTexCoord            = mikktspace::GetTexCoord;
    m_mtsInterface.m_setTSpaceBasic         = mikktspace::SetTangent;
    m_mtsContext.m_pInterface = &m_mtsInterface;
}

void trayser::ModelLoader::Load(const char* path)
{
	g_engine.m_resources.Create<trayser::Model>(path, path, &g_engine);
}

void trayser::ModelLoader::GenerateTangentSpace(LoadingMesh* mesh)
{
    m_mtsContext.m_pUserData = mesh;
    genTangSpaceDefault(&m_mtsContext);
}
