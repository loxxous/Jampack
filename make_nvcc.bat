nvcc main.cpp jampack.cpp ans.cpp checksum.cpp divsufsort.cpp lz77.cpp model.cpp rank.cpp rle.cpp format.cpp -x cu bwt.cpp sys_detect.cpp -L /usr/local/cuda/lib -lcudart -o Jampack_nv -Wno-deprecated-gpu-targets -ccbin "C:\Program Files (x86)\Microsoft Visual Studio\Shared\14.0\VC\bin" --compiler-options="-O2 -openmp"
PAUSE
