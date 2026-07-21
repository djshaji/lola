# LV2 Loader
Simple library to load LV2 plugins in C/C++.

## Roadmap
- Parse ttl files using [serd](https://drobilla.net/software/serd/) library
- Load shared libraries using [dlopen](https://man7.org/linux/man-pages/man3/dlopen.3.html)
- Collect plugin information (URI, name, features, ports, etc.)
- Create plugin instances and manage their lifecycle

## Features
- Most of the LV2 core features are supported, including:
  - Plugin discovery and loading
  - Port enumeration and connection
  - Feature support querying
  - Basic plugin instance management

## LV2 Specification Support
- Non standard, mostly, but it supports the following:
  - LV2 Core
  - LV2 Extensions (some of them)
  - LV2 Features (some of them)

Notably, we want to support atom:Path, so that we can load NAM and IR

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details