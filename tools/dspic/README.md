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
its archive for Linux, `xc-dsc-v4.00-full-install-linux64.tar.xz`, from
Microchip, and checks its digest against the one Microchip's download page
shows, abbreviated to its first eight digits and last four (`4fa70007...
acef`), before anything in it runs; the full digest is pinned once the
job has printed it. Nothing is installed: the archive is unpacked and its
`xc-dsc-gcc` put on the `PATH`. Using XC-DSC accepts Microchip's licence
for it, which the author chose to do (VERIFY-107). Microchip gives XC16,
not XC-DSC, for PIC24's devices; XC16 2.10 is an installer, and its
optimizations above `-O1` want a key from a Microchip account.

**The devices.** XC-DSC keeps its device support in Microchip's device
family packs, apart from the compiler. The job uses two of its virtual
devices, which describe a core with no peripherals: `generic-16dsp`, a
core with the DSP instructions (dsPIC's), and `generic-16bit`, one
without (PIC24's). Whether they compile without a pack is among what run
1 measures.

**The checks.** `probe.sh` prints what the compiler is: its version, its
devices, whether an empty unit compiles at each device, the type sizes
there with and without `-fno-short-double` (without it, `double` has 32
bits), and whether each optimization level, and the project's warning
flags, pass without a message. `tools/tier_check.sh` then checks the
platform tiers at each device, as for AVR: XC-DSC's own 32-bit `double`
refused at `types.h`, and with `-fno-short-double`, Tier 0 compiling and
Tier 1 refused by its `#error`, every public header classified.

**Runs.** VERIFY-107's run 1 measures: the probe is printed, the tier
check reports only (`REPORT_ONLY`), and the job fails only if the fetch,
the digest or the archive does. A later record pins what run 1 measured,
and from then on a difference fails the job.
