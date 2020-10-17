//
// Created by codetector on 10/12/20.
//

#ifndef Z80_IODEVICE_H
#define Z80_IODEVICE_H

#include "stdint.h"

class IODevice {
public:
  virtual void doOut(uint8_t port, uint8_t value) = 0;
  virtual uint8_t doIn(uint8_t port) = 0;
};

#endif //Z80_IODEVICE_H
