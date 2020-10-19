//
// Created by codetector on 10/13/20.
//

#include "TMS5501.h"
#include <iostream>
#include <unistd.h>

uint8_t TMS5501::doIn(uint8_t port) {
  if (this->IsConsole && this->NextChar == EOF) {
    char c;
    if (read(STDIN_FILENO, &c, 1)) {
      this->NextChar = (uint8_t)c;
    }
  }

  if (port == this->PortBase) {
    uint8_t val = 0x80U;
    if (this->NextChar != EOF) {
      val |= 0x40U;
    }
    return val; // TxEmpty
  }
  if (port == this->PortBase + 1) {
    uint8_t val = (uint8_t) this->NextChar;
    this->NextChar = EOF;
    return val;
  }
  return 0;
}

void TMS5501::doOut(uint8_t port, uint8_t value) {
  (void) value;
  if (port == (this->PortBase + 1)) {
    std::cout << (char) value;
    std::fflush(stdout);
  }
}
