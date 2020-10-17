//
// Created by codetector on 10/13/20.
//

#include "TMS5501.h"
#include <iostream>

uint8_t TMS5501::doIn(uint8_t port) {
  if (port == this->PortBase)
    return 0x80; // TxEmpty
  return 0;
}

void TMS5501::doOut(uint8_t port, uint8_t value) {
  (void )value;
  if (port == (this->PortBase + 1)) {
    std::cout << (char)value ;
    std::fflush(stdout);
  }
}
