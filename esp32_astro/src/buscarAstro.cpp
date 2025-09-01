#include <WebServer.h>
#include <AccelStepper.h>
#include <math.h> // lround

// --- ACUMULADORES DE FRAÇÃO DE PASSO (para precisão ultra fina) ---
static double accAzFrac  = 0.0;  // acumula frações de passo no AZ
static double accAltFrac = 0.0;  // acumula frações de passo no ALT
// ====== IMPORTA AS GLOBAIS DO main.cpp ======
extern WebServer    server;
extern AccelStepper motorAz;
extern AccelStepper motorAlt;

extern float PASSOS_POR_GRAU_AZ;
extern float PASSOS_POR_GRAU_ALT;

extern long ultimaMetaAzPassos;
extern long ultimaMetaAltPassos;


// ====== CONFIGURA A ROTA /mover ======
void configurarBuscarAstro() {
  server.on("/mover", HTTP_GET, []() {
    if (!server.hasArg("az") || !server.hasArg("alt")) {
      server.send(400, "text/plain", "Parâmetros ausentes (az, alt)");
      return;
    }

    // 1) Lê graus enviados pelo Python (Skyfield)
    const float grausAz  = server.arg("az").toFloat();
    const float grausAlt = server.arg("alt").toFloat();

    // 2) Converte para passos absolutos (0° alinhado na partida)
    // 2) Converte para passos absolutos com ACÚMULO DE FRAÇÃO (ultra fino)
double stepsAzDesired  = (double)grausAz  * (double)PASSOS_POR_GRAU_AZ;
double stepsAltDesired = (double)grausAlt * (double)PASSOS_POR_GRAU_ALT;

// soma as sobras acumuladas de ciclos anteriores
stepsAzDesired  += accAzFrac;
stepsAltDesired += accAltFrac;

// arredonda para inteiro mais próximo
long alvoAzPassos  = (long)llround(stepsAzDesired);
long alvoAltPassos = (long)llround(stepsAltDesired);

// atualiza as sobras (fração que “sobrou” após o arredondamento)
accAzFrac  = stepsAzDesired  - (double)alvoAzPassos;   // faixa ~(-0.5 .. +0.5)
accAltFrac = stepsAltDesired - (double)alvoAltPassos;


    // 3) Move para a meta absoluta (AccelStepper cuida do trajeto)
    if (alvoAzPassos != motorAz.targetPosition())
        motorAz.moveTo(alvoAzPassos);

    if (alvoAltPassos != motorAlt.targetPosition())
        motorAlt.moveTo(alvoAltPassos);


    // 4) Guarda última meta (útil pra debug)
    ultimaMetaAzPassos  = alvoAzPassos;
    ultimaMetaAltPassos = alvoAltPassos;

    // 5) Log e resposta
    String msg = "Movendo para AZ: " + String(grausAz, 3) + "° (" + String(alvoAzPassos) +
                 " passos), ALT: " + String(grausAlt, 3) + "° (" + String(alvoAltPassos) + " passos)";
    Serial.println("[/mover] " + msg);
    server.send(200, "text/plain", msg);
  });

  // (Opcional) Rota de saúde
  server.on("/ping", HTTP_GET, []() {
    server.send(200, "text/plain", "pong");
  });
}


//Versão boa 02
// #include <WebServer.h>
// #include <AccelStepper.h>
// #include <math.h> // lround

// // ====== IMPORTA AS GLOBAIS DO main.cpp ======
// extern WebServer    server;
// extern AccelStepper motorAz;
// extern AccelStepper motorAlt;

// extern float PASSOS_POR_GRAU_AZ;
// extern float PASSOS_POR_GRAU_ALT;

// extern long ultimaMetaAzPassos;
// extern long ultimaMetaAltPassos;

// // ====== CONFIGURA A ROTA /mover ======
// void configurarBuscarAstro() {
//   server.on("/mover", HTTP_GET, []() {
//     if (!server.hasArg("az") || !server.hasArg("alt")) {
//       server.send(400, "text/plain", "Parâmetros ausentes (az, alt)");
//       return;
//     }

