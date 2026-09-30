#pragma once

#include <cstddef>
#include <string>
#include <termios.h>
#include <vector>

enum class KeyType {
  Ignore,
  Insert,
  Newline,
  Backspace,
  Left,
  Right,
  Quit,
};

struct KeyEvent {
  KeyType type = KeyType::Ignore;
  char character = '\0';
};

struct TerminalSize {
  std::size_t rows;
  std::size_t columns;
};

struct RenderLayout {
  std::size_t topLine;
  std::size_t leftColumn;
  std::size_t cursorRow;
  std::size_t cursorColumn;
  std::vector<std::string> lines;
};

enum class ReadStatus { Event, Closed, Error };

KeyEvent decodeKeyBytes(const std::string& bytes);
bool isEscapeSequenceComplete(const std::string& bytes);
RenderLayout makeRenderLayout(const std::string& text, std::size_t cursor,
                              TerminalSize size);

bool isInteractiveTerminal();
bool getTerminalSize(TerminalSize& size, std::string& error);
bool writeAll(const std::string& output, std::string& error);
ReadStatus readKeyEvent(KeyEvent& event, std::string& error);

class RawTerminal {
 public:
  RawTerminal();
  ~RawTerminal();

  RawTerminal(const RawTerminal&) = delete;
  RawTerminal& operator=(const RawTerminal&) = delete;

  bool enable(std::string& error);
  bool restore(std::string& error);

 private:
  bool enabled_;
  termios original_;
};
