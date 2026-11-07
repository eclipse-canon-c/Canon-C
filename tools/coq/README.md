# tools/coq: bits.h's twelve residual goals, proved in Coq

The automatic provers (Alt-Ergo, Z3, cvc5) leave twelve of bits.h's goals
unproved at every machine model: `frama-c-bits` proves 745 / 757 at
x86_64. Each goal is argued true in docs/deviations.md. VERIFY-061 proves
them in Coq, as WP states them. The job is `12 · Coq: bits.h`
(`.github/workflows/coq.yml`).

| File | What it is |
|---|---|
| `session/interactive/<goal>.v` | The twelve proofs. Each file is the script WP writes for its goal, byte for byte, with the proof after WP's `intros` line. WP names each file after its goal without the `typed_` prefix. |
| `replay.sh` | Phase B's check. It compiles each script as Why3 runs Coq, with what the proof rests on (`Print Assumptions`). It then runs `frama-c-bits`'s command with Coq added, in WP's batch mode, on a copy of the session. |
| `phase_a.sh` | Phase A's tool. It writes the twelve scripts as WP gives them, without proofs, and prints the sources of Why3's Coq library. |

**Versions.** Coq 8.18.0 and Why3 1.7.2's Coq library (opam package
`why3-coq`), the versions in the job. Coq is installed into the Frama-C
switch in that job only.

**What a proof rests on.** Each script holds WP's theory for its goal as
Coq declarations and axioms. Each proof uses those declarations and
nothing else, which `Print Assumptions` shows. Coq's kernel checks the
proof, and popcount's per-byte check also relies on Coq's `vm_compute`.

**How the proofs go.** WP's bit operators are declared, not defined. Ten
of the proofs begin with the same block, which derives from WP's axioms
alone that `land`, `lor`, `lxor`, `lsl`, `lsr`, `to_uint32` and
`to_uint64` are Coq's `Z` operations. Each goal is then proved in Coq's
integers. The record has each argument.

**If bits.h's goals change**, WP writes different scripts. Run
`phase_a.sh` to get the new scripts, then carry each proof over by hand,
after WP's `intros` line. A goal whose statement changed may need its
proof changed too.

**To check the proofs locally**, install Coq 8.18 and why3-coq 1.7.2 with
opam, then run:

    coqc -R "$(why3 --print-libdir)/coq" Why3 tools/coq/session/interactive/<goal>.v
