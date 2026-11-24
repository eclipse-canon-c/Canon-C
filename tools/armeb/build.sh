#!/bin/sh
# tools/armeb/build.sh PREFIX [SRC] -- a toolchain for 32-bit big-endian
# Arm Linux, armeb-linux-gnueabihf, built from the release tarballs that
# Ubuntu's source packages ship, and the kernel's headers that Ubuntu ships
# for 32-bit Arm (VERIFY-121):
#
#   binutils-source             binutils 2.42   the assembler, the linker
#   gcc-13-source               GCC 13.3.0      the C compiler, libgcc
#   glibc-source                glibc 2.39      the C library
#   linux-libc-dev-armhf-cross  Linux 6.8       the kernel's headers
#
# GCC's defaults are Ubuntu's armhf's: Armv7-A, Thumb-2, VFPv3-D16 and the
# hard-float ABI; the data big-endian. GCC links Armv7's big-endian code as
# BE8, its instructions little-endian. Not Ubuntu's own defaults, which GCC
# as released has not: no PIE, no _FORTIFY_SOURCE, no stack protector, and
# no _TIME_BITS=64, so that time_t has glibc's 32 bits. The kernel's
# headers for 32-bit Arm serve either byte order (those that differ test
# __ARMEB__ or __BYTE_ORDER): the files linux-libc-dev-armhf-cross installs
# are copied as they are.
#
# SRC (default /usr/src) is where the packages put the tarballs. Ubuntu's
# own patches to them are not applied: the tarballs are the projects'
# releases, as for tools/renesas/build.sh. Each package is signed by
# Ubuntu's archive, so nothing is downloaded from elsewhere. Built in the
# usual order: binutils; GCC with C only and no C library, its first
# stage; the kernel's headers; glibc, compiled by that GCC; GCC again, its
# second stage, against glibc, with libgcc shared too. The tarballs are
# unpacked and built in the current directory; the toolchain is installed
# into PREFIX, armeb-linux-gnueabihf-gcc and the rest in PREFIX/bin, and
# the C library into the sysroot, PREFIX/armeb-linux-gnueabihf/libc, in
# which qemu-armeb -L runs a program.
set -eu

target=armeb-linux-gnueabihf
prefix=$1
src=${2:-/usr/src}
sysroot=$prefix/$target/libc
jobs=$(nproc 2>/dev/null || echo 2)
work=$(pwd)

one() {   # the one file or directory a pattern names, or an error
  # shellcheck disable=SC2086 # the pattern, expanded
  set -- $1
  if [ "$#" -ne 1 ] || [ ! -e "$1" ]; then
    echo "build.sh: expected one match, found: $*" >&2
    exit 1
  fi
  echo "$1"
}
binutils_tar=$(one "$src/binutils/binutils-*.tar.xz")
gcc_tar=$(one "$src/gcc-13/gcc-13.*.tar.xz")
glibc_tar=$(one "$src/glibc/glibc-*.tar.xz")

mkdir -p "$work/src"
for t in "$binutils_tar" "$gcc_tar" "$glibc_tar"; do
  echo "== unpacking $t"
  tar -xJf "$t" -C "$work/src"
done
binutils_src=$(one "$work/src/binutils-[0-9]*")
gcc_src=$(one "$work/src/gcc-13.*")
glibc_src=$(one "$work/src/glibc-[0-9]*")

PATH=$prefix/bin:$PATH
export PATH

step() {  # step LOG COMMAND...: the command, its output in LOG; on failure,
          # LOG's last lines, so that a failed build says why in CI's log
  log=$1
  shift
  if ! "$@" > "$log" 2>&1; then
    echo "build.sh: failed in $(pwd): $* -- the last lines of $log:" >&2
    tail -n 60 "$log" >&2
    exit 1
  fi
}

# GCC's defaults, both stages: Ubuntu's armhf's.
arm="--with-arch=armv7-a --with-fpu=vfpv3-d16 --with-float=hard --with-mode=thumb"

