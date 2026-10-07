#include <WiFi.h>
#include <AccelStepper.h>
#include <WebServer.h>
#include <math.h>

// =================== GPS ===================
HardwareSerial GPS_SERIAL(2);

#define GPS_RX_PIN 16
#define GPS_TX_PIN 17

// =================== AJUSTES DO SEU HARDWARE ===================
#define DIR_AZ 19
#define STEP_AZ 18
#define DIR_ALT 26
#define STEP_ALT 25

#define AZ_LIMIT_PIN 32
#define ALT_LIMIT_PIN 33

// =================== PARÂMETROS MECÂNICOS ===================
// Motor NEMA17: 200 passos "cheios" por volta
static const int STEPS_PER_REV = 200;
// A4988 em 1/16 (MS1, MS2, MS3 em HIGH)
static const int MICROSTEPPING = 16;

// ===== MODO DE TESTE =====
// true  = motores sem redução, teste de bancada
// false = montagem real com polias
static const bool BENCH_MODE = false;

// Relações mecânicas por eixo (polia/coroa)
// AZ: motor 16 dentes, coroa ~178 dentes => 178/16 = 11.125
// ALT: motor 16 dentes, coroa 112 dentes => 112/16 = 7.0
static const float GEAR_RATIO_AZ_REAL = 24.0f;
static const float GEAR_RATIO_ALT_REAL = 18.75f; // ALT: motor 16 dentes, coroa 300 dentes => 300/16 = 18.75

// Movimento manual
static const float MANUAL_MOVE_DEG =
    1.0f;

// Limites seguros também para o controle manual
static const float MANUAL_AZ_MIN_DEG = 0.0f;
static const float MANUAL_AZ_MAX_DEG = 350.0f;

static const float MANUAL_ALT_MIN_DEG = -27.0f;
static const float MANUAL_ALT_MAX_DEG = 90.0f;

// No HOME da ALT o tubo está 27° abaixo do horizonte
static const float MANUAL_ALT_HOME_DEG = -27.0f;

static const float GEAR_RATIO_AZ =
    BENCH_MODE ? 1.0f : GEAR_RATIO_AZ_REAL;

static const float GEAR_RATIO_ALT =
    BENCH_MODE ? 1.0f : GEAR_RATIO_ALT_REAL;

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
const char *ssid = "CASA LUNA 3 TORRE A";
const char *password = "LUNA2023";

// const char *ssid = "iPhone";
// const char *password = "45301510";

// const char *ssid = "BRUP_MJV_2G";
// const char *password = "12345678";

// const char *ssid = "Queiroz 2.4ghz";
// const char *password = "igdigital3362";

// Declarar a função que configura as rotas no outro arquivo
void configurarBuscarAstro();
void configurarRotas();

void atualizarTracking();
void cancelarNudgeTracking();

// ===== Flag vinda do buscarAstro.cpp (modo tracking por velocidade) =====
extern volatile bool g_tracking;

volatile bool g_homingAz = false;
volatile bool g_homeAzDone = false;

unsigned long homeAzInicio = 0;
const unsigned long HOME_AZ_TIMEOUT = 120000;

volatile bool g_homingAlt = false;
volatile bool g_homeAltDone = false;

unsigned long homeAltInicio = 0;
const unsigned long HOME_ALT_TIMEOUT = 120000;

const float HOME_AZ_FAST_SPEED =
    100.0f * MICROSTEPPING;

const float HOME_AZ_SLOW_SPEED =
    20.0f * MICROSTEPPING;

const float HOME_AZ_BACKOFF_SPEED =
    -40.0f * MICROSTEPPING;

const float HOME_ALT_FAST_SPEED =
    -100.0f * MICROSTEPPING;

const float HOME_ALT_SLOW_SPEED =
    -20.0f * MICROSTEPPING;

const float HOME_ALT_BACKOFF_SPEED =
    40.0f * MICROSTEPPING;

const float HOME_AZ_CLEARANCE_DEG = 1.0f;
const float HOME_ALT_CLEARANCE_DEG = 1.0f;

enum HomePhase
{
  HOME_IDLE,

  HOME_SEEK_FAST,

  HOME_RELEASE_FIRST,
  HOME_CLEAR_FIRST,

  HOME_SEEK_SLOW,

  HOME_RELEASE_FINAL,
  HOME_CLEAR_FINAL
};

