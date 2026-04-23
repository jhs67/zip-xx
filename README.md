# zip-xx

A C++ library for writing ZIP archives through the standard iostream interface. Designed for streaming output — no seeking on the backing stream is required.

## Features

- `zip_ostream` — a `std::ostream` that writes a ZIP archive to any other `std::ostream` or `std::streambuf`
- `zip_streambuf` — the underlying `std::streambuf` for use when you need direct control
- Always writes ZIP64 — no 4 GB file size limit or 65 535 entry limit
- DEFLATE and STORE compression with five compression levels
- UTF-8 filenames, per-entry comments, archive comment
- Unix timestamps (extended timestamp extra field, 0x5455)
- C++20, header-only public API (`include/zip-xx/zip-xx.h`)

## Usage

```cpp
#include <zip-xx/zip-xx.h>
#include <fstream>

std::ofstream file("output.zip", std::ios::binary);
zip_xx::zip_ostream zip(file);

zip.begin_entry("hello.txt");
zip << "Hello, world!\n";
zip.end_entry();

zip.close();
```

`zip_ostream` is a `std::ostream`, so anything that writes to a stream works without modification.

### Compression

```cpp
zip.begin_entry("data.bin",
    /*comment=*/{},
    /*stamp=*/std::chrono::system_clock::now(),
    zip_xx::compression_t::deflate,
    zip_xx::level_t::maximum);
```

`compression_t`: `store`, `deflate`  
`level_t`: `minimal`, `low`, `normal` (default), `high`, `maximum`

### Using zip_streambuf directly

```cpp
std::ofstream file("output.zip", std::ios::binary);
zip_xx::zip_streambuf buf(file);
std::ostream zip(&buf);

buf.begin_entry("readme.txt");
zip << "content";
buf.end_entry();
buf.close();
```

## Requirements

- C++20
- zlib

## Building

Uses [CMake](https://cmake.org) with [vcpkg](https://vcpkg.io) for dependencies.

```sh
cmake --preset default
cmake --build --preset Debug
ctest --preset Debug
```

## License

Copyright (c) 2026 Jon Spencer. ISC License. See [LICENSE](LICENSE).
