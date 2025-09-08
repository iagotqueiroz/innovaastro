#include <WiFi.h>
#include <AccelStepper.h>
#include <WebServer.h>

// =================== AJUSTES DO SEU HARDWARE ===================
#define DIR_AZ 17
#define STEP_AZ 16
#define DIR_ALT 19
#define STEP_ALT 18

// =================== PARÂMETROS MECÂNICOS ===================
// Motor NEMA17: 200 passos "cheios" por volta
static const int STEPS_PER_REV = 200;
// A4988 em 1/16 (MS1, MS2, MS3 em HIGH)
static const int MICROSTEPPING = 16;

// Relações mecânicas por eixo (polia/coroa)
// AZ: motor 16 dentes, coroa ~178 dentes => 178/16 = 11.125
// ALT: motor 16 dentes, coroa 112 dentes => 112/16 = 7.0
static const float GEAR_RATIO_AZ = 11.125f;
static const float GEAR_RATIO_ALT = 7.0f;

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
const char *ssid = "Hotel Mattes Wifi";
const char *password = "mattes.80";

// Declarar a função que configura as rotas no outro arquivo
void configurarBuscarAstro();
void configurarRotas();

// ===== Flag vinda do buscarAstro.cpp (modo tracking por velocidade) =====
extern volatile bool g_tracking;

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
void configurarRotas() {
  server.on("/mover", HTTP_GET, []() {
    String comando = server.arg("comando");

    if (comando == "direita") {
      moverDireita();  // Move para a direita
    } else if (comando == "esquerda") {
      moverEsquerda(); // Move para a esquerda
    } else if (comando == "cima") {
      moverCima();     // Move para cima
    } else if (comando == "baixo") {
      moverBaixo();    // Move para baixo
    }

    server.send(200, "application/json", "{\"status\": \"movimento realizado\", \"comando\": \"" + comando + "\"}");
  });
}




void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("[BOOT] Iniciando ESP32...");

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
  motorAz.setMaxSpeed(1200);    // ajuste fino depois, manter ≥ velocidade máxima que usará
  motorAz.setAcceleration(600); // ajuste fino depois

  motorAlt.setMaxSpeed(1200);
  motorAlt.setAcceleration(600);

  // Direções (mantive como você tinha)
  motorAz.setPinsInverted(true, false);
  motorAlt.setPinsInverted(false, false);

  motorAz.setCurrentPosition(0);
  motorAlt.setCurrentPosition(0);

  // ----- HTTP -----
  configurarRotas(); // Chama a função para configurar as rotas de movimento
  Serial.println("[HTTP] Servidor iniciado. Rotas:");
  Serial.println("  GET /mover?comando=direita  (direita)");
  Serial.println("  GET /mover?comando=esquerda (esquerda)");
  Serial.println("  GET /mover?comando=cima     (cima)");
  Serial.println("  GET /mover?comando=baixo    (baixo)");

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

  if (g_tracking)
  {
    // ===== Seguimento por velocidade contínua =====
    // Velocidades são definidas pela rota /set_speed (deg/s → passos/s)
    motorAz.runSpeed();
    motorAlt.runSpeed();
  }
  else
  {
    // ===== GoTo/posicional =====
    // Caminha até a meta definida por /mover
    motorAz.run();
    motorAlt.run();
  }
}

//-------------------------------------

// versão boa 02
// #include <WiFi.h>
// #include <AccelStepper.h>
// #include <WebServer.h>

// // =================== AJUSTES DO SEU HARDWARE ===================
// #define DIR_AZ   17
// #define STEP_AZ  16
// #define DIR_ALT  19
// #define STEP_ALT 18

// // =================== PARÂMETROS MECÂNICOS ===================
// // Motor NEMA17: 200 passos "cheios" por volta
// static const int   STEPS_PER_REV  = 200;
// // A4988 em 1/16 (MS1, MS2, MS3 em HIGH)
// static const int   MICROSTEPPING  = 16;

