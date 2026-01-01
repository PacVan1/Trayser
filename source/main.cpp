#include <pch.h>
#include <engine.h>

#ifdef main
#undef main
#endif

using namespace trayser;

int main(int argc, char* argv[])
{
	g_engine.Init();
    try 
    {
	    g_engine.Run();	
    }
    catch (const std::exception& e) 
    {
        std::cerr << "Error: " << e.what() << "\n";
        system("pause"); // keep console open
        return -1;
    }
	g_engine.Destroy();	

	return 0;
}
