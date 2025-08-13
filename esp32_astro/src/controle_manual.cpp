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
      Serial.println("[MOVIMENTO] Subindo Altitude.");
    } else if (comando == "baixo") {
      posicaoAtualAlt -= incrementoAlt * passosPorGrau;
      motorAlt.moveTo(posicaoAtualAlt);
      Serial.println("[MOVIMENTO] Descendo Altitude.");
    } else if (comando == "direita") {
      posicaoAtualAz += incrementoAz * passosPorGrau;
      motorAz.moveTo(posicaoAtualAz);
      Serial.println("[MOVIMENTO] Movendo Direita.");
    } else if (comando == "esquerda") {
      posicaoAtualAz -= incrementoAz * passosPorGrau;
      motorAz.moveTo(posicaoAtualAz);
      Serial.println("[MOVIMENTO] Movendo Esquerda.");
    } else if (comando == "parar") {
      motorAz.stop();
      motorAlt.stop();
      Serial.println("[MOVIMENTO] Motores parados.");
    }


    // Resposta JSON
    String resposta = "{\"status\": \"sucesso\", \"mensagem\": \"Comando '" + comando + "' executado.\"}";
    server.send(200, "application/json", resposta);
  });
}
