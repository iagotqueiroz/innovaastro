#include <WiFi.h>
#include <AccelStepper.h>
#include <WebServer.h>

// ======== Wi-Fi ========
const char* ssid = "PEDRO HENRIQUE";
const char* password = "20240204";

// ======== Motor AZIMUTE ========
#define DIR_AZ 17
#define STEP_AZ 16
AccelStepper motorAz(AccelStepper::DRIVER, STEP_AZ, DIR_AZ);

// ======== Motor ALTITUDE ========
#define DIR_ALT 19
#define STEP_ALT 18
AccelStepper motorAlt(AccelStepper::DRIVER, STEP_ALT, DIR_ALT);

// ======== Web Server ========
WebServer server(80);

// ======== Variáveis globais ========
const float passosPorGrau = 200.0 / 360.0;  // Corrigido
long posicaoAtualAz = 0;
long posicaoAtualAlt = 0;

void handleMover() {
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
}

// ======== Incrementos para controle manual (em graus) ========
const float incrementoAz = 1.0;   // Cada comando "direita/esquerda" move 1 grau
const float incrementoAlt = 1.0;  // Cada comando "cima/baixo" move 1 grau

// ======== NOVA ROTA: Controle manual via comandos simples ========
void handleControle() {
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
}


void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[✓] Wi-Fi conectado! IP: " + WiFi.localIP().toString());

  // Velocidade e aceleração mais suaves
  motorAz.setMaxSpeed(200);
  motorAz.setAcceleration(30);
  motorAlt.setMaxSpeed(200);
  motorAlt.setAcceleration(30);

  // Inicializa na posição 0
  motorAz.setCurrentPosition(0);
  motorAlt.setCurrentPosition(0);

  server.on("/mover", handleMover);
  server.on("/controle", handleControle);
  server.begin();
  Serial.println("[✓] Servidor HTTP iniciado!");
}

void loop() {
  server.handleClient();
  motorAz.run();
  motorAlt.run();
}





// #include <WiFi.h>
// #include <AccelStepper.h>
// #include <WebServer.h>

// // ======== Wi-Fi ========
// const char* ssid = "PEDRO HENRIQUE";
// const char* password = "20240204";

// // ======== Motor AZIMUTE ========
// #define DIR_AZ 17
// #define STEP_AZ 16
// AccelStepper motorAz(AccelStepper::DRIVER, STEP_AZ, DIR_AZ);

// // ======== Motor ALTITUDE ========
// #define DIR_ALT 19
// #define STEP_ALT 18
// AccelStepper motorAlt(AccelStepper::DRIVER, STEP_ALT, DIR_ALT);

// // ======== Web Server ========
// WebServer server(80);

// // ======== Variáveis globais ========
// const float passosPorGrau = 1.0 / 1.8; // 1 passo = 1.8 graus
// float atualAz = 0.0;
// float atualAlt = 0.0;

// void handleMover() {
//   if (server.hasArg("az") && server.hasArg("alt")) {
//     float az = server.arg("az").toFloat();
//     float alt = server.arg("alt").toFloat();

//     long passosAz = az * passosPorGrau;
//     long passosAlt = alt * passosPorGrau;

//     motorAz.moveTo(passosAz);
//     motorAlt.moveTo(passosAlt);

//     atualAz = az;
//     atualAlt = alt;

//     server.send(200, "text/plain", "Movendo para AZ: " + String(az) + ", ALT: " + String(alt));
//   } else {
//     server.send(400, "text/plain", "Parâmetros ausentes (az, alt)");
//   }
// }

// void setup() {
//   Serial.begin(115200);

//   WiFi.begin(ssid, password);
//   Serial.print("Conectando ao Wi-Fi");
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//   }
//   Serial.println("\n[✓] Wi-Fi conectado! IP: " + WiFi.localIP().toString());

//   motorAz.setMaxSpeed(100);
//   motorAz.setAcceleration(10);
//   motorAlt.setMaxSpeed(100);
//   motorAlt.setAcceleration(10);

//   server.on("/mover", handleMover);
//   server.begin();
//   Serial.println("[✓] Servidor HTTP iniciado!");
// }

// void loop() {
//   server.handleClient();
//   motorAz.run();
//   motorAlt.run();
// }
