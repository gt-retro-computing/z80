//
// Created by codetector on 10/13/20.
//

#include <stdint.h>
#include <cstdio>
#include "IODevice.h"

#ifndef Z80_TMS5501_H
#define Z80_TMS5501_H

class TMS5501 : public IODevice {
private:
  uint8_t PortBase;
  bool IsConsole;
  int NextChar = EOF;
public:
  explicit TMS5501(uint8_t Base, bool isConsole = false) : PortBase(Base), IsConsole(isConsole) {}

  uint8_t doIn(uint8_t port) override;

  void doOut(uint8_t port, uint8_t value) override;
};

#endif //Z80_TMS5501_H
