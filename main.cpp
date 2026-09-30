#include "editor_state.hpp"
#include "terminal.hpp"

#include <csignal>
#include <iostream>
#include <string>

namespace {

volatile std::sig_atomic_t interrupted = 0;

void requestExit(int) {
  interrupted = 1;
}

bool renderEditor(const EditorState& state, std::string& error) {
  TerminalSize size{};
  if (!getTerminalSize(size, error)) {
    return false;
  }

  const RenderLayout layout = makeRenderLayout(state.text, state.cursor, size);
  std::string output = "\033[2J\033[H";
  for (const std::string& line : layout.lines) {
    output += line;
    output += "\033[K\n";
  }
  output += "\033[" + std::to_string(size.rows) + ";1HCtrl-Q: quit\033[K";
  output += "\033[" + std::to_string(layout.cursorRow + 1) + ";" +
            std::to_string(layout.cursorColumn + 1) + "H";
  return writeAll(output, error);
}

int runEditor(std::string& error) {
  RawTerminal terminal;
  if (!terminal.enable(error)) {
    return 1;
  }

  EditorState state{"", 0};
  int status = 0;
  bool quitRequested = false;
  while (interrupted == 0) {
    if (!renderEditor(state, error)) {
      status = 1;
      break;
    }

    KeyEvent event{};
    const ReadStatus inputStatus = readKeyEvent(event, error);
    if (inputStatus != ReadStatus::Event) {
      if (interrupted != 0) {
        error = "editor interrupted";
      } else if (error.empty()) {
        error = "terminal input closed";
      }
      status = 1;
      break;
    }

    switch (event.type) {
      case KeyType::Insert:
        insertCharacter(state, event.character);
        break;
      case KeyType::Newline:
        insertCharacter(state, '\n');
        break;
      case KeyType::Backspace:
        eraseBeforeCursor(state);
        break;
      case KeyType::Left:
        moveCursorLeft(state);
        break;
      case KeyType::Right:
        moveCursorRight(state);
        break;
      case KeyType::Quit:
        quitRequested = true;
        interrupted = 1;
        break;
      case KeyType::Ignore:
        break;
    }
  }

  if (status == 0 && interrupted != 0 && !quitRequested && error.empty()) {
    error = "editor interrupted";
    status = 1;
  }

  std::string restoreError;
  if (!terminal.restore(restoreError)) {
    error = "could not restore terminal: " + restoreError;
    status = 1;
  }
  return status;
}

}  // namespace

int main() {
  if (!isInteractiveTerminal()) {
    std::cerr << "editor requires interactive stdin and stdout\n";
    return 1;
  }

  std::signal(SIGINT, requestExit);
  std::signal(SIGTERM, requestExit);
  std::signal(SIGHUP, requestExit);
  std::signal(SIGPIPE, SIG_IGN);

  std::string error;
  const int status = runEditor(error);
  if (status != 0) {
    std::cerr << "editor: " << error << '\n';
  }
  return status;
}
