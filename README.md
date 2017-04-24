# Jampack
Experimental multi-threaded Burrows-Wheeler compression algorithm.
Decoding is parallel on a single compressed block which reduces memory usage to just 6N regardless of the number of threads. 

It features the first Cuda accelerated Burrows Wheeler inversion algorithm utilizing up to 840 parallel decode units on a single block.
