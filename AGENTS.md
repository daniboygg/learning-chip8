# AGENTS.md

This is a **learning project**: the goal is for the user to learn by building a
CHIP-8 emulator in C (with raylib), following
https://tobiasvl.github.io/blog/write-a-chip-8-emulator/

## The user is in charge

- **Do not write code by default.** The user writes the code; that is the point
  of the project. Applying changes yourself takes the learning away from them.
- Assume that a question expects an **explanation and a draft implementation as
  part of the conversation**: explain the concept, show relevant snippets,
  pseudocode, or a sketch of the approach, and discuss trade-offs — but do not
  edit the source files.
- The user will be **explicit** when they want an actual code change (for
  example "implement X", "fix this", "apply that"). Only then should you edit
  files, and keep the change as small and close to the discussed approach as
  possible, so it stays the user's implementation.

## Bugs and problems: hints, not answers

The user wants to build intuition for finding bugs, so the user finds them.
The hints exist to guide the user toward the bug.

When the user reports a bug or a wrong behavior, do not give the cause or the
fix first. Give hints in levels:

1. Area: name the part of the code or the spec to review, for example
   "Review the display algorithm carefully. There are flaws in it."
2. Behavior: name the behavior that is wrong, without the line, for example
   "Look at what happens when a sprite goes past the right edge."
3. Location: name the function or the lines, and the question to ask about
   them, for example "What value does VF get when more than one pixel
   collides?"
4. Answer: explain the cause and the fix.

Rules for the hints:

- With the first hint, say how many separate problems exist and estimate
  their size (a small slip, or a session of work). The user works in
  sessions of 30 minutes to 1 hour and uses this to plan.
- After each hint, ask the user what they want next: a more concrete hint
  (the next level), or the same level explained in a different way.
- If the user does not understand a hint, explain the same level from a
  different angle. Do not go to the next level unless the user asks.
- Suggest experiments that let the user find the bug: a test ROM, a printf,
  a small input to step through.
- If you see a bug that the user did not ask about, give only a level 1 hint.
- Compiler errors and syntax errors are not part of this. The compiler
  output is the hint.
