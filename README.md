# zip-xx

A C++ library for reading and writing ZIP archives through the standard iostream interface.

## Features

### Writing
- `zip_ostream` — a `std::ostream` that writes a ZIP archive to any other `std::ostream` or `std::streambuf`
- `zip_streambuf` — the underlying `std::streambuf` for direct control
- No seeking required on the backing stream — works with pipes, sockets, and any non-seekable destination
- Always writes ZIP64 — no 4 GB file size limit or 65 535 entry limit
- DEFLATE and STORE compression with five compression levels
- UTF-8 filenames, per-entry comments, archive comment
- Unix timestamps (extended timestamp extra field, 0x5455)

### Reading
- `zip_archive` — opens an archive and caches the central directory
- `zip_istream` — a `std::istream` for reading one entry's data
- `zip_istreambuf` — the underlying `std::streambuf` for direct control
- Seeking within entries — both STORED and DEFLATE (DEFLATE backward seek restarts decompression)
- Multiple entries open simultaneously against the same archive
- Reads both ZIP64 and standard (non-ZIP64) archives

## Usage

### Writing

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

#### Compression

```cpp
zip.begin_entry("data.bin",
    /*comment=*/{},
    /*stamp=*/std::chrono::system_clock::now(),
    zip_xx::compression_t::deflate,
    zip_xx::level_t::maximum);
```

`compression_t`: `store`, `deflate`  
`level_t`: `minimal`, `low`, `normal` (default), `high`, `maximum`

#### Using `zip_streambuf` directly

```cpp
std::ofstream file("output.zip", std::ios::binary);
zip_xx::zip_streambuf buf(file);
std::ostream zip(&buf);

buf.begin_entry("readme.txt");
zip << "content";
buf.end_entry();
buf.close();
```

### Reading

```cpp
#include <zip-xx/zip-xx.h>
#include <fstream>

std::ifstream file("input.zip", std::ios::binary);
zip_xx::zip_archive arc(file);

// List entries
for (const auto& entry : arc.entries())
    std::cout << entry.name << " (" << entry.uncompressed_size << " bytes)\n";

// Read an entry by name
const zip_xx::zip_entry* e = arc.find("hello.txt");
if (e) {
    zip_xx::zip_istream is(arc, *e);
    std::string text(std::istreambuf_iterator<char>(is), {});
}
```

`zip_istream` is a `std::istream`, so stream extraction operators, `std::getline`, and algorithms that accept input iterators all work without modification.

#### Seeking within an entry

```cpp
zip_xx::zip_istream is(arc, *e);
is.seekg(1024);                  // forward seek
is.seekg(0, std::ios::beg);     // back to the start
is.seekg(-16, std::ios::end);   // relative to end
auto pos = is.tellg();
```

#### Multiple entries open simultaneously

```cpp
zip_xx::zip_istream header(arc, *arc.find("header.bin"));
zip_xx::zip_istream data(arc, *arc.find("data.bin"));

// Read from either stream in any order
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
