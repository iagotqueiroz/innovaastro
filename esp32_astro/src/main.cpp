// #include <WiFi.h>
// #include <AccelStepper.h>
// #include <WebServer.h>

// // ======== Wi-Fi ========
// const char* ssid = "iagorana";
// const char* password = "ranaeiago610";

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
// float atualAz = 0.0;
// float atualAlt = 0.0;
// const float passosPorGrau = 1.8; // 1 passo = 1.8°

// void moverPara(float novoAz, float novoAlt) {
//   float deltaAz = novoAz - atualAz;
//   float deltaAlt = novoAlt - atualAlt;

//   long passosAz = deltaAz * (1.0 / passosPorGrau);
//   long passosAlt = deltaAlt * (1.0 / passosPorGrau);

//   motorAz.move(passosAz);
//   motorAlt.move(passosAlt);

//   atualAz = novoAz;
//   atualAlt = novoAlt;
// }

// void handleMover() {
//   if (server.hasArg("az") && server.hasArg("alt")) {
//     float az = server.arg("az").toFloat();
//     float alt = server.arg("alt").toFloat();

//     moverPara(az, alt);

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

//   motorAz.setMaxSpeed(500);
//   motorAz.setAcceleration(100);
//   motorAlt.setMaxSpeed(500);
//   motorAlt.setAcceleration(100);

//   server.on("/mover", handleMover);
//   server.begin();
//   Serial.println("[✓] Servidor HTTP iniciado!");
// }

// void loop() {
//   server.handleClient();
//   motorAz.run();
//   motorAlt.run();
// }


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
const float passosPorGrau = 1.0 / 1.8; // 1 passo = 1.8 graus
float atualAz = 0.0;
float atualAlt = 0.0;

void handleMover() {
  if (server.hasArg("az") && server.hasArg("alt")) {
    float az = server.arg("az").toFloat();
    float alt = server.arg("alt").toFloat();

    long passosAz = az * passosPorGrau;
    long passosAlt = alt * passosPorGrau;

    motorAz.moveTo(passosAz);
    motorAlt.moveTo(passosAlt);

    atualAz = az;
    atualAlt = alt;

    server.send(200, "text/plain", "Movendo para AZ: " + String(az) + ", ALT: " + String(alt));
  } else {
    server.send(400, "text/plain", "Parâmetros ausentes (az, alt)");
  }
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

  motorAz.setMaxSpeed(250);
  motorAz.setAcceleration(50);
  motorAlt.setMaxSpeed(500);
  motorAlt.setAcceleration(100);

  server.on("/mover", handleMover);
  server.begin();
  Serial.println("[✓] Servidor HTTP iniciado!");
}

void loop() {
  server.handleClient();
  motorAz.run();
  motorAlt.run();
}
