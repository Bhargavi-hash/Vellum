# Vellum

Extremely lightweight vector note-taking for Linux (Qt6).

## Build (CMake)

Install CMake + Qt6 development packages (Widgets + Sql + PrintSupport) and a compiler toolchain.

Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y cmake g++ qt6-base-dev qt6-base-dev-tools libqt6sql6-sqlite
```

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/vellum
```
