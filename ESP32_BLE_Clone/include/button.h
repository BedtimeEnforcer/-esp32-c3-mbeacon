#pragma once

#include <Arduino.h>

class Button {
 public:
  void begin();
  bool pressed();

 private:
  bool stable_ = HIGH;
  bool lastRead_ = HIGH;
  uint32_t changedAt_ = 0;
};