HomePhase homeAzPhase = HOME_IDLE;
HomePhase homeAltPhase = HOME_IDLE;

long homeAzClearTarget = 0;
long homeAltClearTarget = 0;

long homeAzClearanceSteps()
{
  long passos = (long)lround(
      HOME_AZ_CLEARANCE_DEG *
      PASSOS_POR_GRAU_AZ);

  return passos < 1 ? 1 : passos;
}

long homeAltClearanceSteps()
{
  long passos = (long)lround(
      HOME_ALT_CLEARANCE_DEG *
      PASSOS_POR_GRAU_ALT);

  return passos < 1 ? 1 : passos;
}

// =====================================================
// PROTEÇÃO PERMANENTE DOS FINS DE CURSO
// =====================================================

bool limiteAzAcionado()
{
  return digitalRead(AZ_LIMIT_PIN) == LOW;
}

bool limiteAltAcionado()
{
  return digitalRead(ALT_LIMIT_PIN) == LOW;
}

// Para imediatamente sem perder o valor
// da posição lógica atual.
void pararAzImediato()
{
  long posicaoAtual =
      motorAz.currentPosition();

  motorAz.setCurrentPosition(
      posicaoAtual);

  motorAz.moveTo(
      posicaoAtual);

  motorAz.setSpeed(0);
}

void pararAltImediato()
{
  long posicaoAtual =
      motorAlt.currentPosition();

  motorAlt.setCurrentPosition(
      posicaoAtual);

  motorAlt.moveTo(
      posicaoAtual);

  motorAlt.setSpeed(0);
}

// Proteção usada em movimentos de posição:
// manual e GoTo.
void protegerLimitesPosicionais()
{
  // AZ:
  // posição positiva = direção do switch.
  if (
      !g_homingAz &&
      limiteAzAcionado() &&
      motorAz.distanceToGo() > 0)
  {
    pararAzImediato();

    Serial.println(
        "[SEGURANCA] AZ bloqueado pelo fim de curso.");
  }

  // ALT:
  // posição negativa = direção do switch.
  if (
      !g_homingAlt &&
      limiteAltAcionado() &&
      motorAlt.distanceToGo() < 0)
  {
    pararAltImediato();

    Serial.println(
        "[SEGURANCA] ALT bloqueado pelo fim de curso.");
  }
}

bool moverDireita()
{
  long novoAlvo =
      motorAz.currentPosition() +
      (long)lround(
          MANUAL_MOVE_DEG *
          PASSOS_POR_GRAU_AZ);

  float novoAzDeg =
      -novoAlvo /
      PASSOS_POR_GRAU_AZ;

  // DIREITA diminui o AZ lógico.
  // Não pode passar de 0°.
  if (novoAzDeg < MANUAL_AZ_MIN_DEG)
  {
    Serial.println(
        "[SEGURANCA] Manual AZ bloqueado em 0°.");

    return false;
  }

  motorAz.moveTo(novoAlvo);

  return true;
}

bool moverEsquerda()
{
  long novoAlvo =
      motorAz.currentPosition() -
      (long)lround(
          MANUAL_MOVE_DEG *
          PASSOS_POR_GRAU_AZ);

  float novoAzDeg =
      -novoAlvo /
      PASSOS_POR_GRAU_AZ;

  // ESQUERDA aumenta o AZ lógico.
  // Não pode passar de 350°.
  if (novoAzDeg > MANUAL_AZ_MAX_DEG)
  {
    Serial.println(
        "[SEGURANCA] Manual AZ bloqueado em 350°.");

    return false;
  }

  motorAz.moveTo(novoAlvo);

  return true;
}

bool moverCima()
{
  long novoAlvo =
      motorAlt.currentPosition() +
      (long)lround(
          MANUAL_MOVE_DEG *
          PASSOS_POR_GRAU_ALT);

  float novoAltDeg =
      MANUAL_ALT_HOME_DEG +
      novoAlvo /
          PASSOS_POR_GRAU_ALT;

  // CIMA aumenta ALT.
  // Não pode passar de 90°.
  if (novoAltDeg > MANUAL_ALT_MAX_DEG)
  {
    Serial.println(
        "[SEGURANCA] Manual ALT bloqueado em 90°.");

    return false;
  }

  motorAlt.moveTo(novoAlvo);

  return true;
}

