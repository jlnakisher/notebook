# Spec: Terminal Text Editor

## Objective

Create a minimal terminal-based Notepad clone in which a user enters text directly in the main editor. Printable characters are inserted at the cursor in an in-memory plain-text document. This establishes the editor’s core text-entry interaction; file open/save and rich-text behavior are not included.

## Tech Stack

- C++ (existing project)
- POSIX terminal APIs (`termios`, `ioctl`, and `unistd.h`) for character-at-a-time keyboard input
- No new third-party dependencies
- ASCII-only text input for v1; UTF-8 and locale-aware editing are out of scope

## Commands

```bash
./test_runner.sh
g++ -std=c++17 -Wall -Wextra -Werror main.cpp editor_state.cpp terminal.cpp -o app
./app
```

`test_runner.sh` must run the equivalent of:

```bash
g++ -std=c++17 -Wall -Wextra -Werror editor_state.cpp terminal.cpp tests/editor_state_test.cpp -o editor_tests
./editor_tests
```

## Project Structure

- `main.cpp` — application startup and input/render loop
- `editor_state.hpp` / `editor_state.cpp` — document, cursor, and pure editing operations
- `terminal.hpp` / `terminal.cpp` — raw-mode lifecycle, key decoding, dimensions, and terminal output
- `test_runner.sh` — compile and run only non-interactive tests
- `tests/editor_state_test.cpp` — document/cursor and decoder/renderer unit tests
- `specs/` — feature specifications and implementation plans

## Code Style

Use small, single-purpose C++ functions, descriptive `camelCase` names, and keep terminal I/O separate from document-editing state where practical.

```cpp
void insertCharacter(EditorState& state, char character) {
  state.text.insert(state.cursor, 1, character);
  ++state.cursor;
}
```

## Testing Strategy

- `test_runner.sh` must compile a separate `editor_tests` binary using `-std=c++17 -Wall -Wextra -Werror` and run it without launching `./app`.
- Unit-test document/cursor operations and pure key-decoder and render-layout helpers independently of terminal raw-mode I/O.
- Manually verify keyboard input in an interactive pseudo-terminal or shell.
- Compile with warnings enabled before review.

## Boundaries

- Always: restore terminal settings on every supported exit path; keep cursor position within the document; compile with warnings enabled; check all terminal I/O errors.
- Ask first: add a GUI framework or other dependency; add file persistence; change build/CI configuration.
- Never: store rich-text formatting; commit compiled binaries; leave the user’s terminal in raw mode after exit.

## Success Criteria

- The application refuses to start unless both stdin and stdout are TTYs, prints a concise diagnostic to stderr, and exits nonzero without changing terminal state otherwise.
- The application presents one main editing surface in a terminal with a non-document status hint, `Ctrl-Q: quit`.
- Only ASCII bytes `0x20` through `0x7e` are inserted as text; other unrecognized control bytes are ignored.
- Typing an accepted printable byte inserts it at the current cursor position.
- Either carriage return (`0x0d`) or line feed (`0x0a`) inserts a newline at the cursor.
- Left and right movement changes the insertion position without modifying text.
- Either backspace (`0x08`) or delete (`0x7f`) deletes the character immediately before the cursor when one exists.
- `ESC [ D` and `ESC [ C` map to left and right. Incomplete or unknown escape sequences are ignored after bounded timed reads and never become document text.
- Input uses a bounded raw-mode read configuration (`VMIN = 0`, `VTIME > 0`) so EOF, interruptions, and incomplete escape sequences cannot block indefinitely.
- Rendering uses terminal dimensions, clears and homes the editor surface on each redraw, uses a defined vertical and horizontal viewport without line wrapping, and positions the hardware cursor at the document insertion position. Missing or zero dimensions are reported as an error with a nonzero exit.
- The user exits with `Ctrl-Q` (`0x11`). EOF, unrecoverable read/write errors, and supported termination signals also exit through the restoration path with a nonzero status.
- Original `termios` settings are preserved before raw mode and restored idempotently by RAII. `SIGINT`, `SIGTERM`, and `SIGHUP` set an exit flag that the input loop observes; `SIGKILL` cannot be handled. The application does not change cursor visibility or alternate-screen state.
- Document contents exist for the current process only; no files are created or modified.

## Open Questions

None for the initial in-memory terminal editor.
