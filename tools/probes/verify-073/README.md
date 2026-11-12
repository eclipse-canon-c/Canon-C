# VERIFY-073's probe: diag_push's contract, measured before it lands

`diag_push`'s contract kept only the `code` of the frames before the one it
pushes. A program proved with WP alone could therefore not call
`diag_render` after two pushes, although the body keeps the frames whole
(VERIFY-066, "Found while reading"). `diag_push.patch` changes
`semantics/diag.h`:

- `push_frames_preserved` and `push_shift_semantics` state the earlier
  frames kept whole (`d->frames[j] == \old(...)`), not only their `code`;
- `push_file_read` and `push_func_read` ask that `file` and `func` be NULL
  or strings outside `*d`, as `__FILE__` and `__func__` are;
- `push_frame_strings` promises the new frame renderable
  (`frame_strings_ok`, `diag_render`'s `r_strings`);
- a full chain shifts one frame at a time, with a loop WP follows, instead
  of `memmove`, whose byte copy it cannot.

Run 1 found every goal of it proved but 6 of `push_frame_strings`' 13
parts: that `file` and `func` are still strings after the push. Run 1b
measures four variants of the patch, each under `variants/`, a whole patch
of `diag.h` from the checkout:

| variant | patch | what it adds to `diag_push.patch` (P0) |
|---------|-------|------------------------------------------|
| A | `a_lemma.patch` | the lemma `strlen_kept`: a string keeps its length where its bytes are kept |
| B | `b_nocast.patch` | `file`'s and `func`'s separation from `*d` written as `\separated(..., d)`, without the casts |
| C | `c_lemma_kept.patch` | A, and two assertions at the end of `diag_push` that the bytes of `file` and `func` are as at the call |
| D | `d_all.patch` | C and B |

And `p_caller.c`, a program proved with WP alone that pushes two frames
and renders the chain, the use VERIFY-066 found the contract did not allow.

WP runs only in CI, and the units that read `diag.h` are enforced against
their pins. `diagpush_probe.py` applies a patch to a copy of the checkout
and measures there what the change's own run will be compared with:

| step | what | as |
|------|------|----|
| `variants` | `diag_push`'s goals, and its lemma's, with P0 and each variant; for P0, WP's text of `push_frame_strings`' goals | `5 · Proofs: semantics/`, one function |
| `caller` | `p_caller.c`'s goals with each variant, with `-wp-literals`; with P0 also without | a program that uses `diag.h` |
| `wp` | `frama-c-diag`'s command: its summary and every goal not proved | `5 · Proofs: semantics/` |
| `agree` | WP with each prover alone; the goals one or two provers prove, as `tools/agreement/pins/frama-c-diag.txt` would list them | `13 · Prover agreement` |
| `eva` | `tools/eva/programs/diag.c` at the ten machine models, each against its pin | `14 · Eva` |

`wp`, `agree` and `eva` measure `diag_push.patch`. The file `steps` names
the steps that run; a step it does not name posts that it did not run.

It runs in `9 · Probes` (`probes.yml`, `diagpush-probe`), report-only: it
fails only if a patch neither applies nor is found applied, or a step
gives no data. `agreement.py` and `eva.py` are enforced in 13 and 14, so a
difference from their pins, which is what this probe measures, is an error
there. GitHub reads a workflow command at the start of a line even behind
spaces (VERIFY-073 run 1), so the probe prints each such line of theirs
behind "| ", and posts its own notices. If a patch is already in the
checkout, it measures the checkout as it is. Its predictions and their
scores are VERIFY-073's, in `docs/deviations.md`.

    python3 tools/probes/verify-073/diagpush_probe.py [variants|caller|wp|agree|eva]...

Each step's files go to `diagpush_out/` in the directory it is run from,
which the job keeps as the artifact `diagpush-probe`: `variants/` and
`caller/`, each WP run's output; `wp_diag_output.txt`;
`agree_frama-c-diag_PROVER.txt` and `.json`, and `frama-c-diag.txt`, the
would-be pin file; `eva_out/MODEL/diag.log`, `.json` and `.csv`.
