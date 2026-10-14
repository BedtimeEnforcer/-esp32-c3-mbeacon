#include "button.h"

#include "config.h"

void Button::begin() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  stable_ = digitalRead(BUTTON_PIN);
  lastRead_ = stable_;
}

bool Button::pressed() {
  const bool reading = digitalRead(BUTTON_PIN);
  const uint32_t now = millis();
  if (reading != lastRead_) changedAt_ = now;
  lastRead_ = reading;
  if (reading != stable_ && now - changedAt_ >= 40) {
    stable_ = reading;
    return stable_ == LOW;
  }
  return false;
}

