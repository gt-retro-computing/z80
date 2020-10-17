//
// Created by codetector on 10/12/20.
//

#include "8251Uart.h"

#include <iostream>

I8251Uart::I8251Uart(uint8_t base) {
  this->BasePort = base & 0xFE;
}

void I8251Uart::doOut(uint8_t port, uint8_t value) {
  if (port == this->BasePort) {
    std::printf("%c", value);
  } else if (port == this->BasePort + 1) {

  }
}

uint8_t I8251Uart::doIn(uint8_t port) {
  if (port == this->BasePort + 1) { // Control
    return 0x1; // TxReady
  }
  return 0;
}
