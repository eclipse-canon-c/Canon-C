# dsPIC and PIC24's core, with Microchip's XC-DSC: `dspic`

dsPIC33 and PIC24 share a 16-bit core: `int`, `size_t` and pointers of
16 bits, as on AVR, so Canon-C's Tier 0 only (the README's platform
tiers). `2 · Platforms`' job `dspic` compiles for them with Microchip's
XC-DSC 4.00 (VERIFY-107). Nothing is linked or run: QEMU has no model of
either, and Microchip's simulator comes with MPLAB X, not with the
compiler.

**The compiler.** XC-DSC 4.00, of 8 July 2026, is Microchip's compiler
for dsPIC33, and since that day every MPLAB XC compiler's optimization
levels are free; from 4.00, XC-DSC needs no key for them. The job fetches
its archive for Linux, `xc-dsc-v4.00-full-install-linux64.tar.xz`, 83 MB,
from Microchip, and checks its SHA-256 before anything in it runs: the
digest Microchip's download page shows abbreviated (`4fa70007... acef`),
measured whole at VERIFY-107's run 1 and pinned since VERIFY-108, which
also keeps the archive in the Actions cache by it, as `msp430` keeps TI's
package. Nothing is installed: the archive is unpacked and its
`xc-dsc-v4.00/bin` put on the `PATH`. Using XC-DSC accepts Microchip's
licence for it, which the author chose to do (VERIFY-107). Microchip gives
XC16, not XC-DSC, for PIC24's devices; XC16 2.10 is an installer, and its
optimizations above `-O1` want a key from a Microchip account.

**The devices.** XC-DSC keeps its device support in Microchip's device
family packs, apart from the compiler. The job uses two of its virtual
devices, which describe a core with no peripherals: `generic-16dsp`, a
core with the DSP instructions (dsPIC's), and `generic-16bit`, one
without (PIC24's). Both compile without a pack (VERIFY-107); with no
`-mcpu`, XC-DSC warns that the default device's resource file is not
there.

**The checks.** `probe.sh` reports what the compiler is: its version, its
devices, whether an empty unit compiles at each device, the type sizes
there with and without `-fno-short-double` (without it, `double` has 32
bits), and whether each optimization level, and the project's warning
flags, pass without a message. Its output must be `probe.pinned`, line for
line, the compiler's directory written `<bin>`. `tools/tier_check.sh` then
checks the platform tiers at each device, as for AVR: XC-DSC's own 32-bit
`double` refused at `types.h`, and with `-fno-short-double`, Tier 0
compiling and Tier 1 refused by its `#error`, every public header
classified.

**Runs.** VERIFY-107's run 1 measured: the probe was printed, the tier
check reported only (`REPORT_ONLY`), and the job could fail only on the
fetch, the digest or the archive. Every figure was as the record expected:
`int`, `size_t` and pointers of 2 bytes, `double` 4 and 8 with
`-fno-short-double`, plain `char` signed, GCC 8.3.1 beneath, every level
without a message; of the 80 headers, 30 compile, 49 are refused by Tier
1, and `util/time.h` did not compile, the check's `-I util` making its
`<time.h>` itself (VERIFY-110 dropped it). `probe.pinned` is run 1's
output. VERIFY-108 enforces both: a difference fails the job.