//     // 1) Lê graus enviados pelo Python (Skyfield)
//     const float grausAz  = server.arg("az").toFloat();
//     const float grausAlt = server.arg("alt").toFloat();

//     // 2) Converte para passos absolutos (0° alinhado na partida)
//     const long alvoAzPassos  = static_cast<long>(lround(grausAz  * PASSOS_POR_GRAU_AZ));
//     const long alvoAltPassos = static_cast<long>(lround(grausAlt * PASSOS_POR_GRAU_ALT));

//     // 3) Move para a meta absoluta (AccelStepper cuida do trajeto)
//     motorAz.moveTo(alvoAzPassos);
//     motorAlt.moveTo(alvoAltPassos);

//     // 4) Guarda última meta (útil pra debug)
//     ultimaMetaAzPassos  = alvoAzPassos;
//     ultimaMetaAltPassos = alvoAltPassos;

//     // 5) Log e resposta
//     String msg = "Movendo para AZ: " + String(grausAz, 3) + "° (" + String(alvoAzPassos) +
//                  " passos), ALT: " + String(grausAlt, 3) + "° (" + String(alvoAltPassos) + " passos)";
//     Serial.println("[/mover] " + msg);
//     server.send(200, "text/plain", msg);
//   });

//   // (Opcional) Rota de saúde
//   server.on("/ping", HTTP_GET, []() {
//     server.send(200, "text/plain", "pong");
//   });
// }



//Versão boa 01
// #include <WebServer.h>
// #include <AccelStepper.h>

// // ====== IMPORTA AS GLOBAIS DO main.cpp ======
// extern WebServer   server;
// extern AccelStepper motorAz;
// extern AccelStepper motorAlt;

// extern float passosPorGrau;      // precisa ser const aqui também
// extern long ultimaMetaAzPassos;
// extern long ultimaMetaAltPassos;

// // ====== CONFIGURA A ROTA /mover ======
// void configurarBuscarAstro() {
//   server.on("/mover", HTTP_GET, []() {
//     if (!server.hasArg("az") || !server.hasArg("alt")) {
//       server.send(400, "text/plain", "Parâmetros ausentes (az, alt)");
//       return;
//     }

//     // 1) Lê graus enviados pelo Python (Skyfield)
//     const float grausAz  = server.arg("az").toFloat();
//     const float grausAlt = server.arg("alt").toFloat();

//     // 2) Converte para passos absolutos (0° alinhado na partida)
//     const long alvoAzPassos  = (long)(grausAz  * passosPorGrau);
//     const long alvoAltPassos = (long)(grausAlt * passosPorGrau);

//     // 3) Move para a meta absoluta (AccelStepper cuida do trajeto)
//     motorAz.moveTo(alvoAzPassos);
//     motorAlt.moveTo(alvoAltPassos);

//     // 4) Guarda última meta (opcional, útil pra debug)
//     ultimaMetaAzPassos  = alvoAzPassos;
//     ultimaMetaAltPassos = alvoAltPassos;

//     // 5) Log e resposta
//     String msg = "Movendo para AZ: " + String(grausAz, 3) + "° (" + String(alvoAzPassos) +
//                  " passos), ALT: " + String(grausAlt, 3) + "° (" + String(alvoAltPassos) + " passos)";
//     Serial.println("[/mover] " + msg);
//     server.send(200, "text/plain", msg);
//   });

//   // (Opcional) Rota de saúde
//   server.on("/ping", HTTP_GET, []() {
//     server.send(200, "text/plain", "pong");
//   });
// }

//--------------------------------------------------------------------------------------

// #include <Arduino.h>
// #include <WebServer.h>
// #include <AccelStepper.h>
// #include "buscarAstro.h"

// // Pegamos do main.cpp (são globais lá)
// extern WebServer server;
// extern AccelStepper motorAz;
// extern AccelStepper motorAlt;
// extern const float passosPorGrau;

