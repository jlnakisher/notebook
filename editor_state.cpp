#include "editor_state.hpp"

void insertCharacter(EditorState& state, char character) {
  state.text.insert(state.cursor, 1, character);
  ++state.cursor;
}

void moveCursorLeft(EditorState& state) {
  if (state.cursor > 0) {
    --state.cursor;
  }
}

void moveCursorRight(EditorState& state) {
  if (state.cursor < state.text.size()) {
    ++state.cursor;
  }
}

void eraseBeforeCursor(EditorState& state) {
  if (state.cursor == 0) {
    return;
  }

  state.text.erase(state.cursor - 1, 1);
  --state.cursor;
}
