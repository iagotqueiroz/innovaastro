#include <WiFi.h>
#include <AccelStepper.h>
#include <WebServer.h>

// Declaração das funções separadas
void configurarBuscarAstro();
void configurarControleManual();
void configurarRastreamentoCamera();

const char* ssid = "PEDRO HENRIQUE";
const char* password = "20240204";

#define DIR_AZ 17
#define STEP_AZ 16
#define DIR_ALT 19
#define STEP_ALT 18

AccelStepper motorAz(AccelStepper::DRIVER, STEP_AZ, DIR_AZ);
AccelStepper motorAlt(AccelStepper::DRIVER, STEP_ALT, DIR_ALT);
WebServer server(80);

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[✓] Conectado ao Wi-Fi. IP: " + WiFi.localIP().toString());

  motorAz.setMaxSpeed(200);
  motorAz.setAcceleration(30);
  motorAlt.setMaxSpeed(200);
  motorAlt.setAcceleration(30);

  motorAz.setCurrentPosition(0);
  motorAlt.setCurrentPosition(0);

  configurarBuscarAstro();         
  configurarControleManual();
  //configurarRastreamentoCamera();

  server.begin();
  Serial.println("[✓] Servidor HTTP iniciado!");
}

void loop() {
  server.handleClient();
  motorAz.run();
  motorAlt.run();
}