// // Relações mecânicas por eixo (polia/coroa)
// // AZ: motor 16 dentes, coroa ~178 dentes => 178/16 = 11.125
// // ALT: motor 16 dentes, coroa 112 dentes => 112/16 = 7.0
// static const float GEAR_RATIO_AZ  = 11.125f;
// static const float GEAR_RATIO_ALT = 7.0f;

// // passos por grau = (passos por volta * microstepping * relação) / 360
// float PASSOS_POR_GRAU_AZ  = (STEPS_PER_REV * MICROSTEPPING * GEAR_RATIO_AZ)  / 360.0f; // ≈ 98.8889
// float PASSOS_POR_GRAU_ALT = (STEPS_PER_REV * MICROSTEPPING * GEAR_RATIO_ALT) / 360.0f; // ≈ 62.2222

// // =================== GLOBAIS VISÍVEIS EM OUTROS ARQUIVOS ===================
// long ultimaMetaAzPassos  = 0;
// long ultimaMetaAltPassos = 0;

// // Motores e servidor (exportados via extern no buscarAstro.cpp)
// AccelStepper motorAz(AccelStepper::DRIVER, STEP_AZ, DIR_AZ);
// AccelStepper motorAlt(AccelStepper::DRIVER, STEP_ALT, DIR_ALT);
// WebServer server(80);

// // =================== REDE ===================
// // Troque para sua rede se necessário
// const char* ssid = "iagorana";
// const char* password = "ranaeiago610";

// // Declarar a função que configura as rotas no outro arquivo
// void configurarBuscarAstro();

// void setup() {
//   Serial.begin(115200);
//   Serial.println();
//   Serial.println("[BOOT] Iniciando ESP32...");

//   // ----- Wi-Fi -----
//   Serial.printf("[WIFI] Conectando a \"%s\" ...\n", ssid);
//   WiFi.mode(WIFI_STA);
//   WiFi.begin(ssid, password);

//   int tentativas = 0;
//   while (WiFi.status() != WL_CONNECTED && tentativas < 30) {
//     delay(500);
//     Serial.print(".");
//     tentativas++;
//   }
//   Serial.println();
//   if (WiFi.status() == WL_CONNECTED) {
//     Serial.println("[WIFI] Conectado!");
//     Serial.print("[WIFI] IP: ");
//     Serial.println(WiFi.localIP());
//   } else {
//     Serial.println("[WIFI][ERRO] Não conectou. Verifique SSID/senha.");
//     // Você pode seguir sem Wi-Fi se quiser, mas a rota HTTP não vai responder.
//   }

//   // ----- Motores -----
//   motorAz.setMaxSpeed(500);     // ajuste fino depois
//   motorAz.setAcceleration(40);  // ajuste fino depois

//   motorAlt.setMaxSpeed(500);
//   motorAlt.setAcceleration(40);

//   // Direções (mantive como você tinha, pois relatou que a direção estava correta)
//   // (dirInvert, stepInvert)
//   motorAz.setPinsInverted(true,  false);
//   motorAlt.setPinsInverted(false, false);

//   motorAz.setCurrentPosition(0);
//   motorAlt.setCurrentPosition(0);

//   // ----- HTTP -----
//   configurarBuscarAstro();   // define a rota /mover lá no buscarAstro.cpp
//   server.begin();
//   Serial.println("[HTTP] Servidor iniciado. Rota: GET /mover?az=GRAUS&alt=GRAUS");
// }

// void loop() {
//   server.handleClient();
//   motorAz.run();   // mantém o movimento até atingir moveTo
//   motorAlt.run();
// }

// Versão boa 01
// #include <WiFi.h>
// #include <AccelStepper.h>
// #include <WebServer.h>

// // =================== AJUSTES DO SEU HARDWARE ===================
// #define DIR_AZ   17
// #define STEP_AZ  16
// #define DIR_ALT  19
// #define STEP_ALT 18

// // Passo do motor e microstepping
// static const int   STEPS_PER_REV = 3200;   // NEMA17 comum = 200 passos por volta
// static const int   MICROSTEPPING = 16;    // A4988 em 1/16 (MS1, MS2, MS3 em HIGH)
// static const float GEAR_RATIO    = 1.0f;  // Se tiver redução mecânica, ajuste aqui

