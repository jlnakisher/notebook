#include "terminal.hpp"

#include <cerrno>
#include <cstring>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace {

constexpr int kEscapeTimeoutMilliseconds = 100;
constexpr std::size_t kMaxEscapeBytes = 16;

bool waitForInput(int timeoutMilliseconds, short& events, std::string& error) {
  pollfd descriptor{};
  descriptor.fd = STDIN_FILENO;
  descriptor.events = POLLIN;

  const int result = poll(&descriptor, 1, timeoutMilliseconds);
  if (result > 0) {
    events = descriptor.revents;
    return true;
  }
  if (result == 0) {
    events = 0;
    return true;
  }
  error = std::strerror(errno);
  return false;
}

bool readOneByte(char& byte, std::string& error) {
  const ssize_t result = read(STDIN_FILENO, &byte, 1);
  if (result == 1) {
    return true;
  }
  error = result == 0 ? "standard input closed" : std::strerror(errno);
  return false;
}

std::vector<std::string> splitLines(const std::string& text) {
  std::vector<std::string> lines;
  std::string line;
  for (char character : text) {
    if (character == '\n') {
      lines.push_back(line);
      line.clear();
    } else {
      line.push_back(character);
    }
  }
  lines.push_back(line);
  return lines;
}

void findCursor(const std::string& text, std::size_t cursor, std::size_t& line,
                std::size_t& column) {
  line = 0;
  column = 0;
  for (std::size_t index = 0; index < cursor; ++index) {
    if (text[index] == '\n') {
      ++line;
      column = 0;
    } else {
      ++column;
    }
  }
}

}  // namespace

KeyEvent decodeKeyBytes(const std::string& bytes) {
  if (bytes == "\033[C") {
    return {KeyType::Right, '\0'};
  }
  if (bytes == "\033[D") {
    return {KeyType::Left, '\0'};
  }
  if (bytes.size() != 1) {
    return {};
  }

  const unsigned char byte = static_cast<unsigned char>(bytes.front());
  if (byte >= 0x20 && byte <= 0x7e) {
    return {KeyType::Insert, static_cast<char>(byte)};
  }
  if (byte == '\r' || byte == '\n') {
    return {KeyType::Newline, '\0'};
  }
  if (byte == '\b' || byte == 0x7f) {
    return {KeyType::Backspace, '\0'};
  }
  if (byte == 0x11) {
    return {KeyType::Quit, '\0'};
  }
  return {};
}

bool isEscapeSequenceComplete(const std::string& bytes) {
  if (bytes.size() < 3 || bytes[0] != '\033' || bytes[1] != '[') {
    return false;
  }
  const unsigned char byte = static_cast<unsigned char>(bytes.back());
  return byte >= '@' && byte <= '~';
}

RenderLayout makeRenderLayout(const std::string& text, std::size_t cursor,
                              TerminalSize size) {
  const std::vector<std::string> allLines = splitLines(text);
  std::size_t cursorLine = 0;
  std::size_t cursorColumn = 0;
  findCursor(text, cursor, cursorLine, cursorColumn);

  const std::size_t contentRows = size.rows - 1;
  const std::size_t topLine = cursorLine >= contentRows
                                  ? cursorLine - contentRows + 1
                                  : 0;
  const std::size_t leftColumn = cursorColumn >= size.columns
                                     ? cursorColumn - size.columns + 1
                                     : 0;

  RenderLayout layout{topLine,
                      leftColumn,
                      cursorLine - topLine,
                      cursorColumn - leftColumn,
                      {}};
  for (std::size_t row = 0; row < contentRows; ++row) {
    const std::size_t lineIndex = topLine + row;
    if (lineIndex >= allLines.size()) {
      layout.lines.emplace_back();
      continue;
    }

    const std::string& line = allLines[lineIndex];
    if (leftColumn >= line.size()) {
      layout.lines.emplace_back();
    } else {
      layout.lines.push_back(line.substr(leftColumn, size.columns));
    }
  }
  return layout;
}

bool isInteractiveTerminal() {
  return isatty(STDIN_FILENO) != 0 && isatty(STDOUT_FILENO) != 0;
}

bool getTerminalSize(TerminalSize& size, std::string& error) {
  winsize window{};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) != 0) {
    error = std::strerror(errno);
    return false;
  }
  if (window.ws_row < 2 || window.ws_col == 0) {
    error = "terminal must have at least two rows and one column";
    return false;
  }
  size = {window.ws_row, window.ws_col};
  return true;
}

bool writeAll(const std::string& output, std::string& error) {
  std::size_t written = 0;
  while (written < output.size()) {
    const ssize_t result = write(STDOUT_FILENO, output.data() + written,
                                 output.size() - written);
    if (result > 0) {
      written += static_cast<std::size_t>(result);
      continue;
    }
    if (result < 0 && errno == EINTR) {
      continue;
    }
    error = result == 0 ? "short terminal write" : std::strerror(errno);
    return false;
  }
  return true;
}

ReadStatus readKeyEvent(KeyEvent& event, std::string& error) {
  short events = 0;
  if (!waitForInput(-1, events, error)) {
    return ReadStatus::Error;
  }
  if ((events & (POLLERR | POLLNVAL)) != 0 ||
      ((events & POLLHUP) != 0 && (events & POLLIN) == 0)) {
    error = "standard input closed";
    return ReadStatus::Closed;
  }

  char firstByte = '\0';
  if (!readOneByte(firstByte, error)) {
    return ReadStatus::Closed;
  }

  std::string bytes(1, firstByte);
  if (firstByte == '\033') {
    for (std::size_t index = 1; index < kMaxEscapeBytes; ++index) {
      if (!waitForInput(kEscapeTimeoutMilliseconds, events, error)) {
        return ReadStatus::Error;
      }
      if ((events & POLLIN) == 0) {
        break;
      }
      char byte = '\0';
      if (!readOneByte(byte, error)) {
        return ReadStatus::Closed;
      }
      bytes.push_back(byte);
      if (isEscapeSequenceComplete(bytes)) {
        break;
      }
    }
  }

  event = decodeKeyBytes(bytes);
  return ReadStatus::Event;
}

RawTerminal::RawTerminal() : enabled_(false), original_{} {}

RawTerminal::~RawTerminal() {
  std::string ignoredError;
  restore(ignoredError);
}

bool RawTerminal::enable(std::string& error) {
  if (enabled_) {
    return true;
  }
  if (tcgetattr(STDIN_FILENO, &original_) != 0) {
    error = std::strerror(errno);
    return false;
  }

  termios raw = original_;
  cfmakeraw(&raw);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
    error = std::strerror(errno);
    return false;
  }
  enabled_ = true;
  return true;
}

bool RawTerminal::restore(std::string& error) {
  if (!enabled_) {
    return true;
  }
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_) != 0) {
    error = std::strerror(errno);
    return false;
  }
  enabled_ = false;
  return true;
}
