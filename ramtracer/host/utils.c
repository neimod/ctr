#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
#endif

void mssleep(unsigned int millisecs)
{
#ifdef _WIN32
	Sleep(millisecs);
#else
	usleep(millisecs * 1000);
#endif
}