# MAKEINFO=true, for binutils and GCC: their releases carry their manuals
# built, and none is rebuilt.
# The tools are installed stripped, and GCC without LTO, which the tests do
# not use: the toolchain is kept in the Actions cache.
echo "== binutils"
mkdir -p "$work/build-binutils" && cd "$work/build-binutils"
step configure.log "$binutils_src/configure" --target="$target" --prefix="$prefix" \
  --with-sysroot="$sysroot" --disable-nls --disable-werror --disable-gdb \
  --disable-sim --disable-gprofng MAKEINFO=true
step make.log make -j"$jobs" MAKEINFO=true
step install.log make install-strip MAKEINFO=true

echo "== GCC, its first stage: C only, no C library"
mkdir -p "$work/build-gcc1" && cd "$work/build-gcc1"
# shellcheck disable=SC2086 # $arm is four options
step configure.log "$gcc_src/configure" --target="$target" --prefix="$prefix" \
  --with-sysroot="$sysroot" $arm --enable-languages=c --with-newlib \
  --without-headers --disable-nls --disable-shared --disable-threads \
  --disable-libssp --disable-libquadmath --disable-libgomp \
  --disable-libatomic --disable-lto MAKEINFO=true
step make.log make -j"$jobs" all-gcc all-target-libgcc MAKEINFO=true
step install.log make install-strip-gcc install-target-libgcc MAKEINFO=true

echo "== the kernel's headers, from linux-libc-dev-armhf-cross"
headers=/usr/arm-linux-gnueabihf/include
mkdir -p "$sysroot/usr/include"
n=0
for f in $(dpkg-query -L linux-libc-dev-armhf-cross | grep "^$headers/.*\.h\$"); do
  rel=${f#"$headers"/}
  mkdir -p "$sysroot/usr/include/$(dirname "$rel")"
  cp "$f" "$sysroot/usr/include/$rel"
  n=$((n + 1))
done
echo "$n headers"
[ "$n" -gt 900 ] || { echo "build.sh: too few kernel headers: $n" >&2; exit 1; }

# CXX: the target's C++ compiler, which there is none of. glibc uses one
# only for its tests, and finding none for the target, its configure takes
# the build machine's own g++, with which its make would build a test
# program, support/links-dso-program, for the build machine and link it
# for the target. Named, and found not to link, it is left unset.
# MAKEINFO=:, glibc's word for no makeinfo: its manual is neither built nor
# installed. Its make is given no MAKEINFO, which would override that, and
# with which its install looks for the manual it did not build.
echo "== glibc"
mkdir -p "$work/build-glibc" && cd "$work/build-glibc"
step configure.log "$glibc_src/configure" --host="$target" \
  --build="$("$glibc_src/scripts/config.guess")" --prefix=/usr \
  --with-headers="$sysroot/usr/include" --disable-werror --disable-nscd \
  --disable-timezone-tools --disable-profile CXX="$target-g++" MAKEINFO=:
step make.log make -j"$jobs"
step install.log make install install_root="$sysroot"

echo "== GCC, its second stage: against glibc"
mkdir -p "$work/build-gcc2" && cd "$work/build-gcc2"
# shellcheck disable=SC2086 # $arm is four options
step configure.log "$gcc_src/configure" --target="$target" --prefix="$prefix" \
  --with-sysroot="$sysroot" $arm --enable-languages=c --disable-nls \
  --enable-shared --enable-threads=posix --disable-libssp \
  --disable-libquadmath --disable-libgomp --disable-libsanitizer \
  --disable-lto --disable-multilib MAKEINFO=true
step make.log make -j"$jobs" MAKEINFO=true
step install.log make install-strip MAKEINFO=true

echo "== installed in $prefix/bin:"
ls "$prefix/bin"
echo "== the sysroot, $sysroot:"
ls "$sysroot" "$sysroot/lib"