bool moverBaixo()
{
  long novoAlvo =
      motorAlt.currentPosition() -
      (long)lround(
          MANUAL_MOVE_DEG *
          PASSOS_POR_GRAU_ALT);

  float novoAltDeg =
      MANUAL_ALT_HOME_DEG +
      novoAlvo /
          PASSOS_POR_GRAU_ALT;

  // BAIXO diminui ALT.
  // Não pode passar abaixo do horizonte.
  if (novoAltDeg < MANUAL_ALT_MIN_DEG)
  {
    Serial.println(
        "[SEGURANCA] Manual ALT bloqueado em 0°.");

    return false;
  }

  motorAlt.moveTo(novoAlvo);

  return true;
}

// Função para configurar as rotas no servidor
void configurarRotas()
{
  server.on("/controle", HTTP_GET, []()
            {
    String comando = server.arg("comando");

    g_tracking = false;
    cancelarNudgeTracking();

    // Se estava fazendo HOME, cancela somente o modo HOME
    if (g_homingAz)
    {
      g_homingAz = false;
      g_homeAzDone = false;

      motorAz.setSpeed(0);
      motorAz.moveTo(motorAz.currentPosition());
    }

    if (g_homingAlt)
    {
      g_homingAlt = false;
      g_homeAltDone = false;

      motorAlt.setSpeed(0);
      motorAlt.moveTo(motorAlt.currentPosition());
    }

    bool movimentoAceito = true;

    if (comando == "direita")
    {
      movimentoAceito = moverDireita();
    }
    else if (comando == "esquerda")
    {
      movimentoAceito = moverEsquerda();
    }
    else if (comando == "cima")
    {
      movimentoAceito = moverCima();
    }
    else if (comando == "baixo")
    {
      movimentoAceito = moverBaixo();
    }
    else
    {
      server.send(
          400,
          "application/json",
          "{\"ok\":false,\"erro\":\"Comando manual invalido\"}");

      return;
    }

    if (!movimentoAceito)
    {
      server.send(
          422,
          "application/json",
          "{\"ok\":false,\"erro\":\"Movimento manual bloqueado por limite virtual\"}");

      return;
    }

    server.send(
        200,
        "application/json",
        "{\"ok\":true,\"status\":\"movimento realizado\",\"comando\":\"" +
            comando +
            "\"}"); });

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
    g_tracking = false;
    cancelarNudgeTracking();

    g_homeAzDone = false;

    motorAz.setSpeed(0);
    motorAz.moveTo(
        motorAz.currentPosition()
    );

    homeAzInicio = millis();

    g_homingAz = true;

    // Se já estiver apertando o switch,
    // primeiro sai dele.
    if (digitalRead(AZ_LIMIT_PIN) == LOW)
    {
        homeAzPhase = HOME_RELEASE_FIRST;

        motorAz.setSpeed(
            HOME_AZ_BACKOFF_SPEED
        );

        Serial.println(
            "[HOME AZ] Switch já acionado. Recuando."
        );
    }
    else
    {
        homeAzPhase = HOME_SEEK_FAST;

        motorAz.setSpeed(
            HOME_AZ_FAST_SPEED
        );

        Serial.println(
            "[HOME AZ] Procurando switch."
        );
    }


    server.send(
        200,
        "application/json",
        "{\"ok\":true,\"status\":\"HOME AZ iniciado\"}"
    ); });

  server.on("/home_alt", HTTP_GET, []()
            {
    g_tracking = false;
    cancelarNudgeTracking();

    g_homeAltDone = false;

    motorAlt.setSpeed(0);
    motorAlt.moveTo(
        motorAlt.currentPosition()
    );

    homeAltInicio = millis();

    g_homingAlt = true;


    if (digitalRead(ALT_LIMIT_PIN) == LOW)
    {
        homeAltPhase =
            HOME_RELEASE_FIRST;

        motorAlt.setSpeed(
            HOME_ALT_BACKOFF_SPEED
        );

        Serial.println(
            "[HOME ALT] Switch já acionado. Recuando."
        );
    }
    else
    {
        homeAltPhase =
            HOME_SEEK_FAST;

        motorAlt.setSpeed(
            HOME_ALT_FAST_SPEED
        );

        Serial.println(
            "[HOME ALT] Procurando switch."
        );
    }


    server.send(
        200,
        "application/json",
        "{\"ok\":true,\"status\":\"HOME ALT iniciado\"}"
    ); });

  server.on("/home_status", HTTP_GET, []()
            {

  String json = "{";

  json += "\"homingAz\":";
  json += (g_homingAz ? "true" : "false");
  json += ",";

  json += "\"doneAz\":";
  json += (g_homeAzDone ? "true" : "false");
  json += ",";

  json += "\"homingAlt\":";
  json += (g_homingAlt ? "true" : "false");
  json += ",";

  json += "\"doneAlt\":";
  json += (g_homeAltDone ? "true" : "false");

  json += "}";

  server.send(200, "application/json", json); });
}

