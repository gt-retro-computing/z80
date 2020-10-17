//
// Created by codetector on 10/13/20.
//

#include <stdint.h>
#include "IODevice.h"

#ifndef Z80_TMS5501_H
#define Z80_TMS5501_H

class TMS5501 : public IODevice {
private:
  uint8_t PortBase;
public:
  explicit TMS5501(uint8_t Base) : PortBase(Base) {}

  uint8_t doIn(uint8_t port) override;

  void doOut(uint8_t port, uint8_t value) override;
};

#endif //Z80_TMS5501_H
