# Aorai: the library's call-order rules

Some of the library's rules are about the order of calls. A contract
speaks of one call, so it cannot state them. Aorai, a Frama-C plug-in,
takes such a rule written as an automaton in its language, Ya. It
instruments a program with the automaton: a ghost state that each call
updates, and contracts that name the calls each state allows. Eva or WP
then says whether the program keeps the rule. Each file here writes one
rule that the documentation states and no contract states (VERIFY-085,
`docs/deviations.md`):

| automaton | the rule | where it is stated |
|-----------|----------|--------------------|
| `hashmap_iteration.ya` | an iteration does not go on after the map changed | `data/hashmap/hashmap.h`: "Do not mutate the map during iteration" |
| `stringbuf_views.ya` | a view is not read after `stringbuf_close`, until the buffer is initialised again and a view taken from it | `data/stringbuf.h`: `stringbuf_close` "invalidates outstanding borrowed views" |
| `bitset_views.ya` | the same of `bitset_close` | `data/bitset.h` |

Ya has no comments, so the automata are described here.

- **`hashmap_iteration.ya`**, states `idle` (the first), `iterating` and
  `stale`. A call of `hashmap_iter_next` with `*iter == 0` starts an
  iteration (`idle` to `iterating`). In `iterating`, a call of
  `hashmap_init`, `hashmap_insert`, `hashmap_remove` or `hashmap_clear`
  ends it (back to `idle`). In `idle`, a call of `hashmap_iter_next` with
  `*iter != 0`, an iteration going on, leads to `stale`, which nothing
  leaves: the call is refused. An iteration can be left before its end,
  and a new one started from 0, whatever happened in between. The names
  are those of the hashmap instantiated with the prefix `hashmap`
  (`vmacros/vdrivers/hashmap_verify.h`); another prefix renames the five
  functions.
- **`stringbuf_views.ya`**, states `open` (the first), `closed`,
  `reopened` and `stale`. `stringbuf_close` leads from `open` to `closed`.
  In `closed`, an init (`stringbuf_init_arena`, `stringbuf_init_buffer`)
  leads to `reopened`, and in `reopened` a view taken
  (`stringbuf_as_borrowed_str`, `stringbuf_as_borrowed_bytes`,
  `stringbuf_buffer_as_borrowed_bytes`) back to `open`. In `closed` and in
  `reopened`, a read of a view (`borrowed_str_get` or `borrowed_bytes_get`,
  guarded on `b == b`: see below) leads to `stale`: the read is refused. That
  is what a build with `CANON_LIFETIME_DEBUG` checks at run time: the
  close marks the lifetime closed, and the next init draws a new one, so
  that a view taken before the close does not become valid again.
- **`bitset_views.ya`**: the same, with `bitset_close`, `bitset_init`,
  `bitset_as_borrowed_bytes` and `borrowed_bytes_get`.

Every other call leaves the state as it is (`other`).

