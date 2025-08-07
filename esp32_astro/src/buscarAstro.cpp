#include <WebServer.h>
#include <AccelStepper.h>

// Usa variáveis do main.cpp
extern WebServer server;
extern AccelStepper motorAz;
extern AccelStepper motorAlt;

const float passosPorGrau = 200.0 / 360.0;
extern long posicaoAtualAz;
extern long posicaoAtualAlt;

void configurarBuscarAstro() {
  server.on("/mover", []() {
    if (server.hasArg("az") && server.hasArg("alt")) {
      float grausAz = server.arg("az").toFloat();
      float grausAlt = server.arg("alt").toFloat();

      long novaPosAz = grausAz * passosPorGrau;
      long novaPosAlt = grausAlt * passosPorGrau;

      motorAz.moveTo(novaPosAz);
      motorAlt.moveTo(novaPosAlt);

      posicaoAtualAz = novaPosAz;
      posicaoAtualAlt = novaPosAlt;

      server.send(200, "text/plain", "Movendo para AZ: " + String(grausAz) + "°, ALT: " + String(grausAlt) + "°");
    } else {
      server.send(400, "text/plain", "Parâmetros ausentes (az, alt)");
    }
  });
}
