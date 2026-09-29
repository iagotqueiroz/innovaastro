#include <WiFi.h>
#include <AccelStepper.h>
#include <WebServer.h>

// =================== AJUSTES DO SEU HARDWARE ===================
#define DIR_AZ 26
#define STEP_AZ 25
#define DIR_ALT 19
#define STEP_ALT 18

#define AZ_LIMIT_PIN 32
#define ALT_LIMIT_PIN 33

// =================== PARÂMETROS MECÂNICOS ===================
// Motor NEMA17: 200 passos "cheios" por volta
static const int STEPS_PER_REV = 200;
// A4988 em 1/16 (MS1, MS2, MS3 em HIGH)
static const int MICROSTEPPING = 1;

// Relações mecânicas por eixo (polia/coroa)
// AZ: motor 16 dentes, coroa ~178 dentes => 178/16 = 11.125
// ALT: motor 16 dentes, coroa 112 dentes => 112/16 = 7.0
static const float GEAR_RATIO_AZ = 24.0f;
static const float GEAR_RATIO_ALT = 15.0f;

// passos por grau = (passos por volta * microstepping * relação) / 360
float PASSOS_POR_GRAU_AZ = (STEPS_PER_REV * MICROSTEPPING * GEAR_RATIO_AZ) / 360.0f;   // ≈ 98.8889
float PASSOS_POR_GRAU_ALT = (STEPS_PER_REV * MICROSTEPPING * GEAR_RATIO_ALT) / 360.0f; // ≈ 62.2222

// =================== GLOBAIS VISÍVEIS EM OUTROS ARQUIVOS ===================
long ultimaMetaAzPassos = 0;
long ultimaMetaAltPassos = 0;

// Motores e servidor (exportados via extern no buscarAstro.cpp)
AccelStepper motorAz(AccelStepper::DRIVER, STEP_AZ, DIR_AZ);
AccelStepper motorAlt(AccelStepper::DRIVER, STEP_ALT, DIR_ALT);
WebServer server(80);

// =================== REDE ===================
// Troque para sua rede se necessário
const char *ssid = "BRUP_MJV_2G";
const char *password = "12345678";

// const char *ssid = "Queiroz 2.4ghz";
// const char *password = "igdigital3362";

// Declarar a função que configura as rotas no outro arquivo
void configurarBuscarAstro();
void configurarRotas();

// ===== Flag vinda do buscarAstro.cpp (modo tracking por velocidade) =====
extern volatile bool g_tracking;

volatile bool g_homingAz = false;

const float HOME_AZ_SPEED = -50.0;
unsigned long homeAzInicio = 0;
const unsigned long HOME_AZ_TIMEOUT = 15000;

volatile bool g_homingAlt = false;

const float HOME_ALT_SPEED = -50.0;
unsigned long homeAltInicio = 0;
const unsigned long HOME_ALT_TIMEOUT = 15000;

// Funções para mover os motores manualmente
void moverDireita()
{
  motorAz.moveTo(motorAz.currentPosition() + 100); // Move para direita
}

void moverEsquerda()
{
  motorAz.moveTo(motorAz.currentPosition() - 100); // Move para esquerda
}

void moverCima()
{
  motorAlt.moveTo(motorAlt.currentPosition() + 100); // Move para cima
}

void moverBaixo()
{
  motorAlt.moveTo(motorAlt.currentPosition() - 100); // Move para baixo
}

