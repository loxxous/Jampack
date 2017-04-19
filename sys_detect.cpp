/*********************************************
* Detects Hardware for Windows, Mac OS, UNIX, and NVidia accelerated platforms.
**********************************************/
#include "sys_detect.hpp"

namespace System
{
	namespace Cpu
	{
		int64_t Memory = -1;
		int64_t Cores = -1;
	};
	
	namespace Gpu
	{
		int HasCudaSupport = -1;
		int64_t Memory = -1;
		int64_t Cores = -1;
	};
};

extern uint64_t GetCoreCount() 
{
	if(System::Cpu::Cores == -1)
	{
		int64_t Cores;
	#ifdef WIN32
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		Cores = sysinfo.dwNumberOfProcessors;
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
		Cores = count;
	#else
		Cores = sysconf(_SC_NPROCESSORS_ONLN);
	#endif

		System::Cpu::Cores = Cores;
		return Cores;
	}
	else
		return System::Cpu::Cores;
}

extern uint64_t GetAvailableMemory()
{
	if(System::Cpu::Memory == -1)
	{
		int64_t Memory;
	#ifdef WIN32
		MEMORYSTATUSEX status;
		status.dwLength = sizeof(status);
		GlobalMemoryStatusEx(&status);
		Memory = status.ullTotalPhys;
	#elif MACOS
		int mib [] = { CTL_HW, HW_MEMSIZE };
		int64_t value = 0;
		size_t length = sizeof(value);

		if(-1 == sysctl(mib, 2, &value, &length, NULL, 0))
		Memory = value;
	#else
		long pages = sysconf(_SC_PHYS_PAGES);
		long page_size = sysconf(_SC_PAGE_SIZE);
		Memory = pages * page_size;
	#endif
		System::Cpu::Memory = Memory;
		return Memory;
	}
	else
		return System::Cpu::Memory;
}

#ifdef __CUDACC__
extern bool CheckCudaSupport()
{
	if(System::Gpu::HasCudaSupport == -1)
	{
		int device_count;
		int error = cudaGetDeviceCount(&device_count);
		if(error == 0)
		{
			System::Gpu::HasCudaSupport = true;
			return true;
		}
		else
		{
			System::Gpu::HasCudaSupport = false;
			return false;
		}
	}
	else
		return System::Gpu::HasCudaSupport;
}

extern uint64_t GetCudaCoreCount()
{  
	if(System::Gpu::Cores == -1)
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
		System::Gpu::Cores = cores;
		return cores;
	}
	else
		return System::Gpu::Cores;
}

extern uint64_t GetCudaMemory()
{  
	if(System::Gpu::Memory == -1)
	{
		cudaDeviceProp devProp;
		cudaGetDeviceProperties(&devProp, 0);
		System::Gpu::Memory = devProp.totalGlobalMem;
		return System::Gpu::Memory;
	}
	else
		return System::Gpu::Memory;
}
#endif
