# The tests on big-endian 32-bit Arm Linux under qemu-user: `armeb`

`2 · Platforms`' job `armhf` runs the tests on 32-bit Arm Linux with
Ubuntu's cross toolchain, its data little-endian. The job `armeb` runs
them with the data big-endian (VERIFY-121): the same core, Armv7-A, with
Thumb-2, VFPv3-D16 and the hard-float ABI, at the same four legs (Release
and Debug, each with `CANON_LIFETIME` off and `debug`), each test under
`qemu-armeb`, qemu-user's emulator of big-endian 32-bit Arm, which
CTest runs as at `armhf`.

Ubuntu ships no compiler and no C library for big-endian Arm, but it
ships the sources of both. `build.sh` builds a toolchain,
`armeb-linux-gnueabihf`, from the release tarballs of three of Ubuntu's
source packages, with the kernel's headers that Ubuntu ships for 32-bit
Arm:

| package | release | what it gives |
|---------|---------|---------------|
| `binutils-source` | binutils 2.42 | the assembler and the linker |
| `gcc-13-source` | GCC 13.3.0 | the C compiler and libgcc |
| `glibc-source` | glibc 2.39 | the C library, its dynamic linker among it |
| `linux-libc-dev-armhf-cross` | Linux 6.8 | the kernel's headers |

Each package is checked against Ubuntu's signed index, as
`.github/actions/apt-install` installs it; nothing is fetched from
anywhere else. Ubuntu's own patches to the sources are not applied: the
tarballs are the projects' releases, as for `tools/renesas/build.sh`. The
kernel's headers for 32-bit Arm serve either byte order (those that
differ test `__ARMEB__` or `__BYTE_ORDER` as they are compiled), so the
files of `linux-libc-dev-armhf-cross` are copied as they are.

`build.sh PREFIX` builds in the usual order: binutils; GCC with C only
and no C library, its first stage; the kernel's headers; glibc, compiled
by that GCC; GCC again, its second stage, against glibc, with libgcc
shared too. GCC's defaults are Ubuntu's armhf's, Armv7-A with VFPv3-D16
(which Ubuntu writes `armv7-a+fp`), the hard-float ABI and Thumb-2, the
data big-endian. glibc's configure is given the target's `g++`, which
there is none of, so that it builds no C++ with the build machine's, and
`MAKEINFO=:`, so that it neither builds nor installs its manual
(`build.sh` says why). The toolchain goes into PREFIX,
`armeb-linux-gnueabihf-gcc` and the rest in `PREFIX/bin`, and the C
library into the sysroot, `PREFIX/armeb-linux-gnueabihf/libc`, in which
`qemu-armeb -L` runs a program. On two CPUs it builds in 36 minutes
(binutils in 2, GCC's first stage in 13, glibc in 6, its second stage in
15), and takes 166 MB. The job keeps it in the Actions cache, its key the
four packages' versions and `build.sh`'s hash, the entry `analysis.yml`'s
`compcert-armeb` restores too; a leg that misses it builds it.

**BE8.** Armv7-A's big-endian is BE8: the data big-endian, the
instructions little-endian. GCC compiles and assembles big-endian code
as ever, and its driver gives the linker `--be8` for a big-endian link
at Armv6 or later (`BE8_LINK_SPEC`), which turns the instructions round
and marks the binary. The job checks the mark in each test binary's
flags (`readelf -h`), and that `file` reads one as "ELF 32-bit MSB".

**What differs from `armhf`, besides the byte order.** Ubuntu's GCC for
armhf has defaults of Ubuntu's own, which GCC 13.3.0 as released does
not: it defines `_TIME_BITS=64` and `_FILE_OFFSET_BITS=64`, so that
`time_t` and `off_t` have 64 bits there, and here 32, glibc's default on
32-bit Arm; and it builds position-independent executables, with
`_FORTIFY_SOURCE=3` and the stack protector, where this GCC builds
neither. The two compilers' predefined macros differ in those and in the
byte order's, and in nothing else: every type has the same size and
alignment at both, and plain `char` is unsigned (VERIFY-121).

**The checks.** As at `armhf`: the binaries Armv7-A, of the application
profile, with VFP's registers for arguments (`readelf -A`), `main` Thumb
code (its address odd), each test binary needing `libc.so.6`; the 64-bit
atomic of `lifetime.h` lock-free (LDREXD and STREXD), so that no test is
linked with libatomic and `lifetime_test` holds LDREXD. A notice per leg
says how many binaries hold the atomic inline.

**CompCert.** `analysis.yml`'s `compcert-armeb` builds CompCert 3.17 for
its `armeb-linux` configuration (Armv7-A, big-endian, the hard-float
ABI) with this toolchain, which preprocesses, assembles and links for
it, compiles each test with ccomp, and runs it under `qemu-armeb` in the
same sysroot: every test passes, and the job is held to that
(VERIFY-121, VERIFY-122); informational, as every CompCert job
(`docs/compcert-licensing.md`).

**Here.** On Ubuntu 24.04, with the job's packages installed (the four
above; GMP, MPFR and MPC's headers, `libgmp-dev`, `libmpfr-dev` and
`libmpc-dev`, which GCC's build needs; `flex`, `bison` and `gawk`, the
last two glibc's; and `qemu-user`):

```sh
mkdir build-toolchain && cd build-toolchain
sh ../tools/armeb/build.sh "$HOME/gcc-toolchain/armeb-linux-gnueabihf"
cd ..
T=$HOME/gcc-toolchain/armeb-linux-gnueabihf
cmake -B build-armeb -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=arm \
  "-DCMAKE_C_COMPILER=$T/bin/armeb-linux-gnueabihf-gcc" \
  "-DCMAKE_CROSSCOMPILING_EMULATOR=qemu-armeb;-L;$T/armeb-linux-gnueabihf/libc"
cmake --build build-armeb -j
(cd build-armeb && ctest --output-on-failure)
```

`build.sh` finds the tarballs where the packages install them, in
`/usr/src`, or in a folder given as its second argument.