// Função para configurar as rotas no servidor
void configurarRotas()
{
  server.on("/controle", HTTP_GET, []()
            {
    String comando = server.arg("comando");

    g_tracking = false;

    if (comando == "direita") {
      moverDireita();  // Move para a direita
    } else if (comando == "esquerda") {
      moverEsquerda(); // Move para a esquerda
    } else if (comando == "cima") {
      moverCima();     // Move para cima
    } else if (comando == "baixo") {
      moverBaixo();    // Move para baixo
    }

    server.send(200, "application/json", "{\"status\": \"movimento realizado\", \"comando\": \"" + comando + "\"}"); });

  server.on("/limit_az", HTTP_GET, []()
            {

    int raw = digitalRead(AZ_LIMIT_PIN);
    bool acionado = (raw == LOW);

    if (acionado) {
      server.send(
        200,
        "application/json",
        "{\"acionado\":true,\"raw\":0,\"estado\":\"ACIONADO\"}"
      );
    } else {
      server.send(
        200,
        "application/json",
        "{\"acionado\":false,\"raw\":1,\"estado\":\"LIVRE\"}"
      );
    } });

  server.on("/limit_alt", HTTP_GET, []()
            {

    int raw = digitalRead(ALT_LIMIT_PIN);
    bool acionado = (raw == LOW);

    if (acionado) {
      server.send(
        200,
        "application/json",
        "{\"acionado\":true,\"raw\":0,\"estado\":\"ACIONADO\"}"
      );
    } else {
      server.send(
        200,
        "application/json",
        "{\"acionado\":false,\"raw\":1,\"estado\":\"LIVRE\"}"
      );
    } });

  server.on("/home_az", HTTP_GET, []()
            {

    // Desliga tracking
    g_tracking = false;

    // Se o switch já estiver pressionado
    if (digitalRead(AZ_LIMIT_PIN) == LOW) {

      motorAz.setSpeed(0);
      motorAz.setCurrentPosition(0);
      motorAz.moveTo(0);

      g_homingAz = false;

      server.send(
        200,
        "application/json",
        "{\"ok\":true,\"status\":\"AZ já estava no HOME\",\"az\":0}"
      );

      return;
    }

    // Cancela qualquer movimento anterior
    motorAz.moveTo(motorAz.currentPosition());

    // Velocidade do homing
    motorAz.setSpeed(HOME_AZ_SPEED);

    homeAzInicio = millis();
    g_homingAz = true;

    Serial.println("[HOME AZ] Iniciado.");

    server.send(
      200,
      "application/json",
      "{\"ok\":true,\"status\":\"HOME AZ iniciado\"}"
    ); });

  server.on("/home_alt", HTTP_GET, []()
            {

  g_tracking = false;

  // Se já estiver no fim de curso
  if (digitalRead(ALT_LIMIT_PIN) == LOW) {

    motorAlt.setSpeed(0);
    motorAlt.setCurrentPosition(0);
    motorAlt.moveTo(0);

    g_homingAlt = false;

    server.send(
      200,
      "application/json",
      "{\"ok\":true,\"status\":\"ALT já estava no HOME\",\"alt\":0}"
    );

    return;
  }

  // Cancela movimento anterior
  motorAlt.moveTo(motorAlt.currentPosition());

  // Velocidade do Home
  motorAlt.setSpeed(HOME_ALT_SPEED);

  homeAltInicio = millis();
  g_homingAlt = true;

  Serial.println("[HOME ALT] Iniciado.");

  server.send(
    200,
    "application/json",
    "{\"ok\":true,\"status\":\"HOME ALT iniciado\"}"
  ); });
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("[BOOT] Iniciando ESP32...");

  pinMode(AZ_LIMIT_PIN, INPUT_PULLUP);
  pinMode(ALT_LIMIT_PIN, INPUT_PULLUP);

  // ----- Wi-Fi -----
  Serial.printf("[WIFI] Conectando a \"%s\" ...\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 30)
  {
    delay(500);
    Serial.print(".");
    tentativas++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("[WIFI] Conectado!");
    Serial.print("[WIFI] IP: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("[WIFI][ERRO] Não conectou. Verifique SSID/senha.");
    // Dá pra seguir sem Wi-Fi, mas a rota HTTP não vai responder.
  }

  // ----- Motores -----
  motorAz.setMaxSpeed(200);     // ajuste fino depois, manter ≥ velocidade máxima que usará
  motorAz.setAcceleration(100); // ajuste fino depois

  motorAlt.setMaxSpeed(200);
  motorAlt.setAcceleration(100);

  // Direções (mantive como você tinha)
  motorAz.setPinsInverted(true, false);
  motorAlt.setPinsInverted(false, false);

  motorAz.setCurrentPosition(0);
  motorAlt.setCurrentPosition(0);

  // ----- HTTP -----
  configurarRotas(); // Chama a função para configurar as rotas de movimento
  Serial.println("[HTTP] Servidor iniciado. Rotas:");
  Serial.println("  GET /controle?comando=direita  (direita)");
  Serial.println("  GET /controle?comando=esquerda (esquerda)");
  Serial.println("  GET /controle?comando=cima     (cima)");
  Serial.println("  GET /controle?comando=baixo    (baixo)");

  // ----- HTTP -----
  configurarBuscarAstro(); // define as rotas /mover, /set_speed, etc.
  Serial.println("[HTTP] Servidor iniciado. Rotas:");
  Serial.println("  GET /mover?az=GRAUS&alt=GRAUS   (GoTo absoluto)");
  Serial.println("  GET /set_speed?vaz=DEGS&valt=DEGS  (seguimento por velocidade)");
  Serial.println("  GET /track_off (pausa tracking)");
  server.begin();
}

void loop()
{
  server.handleClient();

  if (g_homingAz)
  {
    // Switch encontrado
    if (digitalRead(AZ_LIMIT_PIN) == LOW)
    {
      motorAz.setSpeed(0);

      g_homingAz = false;

      motorAz.setCurrentPosition(0);
      motorAz.moveTo(0);

      Serial.println("[HOME AZ] Switch acionado. AZ = 0.");
    }

    // Segurança: timeout
    else if (millis() - homeAzInicio > HOME_AZ_TIMEOUT)
    {
      motorAz.setSpeed(0);
      motorAz.moveTo(motorAz.currentPosition());

      g_homingAz = false;

      Serial.println("[HOME AZ][ERRO] Timeout.");
    }

    else
    {
      motorAz.runSpeed();
    }
  }
  else if (g_homingAlt)
  {
    if (digitalRead(ALT_LIMIT_PIN) == LOW)
    {
      motorAlt.setSpeed(0);

      g_homingAlt = false;

      motorAlt.setCurrentPosition(0);
      motorAlt.moveTo(0);

      Serial.println("[HOME ALT] Switch acionado. ALT = 0.");
    }

    else if (millis() - homeAltInicio > HOME_ALT_TIMEOUT)
    {
      motorAlt.setSpeed(0);
      motorAlt.moveTo(motorAlt.currentPosition());

      g_homingAlt = false;

      Serial.println("[HOME ALT][ERRO] Timeout.");
    }

    else
    {
      motorAlt.runSpeed();
    }
  }
  else if (g_tracking)
  {
    motorAz.runSpeed();
    motorAlt.runSpeed();
  }
  else
  {
    motorAz.run();
    motorAlt.run();
  }
}
