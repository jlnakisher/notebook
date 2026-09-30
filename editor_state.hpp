#pragma once

#include <cstddef>
#include <string>

struct EditorState {
  std::string text;
  std::size_t cursor;
};

void insertCharacter(EditorState& state, char character);
void moveCursorLeft(EditorState& state);
void moveCursorRight(EditorState& state);
void eraseBeforeCursor(EditorState& state);
