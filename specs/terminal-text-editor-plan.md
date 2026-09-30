# Implementation Plan: Terminal Text Editor

## Overview

Implement a small in-memory, terminal-based plain-text editor in the existing C++ program. The editor will receive keystrokes directly in its main screen, insert text at a tracked cursor, redraw after each edit, and restore normal terminal behavior when it exits.

## Architecture Decisions

- Model the document as an ASCII `std::string` plus a zero-based cursor offset. This makes insertion behavior explicit and avoids claiming Unicode cursor support.
- Put document operations in `editor_state.cpp`; keep key decoding and render-layout calculations pure where possible so tests do not need an interactive terminal.
- Use POSIX raw mode with `VMIN = 0` and a positive `VTIME` to place a finite bound on reads after Escape. Require both standard input and output to be TTYs before changing terminal state.
- Render a clipped, non-wrapping viewport: derive terminal rows/columns with `TIOCGWINSZ`, reserve the last row for `Ctrl-Q: quit`, select vertical/horizontal offsets that keep the document cursor visible, and move the hardware cursor to its calculated screen coordinates.
- Preserve original `termios` in an idempotent RAII guard. Signal handlers for `SIGINT`, `SIGTERM`, and `SIGHUP` only set a `sig_atomic_t` exit flag; normal control flow restores the terminal. The program does not enter the alternate screen or hide the cursor.
- Use `Ctrl-Q` (`0x11`) to exit. File saving is deliberately excluded.

## Dependency Graph

```text
EditorState + cursor operations <--- unit tests
          |
terminal decoder + render layout <--- unit tests
          |
terminal raw-mode lifecycle
          |
          +--> main input, dispatch, and redraw loop
```

## Task List

### Task 1: Implement testable document and cursor operations

**Description:** Add an independent editor-state module and a non-interactive test runner. Keep the application entry point out of test builds.

**Acceptance criteria:**

- [ ] Inserting a printable character places it at `cursor` and advances the cursor by one.
- [ ] Enter uses the same insertion mechanism with `\n`.
- [ ] Cursor movement never goes below zero or past `text.size()`.
- [ ] Backspace is a no-op at cursor zero and otherwise removes the preceding character and moves the cursor left.
- [ ] The test executable has its own `main` and does not compile or start the interactive editor loop.

**Verification:**

- [ ] `./test_runner.sh` runs `g++ -std=c++17 -Wall -Wextra -Werror editor_state.cpp terminal.cpp tests/editor_state_test.cpp -o editor_tests` followed by `./editor_tests`, and never launches `./app`.
- [ ] Tests cover empty text, beginning/middle/end insertion, cursor boundaries, and backspace boundaries.
- [ ] `g++ -std=c++17 -Wall -Wextra -Werror main.cpp editor_state.cpp terminal.cpp -o app` succeeds after all production modules exist.

**Dependencies:** None

**Files likely touched:**

- `main.cpp`
- `editor_state.hpp`
- `editor_state.cpp`
- `tests/editor_state_test.cpp`
- `test_runner.sh`

**Estimated scope:** Small

### Task 2: Add safe terminal input and rendering

**Description:** Add terminal lifecycle and deterministic helpers for input decoding and render layout. Enter raw mode only after validating terminal capabilities, then render one bounded editor surface safely.

**Acceptance criteria:**

- [ ] Non-TTY stdin or stdout produces a diagnostic and nonzero exit before `tcsetattr`.
- [ ] Raw mode uses a saved `termios`, `VMIN = 0`, and a positive `VTIME`; its guard restores the saved state exactly once on every ordinary error/exit path.
- [ ] `SIGINT`, `SIGTERM`, and `SIGHUP` request normal-loop shutdown via an exit flag; `SIGKILL` is explicitly outside the guarantee.
- [ ] Decoder maps CR/LF to newline, BS/DEL to backspace, `ESC [ C`/`ESC [ D` to cursor movement, and `Ctrl-Q` to quit; all other control bytes and incomplete/unknown sequences are ignored after no more than a fixed number of timed reads.
- [ ] Rendering obtains nonzero dimensions, clears and homes the screen, clips long lines rather than wrapping them, reserves a status row, and calculates a visible cursor row/column.
- [ ] EOF, unrecoverable read errors, and partial/failed writes produce nonzero exit and still run terminal restoration.

**Verification:**

- [ ] Unit tests cover supported/unsupported byte mapping, incomplete escape sequences, cursor coordinates, and clipping offsets.
- [ ] Compile command succeeds with warnings treated as errors.
- [ ] Manual check: launch, exit immediately, and confirm the shell remains usable.

**Dependencies:** Task 1

**Files likely touched:**

- `main.cpp`
- `terminal.hpp`
- `terminal.cpp`
- `tests/editor_state_test.cpp`

**Estimated scope:** Small

### Task 3: Connect keystrokes to editor behavior

**Description:** Connect decoded key events to editor operations, redraw after each event, and make every normal and error exit observable and safe.

**Acceptance criteria:**

- [ ] Typing `abc`, moving left once, then typing `X` yields `abXc`.
- [ ] Enter inserts a line break at the cursor.
- [ ] Arrow and Backspace behavior meet the feature specification.
- [ ] CR Enter, LF Enter, BS, and DEL behavior meet the feature specification.
- [ ] Ctrl-Q exits and returns the user to a normally functioning terminal; the status row documents the key chord.
- [ ] Unknown control bytes and incomplete/unknown escape sequences neither insert text nor stall the editor.

**Verification:**

- [ ] Run `./test_runner.sh` successfully.
- [ ] Manual check: complete the `abXc` flow, CR and LF multiline flows, both Backspace byte variants at the start, incomplete/unknown Escape input, a long-line viewport flow, non-TTY rejection, and Ctrl-Q exit.

**Dependencies:** Tasks 1–2

**Files likely touched:**

- `main.cpp`
- `terminal.cpp`
- `tests/editor_state_test.cpp`

**Estimated scope:** Small

## Checkpoint: Core editor complete

- [ ] Automated document-operation tests pass.
- [ ] Decoder and render-layout tests pass without a TTY.
- [ ] Compilation succeeds with `-Wall -Wextra -Werror`.
- [ ] The manual text-entry, insertion, navigation, deletion, viewport, non-TTY-rejection, and clean-exit flows pass in an interactive terminal.

## Risks and Mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Raw mode leaves the terminal unusable after an error or signal | High | Validate TTYs first; use idempotent RAII restoration and signal-to-exit-flag handling. Document the unavoidable `SIGKILL` limitation. |
| Escape sequences from arrow keys are partial or unknown | High | Use bounded timed reads after Escape; recognize only left/right CSI sequences and consume/ignore the remainder. |
| Rendering loses the cursor on long or multiline content | High | Define a non-wrapping viewport and test cursor-coordinate and clipping calculations as pure functions. |
| Interactive behavior is hard to automate | Medium | Unit-test state, decoding, and layout; reserve actual raw-mode lifecycle for a pseudo-TTY/manual smoke checklist. |

## Explicitly Out of Scope

- Opening, saving, or autosaving files
- Mouse input, selection, copy/paste, undo/redo
- Rich-text formatting
- GUI/windowed rendering
