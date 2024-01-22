#include "defines_and_functions.h"

void setup() {
  Serial.begin(115200);
  Serial.println("Comunicão serial iniciada");

  initLEDS();
  statusLEDS(true, false, false); //RGB

  initWIFI();

  initServidor();
}

void loop() {
  server.handleClient();
}
