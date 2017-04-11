/*********************************************
* Detects Hardware for Windows, Mac OS, UNIX, and NVidia accelerated platforms.
**********************************************/
#include "sys_detect.hpp"

extern uint64_t GetCoreCount() 
{
#ifdef WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#elif MACOS
    int nm[2];
    size_t len = 4;
    uint32_t count;

    nm[0] = CTL_HW; nm[1] = HW_AVAILCPU;
    sysctl(nm, 2, &count, &len, NULL, 0);

    if(count < 1) 
	{
        nm[1] = HW_NCPU;
        sysctl(nm, 2, &count, &len, NULL, 0);
        if(count < 1) { count = 1; }
    }
    return count;
#else
    return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

extern uint64_t GetAvailableMemory()
{
#ifdef WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return status.ullTotalPhys;
#elif MACOS
	int mib [] = { CTL_HW, HW_MEMSIZE };
	int64_t value = 0;
	size_t length = sizeof(value);

	if(-1 == sysctl(mib, 2, &value, &length, NULL, 0))
	return value;
#else
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return pages * page_size;
#endif
}

#ifdef __CUDACC__
extern bool CheckCUDASupport()
{
	int device_count;
	int error = cudaGetDeviceCount(&device_count);
	if(error == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

extern uint64_t GetCUDACoreCount()
{  
	cudaDeviceProp devProp;
	cudaGetDeviceProperties(&devProp, 0);
    int cores = 0;
    int mp = devProp.multiProcessorCount;
    switch (devProp.major)
	{
		case 2: // Fermi
			if (devProp.minor == 1) cores = mp * 48;
			else cores = mp * 32;
			break;
		case 3: // Kepler
			cores = mp * 192;
			break;
		case 5: // Maxwell
			cores = mp * 128;
			break;
		case 6: // Pascal
			if (devProp.minor == 1) cores = mp * 128;
			else if (devProp.minor == 0) cores = mp * 64;
			break;
		default: break;
    }
    return cores;
}

extern uint64_t GetCUDAMemory()
{  
	cudaDeviceProp devProp;
	cudaGetDeviceProperties(&devProp, 0);
    return devProp.totalGlobalMem;
}
#endif