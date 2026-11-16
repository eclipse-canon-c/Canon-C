# MetAcsl: the library's rules across its functions

A contract speaks of one function. Some of the library's rules speak of
all of them, and no contract states them. MetAcsl, a Frama-C plug-in,
takes such a rule as a meta-property: a predicate, the functions it
applies to (`\targets`), and a context, the points of each function where
it must hold (`\context`). It writes the predicate as a check at each such
point, and WP proves the checks (VERIFY-086, `docs/deviations.md`).

| rule | file | where the check is | what it says | where the rule is stated |
|------|------|--------------------|--------------|--------------------------|
| `handler_written` | `rules/handler_written.h` | each write of every function but `contract_set_handler`, and each call of a function without a body, for each location its `assigns` names | the location written is not `canon_contract_handler` | `core/primitives/contract.h`: the handler is "the library's ONLY global mutable object", written "through contract_set_handler() only" |
| `handler_kept` | `rules/handler_kept.h` | the return of every function but `contract_set_handler` | the handler is the one the function was called with | the same |
| `heap_raw` | `rules/heap_raw.h` | each call of every function but `mem_alloc` and `mem_free` | the function called is not `malloc`, `calloc`, `realloc` or `free` | README: arena.h and pool.h "never call `malloc`"; "No hidden allocations" |
| `heap_wrappers` | `rules/heap_wrappers.h` | each call of every function but `mem_alloc_array_checked`, `vec_int_alloc` and `vec_int_free` | the function called is not `mem_alloc`, `mem_free` or `mem_alloc_array_checked` | the same; `data/vec/vec.h`'s heap strategy |

The two handler rules state one rule twice. `handler_written` takes the
handler's address, and with it WP must show each write apart from the
handler; `handler_kept` does not, so that WP, as no code of the library
takes the address either, can keep the handler out of memory. They run
apart. Where a unit does not include `contract.h` there is no handler,
and neither runs.

The heap rules name `vec_int_alloc` and `vec_int_free`, vec's heap
constructor and destructor as the proof units instantiate them. A name a
unit does not have is ignored in `\targets` (MetAcsl's warning
`unknown-func`, made inactive), and `\tguard` makes a comparison with a
function the unit does not declare true.

**What a check does not cover.** At a call through a function pointer,
MetAcsl's `\writing` context assumes that the callee writes nothing it
must check, and says so; the `\calling` context compares the pointer with
the functions named, which WP cannot decide when the pointer is a
parameter. The rules speak of the library's code in the proof
configuration (`-DCANON_NO_REQUIRE -DNDEBUG`), as the units do.

## The runs

`metacsl.py run PART` takes each unit of `tools/agreement/agreement.py`'s
`UNITS` in the part (core, semantics or data, as the proof workflows
group them) and runs it three times: the two heap rules, `handler_kept`,
`handler_written`. Each run is a C file of its own,
`metacsl_out/UNIT/RUN/unit.c`, that includes the unit's file and then the
run's rules, preprocessed with the unit's options. Then:

    frama-c -cpp-extra-args=CPP unit.c EXTRA -meta -meta-checks -meta-list-targets
            -meta-warn-key unknown-func=inactive [META_EXTRA] -then-last
            -wp -wp-model MODEL -wp-prop=@check -wp-prover alt-ergo,z3,cvc5 -wp-timeout 120
            -then-last -report-untried -report-csv report.csv

MetAcsl writes checks (`-meta-checks`), and WP proves the checks alone: no
unit has checks of its own. The timeout is the proof jobs' (since run 1b;
10 s in run 1). Report's table gives each its status; the run's directory
keeps the rules' rows (`checks.csv`) and Frama-C's log,
`metacsl_out/summary_PART.json` the counts.

A check is proved when Report gives it "Valid", or "Partially proven":
proved, with properties of the unit the run does not prove, its loop
invariants and its callees' contracts, which the unit's proof job proves.

`diag` takes `-meta-no-check-ext` (`META_EXTRA`). Its proof needs
`-variadic-no-translation`, so that its calls of the trusted `snprintf`
and `fprintf` keep their variable arguments (VERIFY-017's "Trusted stdio
axioms"); at a call of a function without a body, MetAcsl's `\writing`
context binds the formals to the arguments, and stops on more arguments
than formals (run 1: `Invalid_argument("List.iter2")`). Without the
option's check, diag's own writes are checked, and not those its stdio
calls make, which by the axioms are the buffer they are given.

**The pins.** `pins.txt` names, unit by unit and rule by rule, the
functions whose checks WP does not prove, and why: each calls through a
pointer its caller passed (`region_end` a cleanup hook, which may be
`free`; option's and result's combinators and `vec_int_to_stringbuf_cb`
the function they are given, which may call `contract_set_handler`). The
library's own code calls neither. Each part posts a notice for each rule,
its checks by unit and how many are proved, and one for the part with the
differences from the pins. Report-only in runs 1 and 1b (`ENFORCE`): a
part fails only when a run gives no table or is stopped (60 minutes a
run; no run starts past 240 minutes of a part); enforced, a difference
fails it too.

    python3 tools/metacsl/metacsl.py check
    python3 tools/metacsl/metacsl.py run PART [UNIT...]

`15 · MetAcsl` runs the three parts (`.github/workflows/metacsl.yml`),
each installing frama-c-metacsl 0.7, MetAcsl's version for Frama-C 29, in
the Frama-C setup's switch (with `--assume-depexts`, as the setup), and
removing it at the end. Frama-C 25 has no
MetAcsl, and rejects a `meta` annotation.
