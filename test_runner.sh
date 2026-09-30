#!/bin/bash

set -euo pipefail

g++ -std=c++17 -Wall -Wextra -Werror editor_state.cpp terminal.cpp \
  tests/editor_state_test.cpp -o editor_tests
./editor_tests
