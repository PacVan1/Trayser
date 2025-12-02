#include <pch.h>
#include <engine.h>

#ifdef main
#undef main
#endif

using namespace trayser;

int main(int argc, char* argv[])
{
	g_engine.Init();	
	g_engine.Run();	
	g_engine.Cleanup();	

	return 0;
}
