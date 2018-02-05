# threadpool
Simple C++ thread pool self-contained in a header file.

This is just a research project. It's lacking efficiency (it's using locking for queueing when it could use a lockless queue), cleanliness and proper testing.

For usage, see ```threadpool/main.cpp```.
