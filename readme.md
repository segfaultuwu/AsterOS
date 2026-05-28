# AsterOS

> [!NOTE]
> **AsterOS is experimental.**
> This project is a hobby x86_64 kernel built with Limine, Flanterm, GDT/TSS, IDT, PMM/VMM and early usermode experiments.

# clone:
```bash
# clone normally, then:
git submodule update --init --recursive
```

> [!WARNING]
> **DO NOT** RUN THIS ON REAL HARDWARE UNLESS YOU WANT YOUR DISK TO GET CORRUPTED

# Boot:
```bash
mkdir build
cd build
cmake ..
make run
```
