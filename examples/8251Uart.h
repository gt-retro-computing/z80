//
// Created by codetector on 10/12/20.
//

#ifndef Z80_8251UART_H
#define Z80_8251UART_H

#include <stdint.h>
#include "IODevice.h"

class I8251Uart: public IODevice {
public:
  uint8_t BasePort;
  explicit I8251Uart(uint8_t base);

  void doOut(uint8_t port, uint8_t value) override;

  uint8_t doIn(uint8_t port) override;
};

#endif //Z80_8251UART_H