// // passos por grau = (passos por volta * microstepping * relação) / 360
// float passosPorGrau = (STEPS_PER_REV * MICROSTEPPING * GEAR_RATIO) / 360.0f;

// // =================== GLOBAIS VISÍVEIS EM OUTROS ARQUIVOS ===================
// long ultimaMetaAzPassos = 0;
// long ultimaMetaAltPassos = 0;

// // Motores e servidor (exportados via extern no buscarAstro.cpp)
// AccelStepper motorAz(AccelStepper::DRIVER, STEP_AZ, DIR_AZ);
// AccelStepper motorAlt(AccelStepper::DRIVER, STEP_ALT, DIR_ALT);
// WebServer server(80);

// // =================== REDE ===================
// // Troque para sua rede se necessário
// const char* ssid = "iagorana";
// const char* password = "ranaeiago610";

// // Declarar a função que configura as rotas no outro arquivo
// void configurarBuscarAstro();

// void setup() {
//   Serial.begin(115200);
//   Serial.println();
//   Serial.println("[BOOT] Iniciando ESP32...");

//   // ----- Wi-Fi -----
//   Serial.printf("[WIFI] Conectando a \"%s\" ...\n", ssid);
//   WiFi.mode(WIFI_STA);
//   WiFi.begin(ssid, password);

//   int tentativas = 0;
//   while (WiFi.status() != WL_CONNECTED && tentativas < 30) {
//     delay(500);
//     Serial.print(".");
//     tentativas++;
//   }
//   Serial.println();
//   if (WiFi.status() == WL_CONNECTED) {
//     Serial.println("[WIFI] Conectado!");
//     Serial.print("[WIFI] IP: ");
//     Serial.println(WiFi.localIP());
//   } else {
//     Serial.println("[WIFI][ERRO] Não conectou. Verifique SSID/senha.");
//     // Você pode seguir sem Wi-Fi se quiser, mas a rota HTTP não vai responder.
//   }

//   // ----- Motores -----
//   motorAz.setMaxSpeed(500);     // ajuste fino depois
//   motorAz.setAcceleration(40);  // ajuste fino depois

//   motorAlt.setMaxSpeed(500);
//   motorAlt.setAcceleration(40);

//   // Se a direção estiver invertida no seu conjunto, troque true/false
//   motorAz.setPinsInverted(true, false);  // (dirInvert, stepInvert)
//   motorAlt.setPinsInverted(false, false);

//   motorAz.setCurrentPosition(0);
//   motorAlt.setCurrentPosition(0);

//   // ----- HTTP -----
//   configurarBuscarAstro();   // define a rota /mover lá no buscarAstro.cpp
//   server.begin();
//   Serial.println("[HTTP] Servidor iniciado. Rota: GET /mover?az=GRAUS&alt=GRAUS");
// }

// void loop() {
//   server.handleClient();
//   motorAz.run();   // mantém o movimento até atingir moveTo
//   motorAlt.run();
// }

//--------------------------------------------------------------------------------------

// #include <Arduino.h>
// #include <WiFi.h>
// #include <WebServer.h>
// #include <AccelStepper.h>

// // ========== Pinos dos motores (ajuste se necessário) ==========
// #define DIR_AZ  17
// #define STEP_AZ 16
// #define DIR_ALT 19
// #define STEP_ALT 18

// // ========== Objetos globais essenciais ==========
// WebServer server(80);
// AccelStepper motorAz(AccelStepper::DRIVER, STEP_AZ, DIR_AZ);
// AccelStepper motorAlt(AccelStepper::DRIVER, STEP_ALT, DIR_ALT);

// // ========== Wi-Fi (temporário; depois movemos p/ config) ==========
// const char* ssid     = "PEDRO HENRIQUE";
// const char* password = "20240204";

// void configurarBuscarAstro();

// void setup() {
//   // ---- Serial ----
//   Serial.begin(115200);
//   delay(50);

