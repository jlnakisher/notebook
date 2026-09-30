#include "../editor_state.hpp"
#include "../terminal.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void expectEqual(const std::string& actual, const std::string& expected,
                 const char* description) {
  if (actual != expected) {
    std::cerr << "FAILED: " << description << "\nexpected: " << expected
              << "\nactual: " << actual << '\n';
    std::exit(1);
  }
}

void expectEqual(std::size_t actual, std::size_t expected,
                 const char* description) {
  if (actual != expected) {
    std::cerr << "FAILED: " << description << "\nexpected: " << expected
              << "\nactual: " << actual << '\n';
    std::exit(1);
  }
}

void testInsertAtCursor() {
  EditorState state{"ac", 1};
  insertCharacter(state, 'b');

  expectEqual(state.text, "abc", "inserts in the middle of text");
  expectEqual(state.cursor, 2, "advances cursor after insertion");
}

void testCursorBoundaries() {
  EditorState state{"a", 0};
  moveCursorLeft(state);
  expectEqual(state.cursor, 0, "does not move left of document start");

  moveCursorRight(state);
  moveCursorRight(state);
  expectEqual(state.cursor, 1, "does not move right of document end");
}

void testBackspace() {
  EditorState state{"abc", 2};
  eraseBeforeCursor(state);
  expectEqual(state.text, "ac", "deletes preceding character");
  expectEqual(state.cursor, 1, "moves cursor left after deletion");

  state.cursor = 0;
  eraseBeforeCursor(state);
  expectEqual(state.text, "ac", "backspace at start is a no-op");
}

void testNewlineIsInsertion() {
  EditorState state{"ab", 1};
  insertCharacter(state, '\n');
  expectEqual(state.text, "a\nb", "inserts newline at cursor");
  expectEqual(state.cursor, 2, "advances cursor after newline");
}

void testKeyDecoding() {
  expectEqual(static_cast<std::size_t>(decodeKeyBytes("x").type),
              static_cast<std::size_t>(KeyType::Insert),
              "decodes printable ASCII as insertion");
  expectEqual(static_cast<std::size_t>(decodeKeyBytes("\r").type),
              static_cast<std::size_t>(KeyType::Newline),
              "decodes carriage return as newline");
  expectEqual(static_cast<std::size_t>(decodeKeyBytes("\n").type),
              static_cast<std::size_t>(KeyType::Newline),
              "decodes line feed as newline");
  expectEqual(static_cast<std::size_t>(decodeKeyBytes("\b").type),
              static_cast<std::size_t>(KeyType::Backspace),
              "decodes backspace as backspace");
  expectEqual(static_cast<std::size_t>(decodeKeyBytes("\177").type),
              static_cast<std::size_t>(KeyType::Backspace),
              "decodes delete as backspace");
  expectEqual(static_cast<std::size_t>(decodeKeyBytes("\033[D").type),
              static_cast<std::size_t>(KeyType::Left),
              "decodes left arrow sequence");
  expectEqual(static_cast<std::size_t>(decodeKeyBytes("\033[C").type),
              static_cast<std::size_t>(KeyType::Right),
              "decodes right arrow sequence");
  expectEqual(static_cast<std::size_t>(decodeKeyBytes("\021").type),
              static_cast<std::size_t>(KeyType::Quit),
              "decodes Ctrl-Q as quit");
  expectEqual(static_cast<std::size_t>(decodeKeyBytes("\001").type),
              static_cast<std::size_t>(KeyType::Ignore),
              "ignores unrelated control bytes");
  expectEqual(static_cast<std::size_t>(decodeKeyBytes("\033[").type),
              static_cast<std::size_t>(KeyType::Ignore),
              "ignores incomplete escape sequence");
  expectEqual(isEscapeSequenceComplete("\033["), false,
              "does not finish after CSI introducer");
  expectEqual(isEscapeSequenceComplete("\033[D"), true,
              "finishes after CSI final byte");
}

void testRenderLayout() {
  const RenderLayout layout = makeRenderLayout("abc\ndefgh", 7, {3, 3});
  expectEqual(layout.topLine, 0, "keeps cursor line in a two-row viewport");
  expectEqual(layout.leftColumn, 1, "horizontally scrolls long cursor line");
  expectEqual(layout.cursorRow, 1, "places cursor on second visible row");
  expectEqual(layout.cursorColumn, 2, "places cursor inside clipped line");
  expectEqual(layout.lines.at(1), "efg", "clips long lines without wrapping");
}

}  // namespace

int main() {
  testInsertAtCursor();
  testCursorBoundaries();
  testBackspace();
  testNewlineIsInsertion();
  testKeyDecoding();
  testRenderLayout();
  return 0;
}