**What the automata do not know.** They know calls, not objects: an
automaton reads a program's maps as one map and its iterators as one, its
buffers as one buffer and its views as one view. For a program that
keeps one of each at a time it is the rule. For one that keeps more, it
can refuse what the rule allows (a read of one buffer's view after
another buffer's close), and allow what the rule refuses: a view taken
before a close and read after the buffer was initialised again and
another view taken, or an iteration that a mutation ended, continued
after a second iterator started from 0.

**Why a read is guarded on `b == b`.** A rule made of calls alone, Aorai
decides by itself in a program without branches: it finds that no run of
`main` keeps the rule and gives `main` the precondition `\false`, without
saying which call. With the read guarded on a value, Aorai leaves the
check to each read, as a precondition that Eva or WP then decides at that
call: for `stringbuf_closed.c`, `aorai_CurStates ≡ closed ⇒ (b ≢ b ∧ b ≢ b)
∨ b ≢ b`, that is, not in `closed`. The guard is true of every call; it is
there only so that Aorai does not evaluate it. VERIFY-085's run 1 had it
`b != 0`, which Frama-C 25's Aorai reads and 29's refuses, a pointer
compared with an integer ("types borrowed_str const * and ℤ are not
convertible"); Ya has no `\null`. A guard on the view's length, `b->str.len
>= 0`, type-checks, but Aorai joins the two reads' guards by the name of
their parameter, `b` in both, and writes into `borrowed_str_get`'s
precondition `b->bytes.len`, a field `borrowed_str` does not have. `b == b`
reads the same in both.

**What Aorai cannot instrument.** It stops at a call through a function
pointer that has no `calls` clause. `core/region.h`'s `region_end` calls
each hook so, and a library cannot name its callers' hooks, so no rule of
region.h is here. Of `tools/eva/programs/`, Frama-C 25's Aorai
instruments 21 of 27.

## The cases

`aorai.py`'s `CASES` pairs each program with an automaton, and says which
verdict the rule gives it:

| program | automaton | verdict |
|---------|-----------|---------|
| `tools/eva/programs/hashmap.c` | `hashmap_iteration` | follows |
| `programs/hashmap_ok.c` | `hashmap_iteration` | follows |
| `programs/hashmap_stale.c` | `hashmap_iteration` | breaks |
| `tools/eva/programs/stringbuf.c` | `stringbuf_views` | follows |
| `programs/stringbuf_ok.c` | `stringbuf_views` | follows |
| `programs/stringbuf_closed.c` | `stringbuf_views` | breaks |
| `programs/stringbuf_reopened.c` | `stringbuf_views` | breaks |
| `tools/eva/programs/bitset.c` | `bitset_views` | follows |
| `programs/bitset_ok.c` | `bitset_views` | follows |
| `programs/bitset_closed.c` | `bitset_views` | breaks |
| `programs/bitset_reopened.c` | `bitset_views` | breaks |

The three of `tools/eva/programs/` are those that use the rules'
functions. Each program here keeps or breaks its rule in one way, and one
that breaks it marks the call the rule refuses `/* refused */`. Each is
preprocessed as the Eva program it names.

Aorai instruments the program, and Eva runs it with `tools/eva/eva.py`'s
options at x86_64, but for its `-eva-use-spec ptr_align_padding`: from the
specification Eva leaves Aorai's postcondition on that function unknown,
the ghost code that keeps the state being in the body. Report's table
then gives each property its status. Aorai's properties are those that
name its state (`aorai_`). A case **follows** its rule when none of them
is invalid or unknown and the program reaches its `END`; it **breaks** it
when a call in the program violates one of Aorai's preconditions, and
only the marked call. Anything else is "undecided".

    python3 tools/aorai/aorai.py check           CASES against the files
    python3 tools/aorai/aorai.py run [CASE...]   Aorai and Eva on each case
    python3 tools/aorai/aorai.py native          each program compiled and run

`native` compiles each program here with gcc as `eva.py native` compiles
the Eva programs, and runs it, then again with `CANON_LIFETIME_DEBUG` and
the contracts' checks on. A normal build of each returns 0. Built with
the checks, a program that breaks a views rule stops at the read, a
contract violation of `lifetime_assert_valid`, as Aorai says it breaks
the rule; `hashmap_stale.c` returns 0, as no run-time check sees the
iteration rule.

The job `aorai` of `14 · Eva` runs the three (`.github/workflows/eva.yml`):
a case not as `CASES` says, or a program not as `LIFETIME_DEBUG` says,
fails it (`ENFORCE`, since VERIFY-085's run 2). With Frama-C 25,
`EVA_FRAMAC_ARGS='-c11 -kernel-warn-key annot-error=active'`, as for
`eva.py`.

**WP.** VERIFY-085's probe (`tools/probes/verify-085/`, closed) measured
WP on the instrumented programs. On the hashmap programs, which pass a
`u64[64]` to `bytes_from` as 512 bytes, its precondition is false in WP's
typed model, which counts the array in cells of its type, and WP, taking
it as a hypothesis after the call, proves anything after it. With the
buffer declared as bytes, WP leaves unproved the call the rule refuses,
and proves Aorai's other properties but the precondition of the call in
`hashmap_ok.c`'s `while`, whose first turn needs `iter` to be 0, which
Aorai's loop invariants, of the state alone, do not say. The views
programs gave WP no such trouble: it left each refused read unproved, and
proved Aorai's properties of `stringbuf_ok.c` and `bitset_ok.c`.
