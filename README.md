# chat
simple C++ multiprocess web socket chat service

## Overview
run as master process with fork configured number childs

### requirements
[ASIO](http://think-async.com) ([asio github](https://github.com/chriskohlhoff/asio))
[SQlite](https://sqlite.org)
[spdlog](https://github.com/gabime/spdlog)
[nlohmann/json](https://github.com/nlohmann/json)

### Build System
- **CMake 3.21+**: now only testing on linux

```
./build.sh
```

### running
```
cd build && ./chat
```
open another console and run test.sh
```
./test.sh
```
on waiting tested operations press Ctrl+C