// // Rota: /mover?az=XX.xx&alt=YY.yy  (em GRAUS)
// void configurarBuscarAstro() {
//   server.on("/mover", []() {
//     if (!server.hasArg("az") || !server.hasArg("alt")) {
//       server.send(400, "text/plain", "Parâmetros ausentes (az, alt)");
//       return;
//     }

//     const float azGraus  = server.arg("az").toFloat();
//     const float altGraus = server.arg("alt").toFloat();

//     // Converte graus -> passos
//     const long alvoAzPassos  = lroundf(azGraus  * passosPorGrau);
//     const long alvoAltPassos = lroundf(altGraus * passosPorGrau);

//     // Agenda movimento (quem move de verdade é o run() no loop)
//     motorAz.moveTo(alvoAzPassos);
//     motorAlt.moveTo(alvoAltPassos);

//     // Logs e resposta
//     Serial.printf("[/mover] AZ: %.2f° -> %ld passos | ALT: %.2f° -> %ld passos\n",
//                   azGraus, alvoAzPassos, altGraus, alvoAltPassos);

//     server.send(200, "text/plain",
//       "Movendo para AZ: " + String(azGraus) + "°, ALT: " + String(altGraus) + "°");
//   });
// }




// #include <WebServer.h>
// #include <AccelStepper.h>

// // Usa variáveis do main.cpp
// extern WebServer server;
// extern AccelStepper motorAz;
// extern AccelStepper motorAlt;

// extern float passosPorGrau;
// extern long novaPosicaoAz;
// extern long novaPosicaoAlt;

// extern float deltaAz;
// extern float deltaAlt;

// void configurarBuscarAstro() {
//   server.on("/mover", []() {
//     if (server.hasArg("az") && server.hasArg("alt")) {
//       float grausAz = server.arg("az").toFloat();
//       float grausAlt = server.arg("alt").toFloat();

//       float deltaAz = grausAz - novaPosicaoAz;
//       float deltaAlt = grausAlt - novaPosicaoAlt;

//       // Inverte direção se necessário
//       float passosAz = deltaAz * passosPorGrau;  // Inverte a direção do motor de azimute
//       float passosAlt = deltaAlt * passosPorGrau;
      
//       //Inverte a direção do motor de altitude

//       motorAz.moveTo(passosAz);
//       motorAlt.moveTo(passosAlt);

//       novaPosicaoAz = grausAz;
//       novaPosicaoAlt = grausAlt;

//       server.send(200, "text/plain", "Movendo para AZ: " + String(grausAz) + "°, ALT: " + String(grausAlt) + "°");
//     } else {
//       server.send(400, "text/plain", "Parâmetros ausentes (az, alt)");
//     }
//   });
// }



// #include <WebServer.h>
// #include <AccelStepper.h>

// // Usa variáveis do main.cpp
// extern WebServer server;
// extern AccelStepper motorAz;
// extern AccelStepper motorAlt;

// const float passosPorGrau = 3200.0 / 360.0;
// extern long posicaoAtualAz;
// extern long posicaoAtualAlt;

// void configurarBuscarAstro() {
//   server.on("/mover", []() {
//     if (server.hasArg("az") && server.hasArg("alt")) {
//       float grausAz = server.arg("az").toFloat();
//       float grausAlt = server.arg("alt").toFloat();

//       long novaPosAz = grausAz * passosPorGrau;
//       long novaPosAlt = grausAlt * passosPorGrau;

//       motorAz.moveTo(novaPosAz);
//       motorAlt.moveTo(novaPosAlt);

//       posicaoAtualAz = novaPosAz;
//       posicaoAtualAlt = novaPosAlt;

//       server.send(200, "text/plain", "Movendo para AZ: " + String(grausAz) + "°, ALT: " + String(grausAlt) + "°");
//     } else {
//       server.send(400, "text/plain", "Parâmetros ausentes (az, alt)");
//     }
//   });
// }
