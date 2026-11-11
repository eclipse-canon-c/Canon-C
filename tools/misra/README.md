# The misra job's pins

The `misra` job (`3 · Static analysis & coverage`, `analysis.yml`) runs
cppcheck's MISRA C:2012 addon on the library's headers. Its real findings
are those outside `canary/`, whose three intentional violations test the
instrument. Since VERIFY-068, each real finding is argued in
`docs/deviations.md`: a deviation (MISRA-DEV-nnn), or a finding of
cppcheck's that is not a violation (MISRA-TOOL-nnn). `pins.txt` holds them
by file and rule: how many, and the record that argues them.

    python3 tools/misra/misra.py report REPORT   the job's report against the pins
    python3 tools/misra/misra.py pin REPORT      write pins.txt from a report
    python3 tools/misra/misra.py check           each pin names a record that exists

The job's last step runs `report` on its `misra_report.txt` and posts the
result as a notice: report-only in VERIFY-068's run 2; from its run 3, a
difference fails the job. `8 · Records` runs `check`.

A finding is pinned by file and rule, not by line, so an edit that moves
it keeps its pin. A count changes only with a record that says why: a new
finding is fixed, or argued in a record that its pin names. cppcheck
reports at most one MISRA finding per line, so fixing one can bring up
another at the same line (`docs/misra-campaign.md`); the comparison then
shows it.

The pins are cppcheck 2.13.0's, the version of the job's pinned runner
(ubuntu-24.04). Another version reads other rules, and moving the runner
means re-pinning, with a record.

Not MISRA compliance: cppcheck's addon covers part of the guidelines and
is not a qualified checker (`docs/misra-campaign.md`).