//   // ---- Motores: limites seguros (ajuste depois) ----
//   motorAz.setMaxSpeed(250);     // passos/s
//   motorAz.setAcceleration(30);  // passos/s^2
//   motorAlt.setMaxSpeed(700);
//   motorAlt.setAcceleration(50);

//   // Zera posição lógica
//   motorAz.setCurrentPosition(0);
//   motorAlt.setCurrentPosition(0);

//   // Inversão de pinos se necessário (direction, step, enable)
//   motorAz.setPinsInverted(false, true);
//   motorAlt.setPinsInverted(true, false);

//   // ---- Wi-Fi ----
//   Serial.print("[WiFi] Conectando");
//   WiFi.begin(ssid, password);
//   int attempts = 0;
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//     if (++attempts > 20) {
//       Serial.println("\n[WiFi] Falha ao conectar (seguiremos sem rotas por enquanto).");
//       break; // não dá return pra não travar o loop
//     }
//   }

//   if (WiFi.status() == WL_CONNECTED) {
//     Serial.println("\n[WiFi] Conectado!");
//     Serial.print("[WiFi] IP: "); Serial.println(WiFi.localIP());
//   }

//   // ---- Servidor HTTP ----
//   // (Sem rotas por enquanto; adicionaremos depois)
//   server.begin();
//   Serial.println("[HTTP] Servidor iniciado (sem rotas ainda).");
// }

// void loop() {
//   // Processa requisições (se houver); sem rotas, apenas mantém o servidor vivo
//   server.handleClient();

//   // Mantém os motores andando suave até seus targets (quando formos definir)
//   motorAz.run();
//   motorAlt.run();

//   // Nada bloqueante aqui. Adicionaremos timers/rotas nos próximos passos.
// }

// #include <WiFi.h>
// #include <AccelStepper.h>
// #include <WebServer.h>

// const float passosPorGrau = 30;
// long novaPosicaoAz;
// long novaPosicaoAlt;
// float deltaAz;
// float deltaAlt;

//  //Definindo os pinos dos motores
// #define DIR_AZ 17
// #define STEP_AZ 16
// #define DIR_ALT 19
// #define STEP_ALT 18

// AccelStepper motorAz(AccelStepper::DRIVER, STEP_AZ, DIR_AZ);
// AccelStepper motorAlt(AccelStepper::DRIVER, STEP_ALT, DIR_ALT);
// WebServer server(80);

// const char* ssid = "PEDRO HENRIQUE";  // Substitua pelo seu SSID
// const char* password = "20240204";  // Substitua pela sua senha

// void setup() {
//   Serial.begin(115200);  // Configura o monitor serial

//   WiFi.begin(ssid, password);
//   Serial.print("Conectando ao Wi-Fi");

//   // Espera até a ESP32 conectar ao Wi-Fi
//   int attempts = 0;
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//     attempts++;

//     // Limita o número de tentativas de conexão (para evitar loop infinito)
//     if (attempts > 20) {
//       Serial.println("\n[ERRO] Não foi possível conectar ao Wi-Fi.");
//       return;  // Sai do código caso não consiga conectar
//     }
//   }

//   // Exibe o IP no monitor serial
//   Serial.println("\n[✓] Conectado ao Wi-Fi!");
//   Serial.print("IP da ESP32: ");
//   Serial.println(WiFi.localIP());  // Exibe o IP

//   // Configuração dos motores
//   motorAz.setMaxSpeed(250);
//   motorAz.setAcceleration(30);
//   motorAlt.setMaxSpeed(700);
//   motorAlt.setAcceleration(50);

//   motorAz.setCurrentPosition(0);
//   motorAlt.setCurrentPosition(0);

//   // Inverter a direção dos motores (se necessário)
//   motorAz.setPinsInverted(false, true);  // Inverte a direção do motor de azimute
//   motorAlt.setPinsInverted(true, false); // Inverte a direção do motor de altitude