void updateHomeAz()
{
  // =========================
  // TIMEOUT
  // =========================

  if (
      millis() - homeAzInicio >
      HOME_AZ_TIMEOUT)
  {
    motorAz.setSpeed(0);

    motorAz.moveTo(
        motorAz.currentPosition());

    g_homingAz = false;
    g_homeAzDone = false;

    homeAzPhase = HOME_IDLE;

    Serial.println(
        "[HOME AZ][ERRO] Timeout.");

    return;
  }

  switch (homeAzPhase)
  {

    // =========================
    // PROCURA RÁPIDA
    // =========================

  case HOME_SEEK_FAST:

    if (
        digitalRead(AZ_LIMIT_PIN) == LOW)
    {
      motorAz.setSpeed(
          HOME_AZ_BACKOFF_SPEED);

      homeAzPhase =
          HOME_RELEASE_FIRST;

      Serial.println(
          "[HOME AZ] Primeiro toque.");
    }
    else
    {
      motorAz.runSpeed();
    }

    break;

    // =========================
    // ESPERA SWITCH SOLTAR
    // =========================

  case HOME_RELEASE_FIRST:

    if (
        digitalRead(AZ_LIMIT_PIN) == HIGH)
    {
      homeAzClearTarget =
          motorAz.currentPosition() - homeAzClearanceSteps();

      homeAzPhase =
          HOME_CLEAR_FIRST;

      Serial.println(
          "[HOME AZ] Switch liberado.");
    }
    else
    {
      motorAz.runSpeed();
    }

    break;

    // =========================
    // FOLGA EXTRA
    // =========================

  case HOME_CLEAR_FIRST:

    if (
        motorAz.currentPosition() <= homeAzClearTarget)
    {
      motorAz.setSpeed(
          HOME_AZ_SLOW_SPEED);

      homeAzPhase =
          HOME_SEEK_SLOW;

      Serial.println(
          "[HOME AZ] Aproximação lenta.");
    }
    else
    {
      motorAz.runSpeed();
    }

    break;

    // =========================
    // SEGUNDO TOQUE
    // =========================

  case HOME_SEEK_SLOW:

    if (
        digitalRead(AZ_LIMIT_PIN) == LOW)
    {
      motorAz.setSpeed(
          HOME_AZ_BACKOFF_SPEED);

      homeAzPhase =
          HOME_RELEASE_FINAL;

      Serial.println(
          "[HOME AZ] Segundo toque.");
    }
    else
    {
      motorAz.runSpeed();
    }

    break;

    // =========================
    // SOLTA SWITCH NOVAMENTE
    // =========================

  case HOME_RELEASE_FINAL:

    if (
        digitalRead(AZ_LIMIT_PIN) == HIGH)
    {
      homeAzClearTarget =
          motorAz.currentPosition() - homeAzClearanceSteps();

      homeAzPhase =
          HOME_CLEAR_FINAL;

      Serial.println(
          "[HOME AZ] Switch liberado final.");
    }
    else
    {
      motorAz.runSpeed();
    }

    break;

    // =========================
    // POSIÇÃO SEGURA FINAL
    // =========================

  case HOME_CLEAR_FINAL:

    if (
        motorAz.currentPosition() <= homeAzClearTarget)
    {
      motorAz.setSpeed(0);

      // O ZERO passa a ser a
      // posição segura, não
      // o switch pressionado.
      motorAz.setCurrentPosition(0);
      motorAz.moveTo(0);

      g_homingAz = false;
      g_homeAzDone = true;

      homeAzPhase = HOME_IDLE;

      Serial.println(
          "[HOME AZ] Concluído. Switch livre. AZ = 0.");
    }
    else
    {
      motorAz.runSpeed();
    }

    break;

  default:

    motorAz.setSpeed(0);

    break;
  }
}

