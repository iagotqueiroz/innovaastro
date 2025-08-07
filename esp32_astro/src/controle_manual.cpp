#include <WebServer.h>
#include <AccelStepper.h>

extern WebServer server;
extern AccelStepper motorAz;
extern AccelStepper motorAlt;

const float passosPorGrau = 200.0 / 360.0;
const float incrementoAz = 1.0;
const float incrementoAlt = 1.0;

long posicaoAtualAz = 0;
long posicaoAtualAlt = 0;

void configurarControleManual() {
  server.on("/controle", []() {
    if (!server.hasArg("comando")) {
      server.send(400, "text/plain", "Parâmetro 'comando' ausente.");
      return;
    }

    String comando = server.arg("comando");
    Serial.println("[COMANDO MANUAL] " + comando);

    if (comando == "cima") {
      posicaoAtualAlt += incrementoAlt * passosPorGrau;
      motorAlt.moveTo(posicaoAtualAlt);
    } else if (comando == "baixo") {
      posicaoAtualAlt -= incrementoAlt * passosPorGrau;
      motorAlt.moveTo(posicaoAtualAlt);
    } else if (comando == "direita") {
      posicaoAtualAz += incrementoAz * passosPorGrau;
      motorAz.moveTo(posicaoAtualAz);
    } else if (comando == "esquerda") {
      posicaoAtualAz -= incrementoAz * passosPorGrau;
      motorAz.moveTo(posicaoAtualAz);
    } else if (comando == "parar") {
      motorAz.stop();
      motorAlt.stop();
    } else {
      server.send(400, "text/plain", "Comando inválido.");
      return;
    }

    server.send(200, "text/plain", "Comando '" + comando + "' executado.");
  });
}