//   //Configuração das rotas diretamente no setup()
//   server.on("/mover", HTTP_GET, []() {
//     if (server.hasArg("az") && server.hasArg("alt")) {
//       float az = server.arg("az").toFloat();  // Obtém o azimute
//       float alt = server.arg("alt").toFloat();  // Obtém a altitude

//       // Converte azimute e altitude para passos, invertendo a direção se necessário
//       long novaPosAz = az * passosPorGrau;  // Inverte direção se necessário
//       long novaPosAlt = alt * passosPorGrau; // Inverte direção se necessário

//       motorAz.moveTo(novaPosAz);
//       motorAlt.moveTo(novaPosAlt);

//       novaPosicaoAz = novaPosAz;
//       novaPosicaoAlt = novaPosAlt;

//       // Atualiza a posição atual
//       Serial.println("[MOVIMENTO] Mover para AZ: " + String(az) + "°, ALT: " + String(alt) + "°");
//       server.send(200, "text/plain", "Movendo para AZ: " + String(az) + "°, ALT: " + String(alt) + "°");
//     } else {
//       server.send(400, "text/plain", "Parâmetros ausentes (az, alt)");
//     }
//   });

//   server.begin();  // Inicia o servidor
//   Serial.println("[✓] Servidor HTTP iniciado!");
// }

// void loop() {
//   server.handleClient();  // Handle incoming requests
//   motorAz.run();  // Mover o motor de azimute
//   motorAlt.run(); // Mover o motor de altitude
// }

// #include <WiFi.h>
// #include <AccelStepper.h>
// #include <WebServer.h>

// // Definindo os pinos dos motores
// #define DIR_AZ 17
// #define STEP_AZ 16
// #define DIR_ALT 19
// #define STEP_ALT 18

// const float passosPorGrau = 200.0 / 360.0;  // Calcula os passos por grau (200 passos por rotação de 360°)

// AccelStepper motorAz(AccelStepper::DRIVER, STEP_AZ, DIR_AZ);
// AccelStepper motorAlt(AccelStepper::DRIVER, STEP_ALT, DIR_ALT);
// WebServer server(80);

// const char* ssid = "PEDRO HENRIQUE";  // Substitua pelo seu SSID
// const char* password = "20240204";  // Substitua pela sua senha

// void setup() {
//   Serial.begin(115200);  // Configura o monitor serial

//   WiFi.begin(ssid, password);
//   Serial.print("Conectando ao Wi-Fi");

//   // Espera até a ESP32 conectar ao Wi-Fi
//   int attempts = 0;
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//     attempts++;

//     // Limita o número de tentativas de conexão (para evitar loop infinito)
//     if (attempts > 20) {
//       Serial.println("\n[ERRO] Não foi possível conectar ao Wi-Fi.");
//       return;  // Sai do código caso não consiga conectar
//     }
//   }

//   // Exibe o IP no monitor serial
//   Serial.println("\n[✓] Conectado ao Wi-Fi!");
//   Serial.print("IP da ESP32: ");
//   Serial.println(WiFi.localIP());  // Exibe o IP

//   // Configuração dos motores
//   motorAz.setMaxSpeed(500);
//   motorAz.setAcceleration(100);
//   motorAlt.setMaxSpeed(500);
//   motorAlt.setAcceleration(100);

//   motorAz.setCurrentPosition(0);
//   motorAlt.setCurrentPosition(0);

//   // Inverter a direção dos motores (se necessário)
//   motorAz.setPinsInverted(true, false);  // Inverte a direção do motor de azimute
//   motorAlt.setPinsInverted(true, false); // Inverte a direção do motor de altitude

//   // Configuração das rotas diretamente no setup()
//   server.on("/mover", HTTP_GET, []() {
//     if (server.hasArg("az") && server.hasArg("alt")) {
//       float az = server.arg("az").toFloat();  // Obtém o azimute
//       float alt = server.arg("alt").toFloat();  // Obtém a altitude

//       // Converte azimute e altitude para passos
//       long novaPosAz = -az * passosPorGrau;  // Inverte direção se necessário
//       long novaPosAlt = -alt * passosPorGrau; // Inverte direção se necessário

