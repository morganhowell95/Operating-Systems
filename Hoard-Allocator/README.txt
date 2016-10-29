This program is roughly based on the Hoard Allocator. It implements malloc, free, poisoning of both of these, and the ability to create a threshold of free superblocks that may be allowed at any one level at a given time. 

I worked alone on this assignment and signed the honor code at the top of "th_alloc.c".

I have created many (many) testers. For ease, I created a manifest of all my testers in "test.c". This file can be ran (after compilation) by running: LD_PRELOAD=./th_alloc.so ./test 

The Makefile in this directory can be used to compile all files, including the allocator, as well as clean up these files.

Please have fun with my implementation of Hoard!


I received no help or unwarranted assistance on this assignment and implicitly sign the honor code with my signature below:
Morgan J. Howell (mjhowell)


