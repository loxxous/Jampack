/*********************************************
* Detects Hardware for Windows, Mac OS, UNIX, and NVidia accelerated platforms.
* The System namespace allows us to skip querying the OS and external libraries once we already know the hardware.
* Querying the Cuda library is very costly and is only ever done once.
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
extern bool CheckCudaSupport();

extern uint64_t GetCudaCoreCount();

extern uint64_t GetCudaMemory();

#endif

#endif // SYS_DETECT_H