void updateHomeAlt()
{
  if (
      millis() - homeAltInicio >
      HOME_ALT_TIMEOUT)
  {
    motorAlt.setSpeed(0);

    motorAlt.moveTo(
        motorAlt.currentPosition());

    g_homingAlt = false;
    g_homeAltDone = false;

    homeAltPhase = HOME_IDLE;

    Serial.println(
        "[HOME ALT][ERRO] Timeout.");

    return;
  }

  switch (homeAltPhase)
  {

  case HOME_SEEK_FAST:

    if (
        digitalRead(ALT_LIMIT_PIN) == LOW)
    {
      motorAlt.setSpeed(
          HOME_ALT_BACKOFF_SPEED);

      homeAltPhase =
          HOME_RELEASE_FIRST;

      Serial.println(
          "[HOME ALT] Primeiro toque.");
    }
    else
    {
      motorAlt.runSpeed();
    }

    break;

  case HOME_RELEASE_FIRST:

    if (
        digitalRead(ALT_LIMIT_PIN) == HIGH)
    {
      homeAltClearTarget =
          motorAlt.currentPosition() + homeAltClearanceSteps();

      homeAltPhase =
          HOME_CLEAR_FIRST;

      Serial.println(
          "[HOME ALT] Switch liberado.");
    }
    else
    {
      motorAlt.runSpeed();
    }

    break;

  case HOME_CLEAR_FIRST:

    if (
        motorAlt.currentPosition() >= homeAltClearTarget)
    {
      motorAlt.setSpeed(
          HOME_ALT_SLOW_SPEED);

      homeAltPhase =
          HOME_SEEK_SLOW;

      Serial.println(
          "[HOME ALT] Aproximação lenta.");
    }
    else
    {
      motorAlt.runSpeed();
    }

    break;

  case HOME_SEEK_SLOW:

    if (
        digitalRead(ALT_LIMIT_PIN) == LOW)
    {
      motorAlt.setSpeed(
          HOME_ALT_BACKOFF_SPEED);

      homeAltPhase =
          HOME_RELEASE_FINAL;

      Serial.println(
          "[HOME ALT] Segundo toque.");
    }
    else
    {
      motorAlt.runSpeed();
    }

    break;

  case HOME_RELEASE_FINAL:

    if (
        digitalRead(ALT_LIMIT_PIN) == HIGH)
    {
      homeAltClearTarget =
          motorAlt.currentPosition() + homeAltClearanceSteps();

      homeAltPhase =
          HOME_CLEAR_FINAL;

      Serial.println(
          "[HOME ALT] Switch liberado final.");
    }
    else
    {
      motorAlt.runSpeed();
    }

    break;

  case HOME_CLEAR_FINAL:

    if (
        motorAlt.currentPosition() >= homeAltClearTarget)
    {
      motorAlt.setSpeed(0);

      motorAlt.setCurrentPosition(0);
      motorAlt.moveTo(0);

      g_homingAlt = false;
      g_homeAltDone = true;

      homeAltPhase = HOME_IDLE;

      Serial.println(
          "[HOME ALT] Concluído. Switch livre. ALT = 0.");
    }
    else
    {
      motorAlt.runSpeed();
    }

    break;

  default:

    motorAlt.setSpeed(0);

    break;
  }
}

void setup()
{
  Serial.begin(115200);

  GPS_SERIAL.begin(
      9600,
      SERIAL_8N1,
      GPS_RX_PIN,
      GPS_TX_PIN);

  Serial.println("[GPS] UART iniciada em 9600 baud.");

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
  motorAz.setMaxSpeed(200.0f * MICROSTEPPING);     // ajuste fino depois, manter ≥ velocidade máxima que usará
  motorAz.setAcceleration(100.0f * MICROSTEPPING); // ajuste fino depois

  motorAlt.setMaxSpeed(200.0f * MICROSTEPPING);
  motorAlt.setAcceleration(100.0f * MICROSTEPPING);

  // Direções (mantive como você tinha)
  motorAz.setPinsInverted(false, false);
  motorAlt.setPinsInverted(true, false, false);

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

  while (GPS_SERIAL.available())
  {
    char c = GPS_SERIAL.read();
    Serial.write(c);
  }

  if (g_homingAz)
  {
    updateHomeAz();
  }
  else if (g_homingAlt)
  {
    updateHomeAlt();
  }
  else if (g_tracking)
  {
    atualizarTracking();
  }
  else
  {
    protegerLimitesPosicionais();

    motorAz.run();
    motorAlt.run();
  }
}
