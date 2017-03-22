# Jampack
Experimental multi-threaded Burrows-Wheeler compression algorithm.
Decoding is parallel on a single compressed block which reduces memory usage to just 6N regardless of the number of threads.
