## Authorship Model

This project does not have a "human-authored part" and an "AI-authored
part" that can be cleanly separated by file or by module. AI assistance
was used to produce text across essentially the entire codebase,
including the C implementation, the ACSL annotations, the test code,
the deviation arguments, and significant portions of the documentation.

What is human-authored is the judgment exercised throughout:

- The decision of what belongs in the library and what does not
- The architectural layering and the rationale for it
- The API surface and its contracts
- The verification approach and what counts as sufficient evidence
- Decisions about which AI-produced output is kept, modified, or
  rejected
- Final accountability for every line in the repository

AI-produced text is in the repository only because the human author
reviewed it, judged it correct, and chose to include it. Where AI
output was incorrect, unclear, or did not match the intended design,
it was modified or replaced. The author is responsible for the
contents of the codebase regardless of how any specific line was
first produced.

This is closer to a dictation-with-revision model than to a
generation-and-acceptance model: the AI produces text under
direction, and the author retains editorial and engineering
control over what becomes part of the project.