//       motorAz.moveTo(novaPosAz);
//       motorAlt.moveTo(novaPosAlt);

//       // Atualiza a posição atual
//       Serial.println("[MOVIMENTO] Mover para AZ: " + String(az) + "°, ALT: " + String(alt) + "°");
//       server.send(200, "text/plain", "Movendo para AZ: " + String(az) + "°, ALT: " + String(alt) + "°");
//     } else {
//       server.send(400, "text/plain", "Parâmetros ausentes (az, alt)");
//     }
//   });

//   server.begin();  // Inicia o servidor
//   Serial.println("[✓] Servidor HTTP iniciado!");
// }

// void loop() {
//   server.handleClient();  // Handle incoming requests
//   motorAz.run();  // Mover o motor de azimute
//   motorAlt.run(); // Mover o motor de altitude
// }

// #include <WiFi.h>
// #include <AccelStepper.h>
// #include <WebServer.h>

// // Definindo os pinos dos motores
// #define DIR_AZ 17
// #define STEP_AZ 16
// #define DIR_ALT 19
// #define STEP_ALT 18

// const float passosPorGrau = 200.0 / 360.0;  // Calcula os passos por grau (200 passos por rotação de 360°)

// AccelStepper motorAz(AccelStepper::DRIVER, STEP_AZ, DIR_AZ);
// AccelStepper motorAlt(AccelStepper::DRIVER, STEP_ALT, DIR_ALT);
// WebServer server(80);

// const char* ssid = "PEDRO HENRIQUE";  // Substitua pelo seu SSID
// const char* password = "20240204";  // Substitua pela sua senha

// void setup() {
//   Serial.begin(115200);  // Configura o monitor serial

//   WiFi.begin(ssid, password);
//   Serial.print("Conectando ao Wi-Fi");

//   // Espera até a ESP32 conectar ao Wi-Fi
//   int attempts = 0;
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//     attempts++;

//     // Limita o número de tentativas de conexão (para evitar loop infinito)
//     if (attempts > 20) {
//       Serial.println("\n[ERRO] Não foi possível conectar ao Wi-Fi.");
//       return;  // Sai do código caso não consiga conectar
//     }
//   }

//   // Exibe o IP no monitor serial
//   Serial.println("\n[✓] Conectado ao Wi-Fi!");
//   Serial.print("IP da ESP32: ");
//   Serial.println(WiFi.localIP());  // Exibe o IP

//   // Configuração dos motores
//   motorAz.setMaxSpeed(500);
//   motorAz.setAcceleration(100);
//   motorAlt.setMaxSpeed(500);
//   motorAlt.setAcceleration(100);

//   motorAz.setCurrentPosition(0);
//   motorAlt.setCurrentPosition(0);

//   // Configuração das rotas diretamente no setup()
//   server.on("/mover", HTTP_GET, []() {
//     if (server.hasArg("az") && server.hasArg("alt")) {
//       float az = server.arg("az").toFloat();  // Obtém o azimute
//       float alt = server.arg("alt").toFloat();  // Obtém a altitude

//       // Converte azimute e altitude para passos
//       long novaPosAz = az * passosPorGrau;
//       long novaPosAlt = alt * passosPorGrau;

//       motorAz.moveTo(novaPosAz);
//       motorAlt.moveTo(novaPosAlt);

//       // Atualiza a posição atual
//       Serial.println("[MOVIMENTO] Mover para AZ: " + String(az) + "°, ALT: " + String(alt) + "°");
//       server.send(200, "text/plain", "Movendo para AZ: " + String(az) + "°, ALT: " + String(alt) + "°");
//     } else {
//       server.send(400, "text/plain", "Parâmetros ausentes (az, alt)");
//     }
//   });

//   server.begin();  // Inicia o servidor
//   Serial.println("[✓] Servidor HTTP iniciado!");
// }

// void loop() {
//   server.handleClient();  // Handle incoming requests
//   motorAz.run();  // Mover o motor de azimute
//   motorAlt.run(); // Mover o motor de altitude
// }
