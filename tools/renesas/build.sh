#!/bin/sh
# tools/renesas/build.sh TARGET PREFIX [SRC] -- a bare-metal toolchain for
# TARGET (v850-elf for RH850), built from the release tarballs that
# Ubuntu's source packages ship (VERIFY-101):
#
#   binutils-source   binutils 2.42      the assembler, the linker
#   gcc-13-source     GCC 13.3.0         the C compiler, and libgcc
#   newlib-source     newlib 4.4.0       the C library, its system calls
#   gdb-source        GDB 15.1           the simulator, TARGET-run
#
# SRC (default /usr/src) is where the packages put the tarballs. Ubuntu's
# own patches to them, for its Linux builds, are not applied: the
# tarballs are the projects' releases. Each package is signed by Ubuntu's
# archive, so nothing is downloaded from elsewhere. The tarballs are
# unpacked and built in the current directory, and the toolchain is
# installed into PREFIX: TARGET-gcc and the rest in PREFIX/bin, with GCC's
# multilibs of newlib and libgcc. Run tools/renesas/renesas.py to use it.
set -eu

target=$1
prefix=$2
src=${3:-/usr/src}
jobs=$(nproc 2>/dev/null || echo 2)
work=$(pwd)

one() {   # the one file or directory a pattern names, or an error
  set -- $1
  if [ "$#" -ne 1 ] || [ ! -e "$1" ]; then
    echo "build.sh: expected one match, found: $*" >&2
    exit 1
  fi
  echo "$1"
}
binutils_tar=$(one "$src/binutils/binutils-*.tar.xz")
gcc_tar=$(one "$src/gcc-13/gcc-13.*.tar.xz")
newlib_tar=$(one "$src/newlib/newlib-*.tar.xz")
gdb_tar=$(one "$src/gdb.tar.xz")

mkdir -p "$work/src"
for t in "$binutils_tar" "$gcc_tar" "$newlib_tar" "$gdb_tar"; do
  echo "== unpacking $t"
  tar -xJf "$t" -C "$work/src"
done
binutils_src=$(one "$work/src/binutils-[0-9]*")
gcc_src=$(one "$work/src/gcc-13.*")
newlib_src=$(one "$work/src/newlib-[0-9]*")
gdb_src=$work/src/gdb

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

# MAKEINFO=true: the releases carry their manuals built; none is rebuilt.
# The tools are installed stripped, and GCC without LTO, which the tests do
# not use: the toolchain is kept in the Actions cache.
echo "== binutils"
mkdir -p "$work/build-binutils" && cd "$work/build-binutils"
step configure.log "$binutils_src/configure" --target="$target" --prefix="$prefix" \
  --disable-nls --disable-werror --disable-gdb --disable-sim \
  --disable-gprofng MAKEINFO=true
step make.log make -j"$jobs" MAKEINFO=true
step install.log make install-strip MAKEINFO=true

echo "== GCC, C only, and libgcc"
mkdir -p "$work/build-gcc" && cd "$work/build-gcc"
step configure.log "$gcc_src/configure" --target="$target" --prefix="$prefix" \
  --enable-languages=c --with-newlib --without-headers \
  --disable-nls --disable-shared --disable-threads --disable-libssp \
  --disable-libquadmath --disable-libgomp --disable-libatomic --disable-lto \
  MAKEINFO=true
step make.log make -j"$jobs" all-gcc all-target-libgcc MAKEINFO=true
step install.log make install-strip-gcc install-target-libgcc MAKEINFO=true

echo "== newlib"
mkdir -p "$work/build-newlib" && cd "$work/build-newlib"
step configure.log "$newlib_src/configure" --target="$target" --prefix="$prefix" \
  --disable-nls MAKEINFO=true
step make.log make -j"$jobs" MAKEINFO=true
step install.log make install MAKEINFO=true

# The simulator's configure requires readline: GDB's own copy, in the same
# tarball, which the top level configures and builds for it unless
# --disable-readline is given; the runner has no system readline to link
# (VERIFY-101's run 1).
echo "== GDB's simulator"
mkdir -p "$work/build-sim" && cd "$work/build-sim"
step configure.log "$gdb_src/configure" --target="$target" --prefix="$prefix" \
  --disable-gdb --disable-gdbserver --disable-binutils --disable-ld \
  --disable-gas --disable-gprof --disable-gprofng --disable-nls \
  --disable-werror --disable-libdecnumber \
  MAKEINFO=true
step make.log make -j"$jobs" all-sim MAKEINFO=true
step install.log make install-strip-sim MAKEINFO=true

echo "== installed in $prefix/bin:"
ls "$prefix/bin"
