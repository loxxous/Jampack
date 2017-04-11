/*********************************************
* Detects Hardware for Windows, Mac OS, UNIX, and NVidia accelerated platforms.
**********************************************/
#ifndef SYS_DETECT_H
#define SYS_DETECT_H

#include <stdint.h>

#ifdef _WIN32
	#include <windows.h>
#elif MACOS
	#include <sys/param.h>
	#include <sys/sysctl.h>
#else
	#include <unistd.h>
#endif

extern uint64_t GetCoreCount();

extern uint64_t GetAvailableMemory();

#ifdef __CUDACC__
extern bool CheckCUDASupport();

extern uint64_t GetCUDACoreCount();

extern uint64_t GetCUDAMemory();
#endif

#endif // SYS_DETECT_H