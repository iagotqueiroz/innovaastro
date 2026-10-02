#include <WebServer.h>
#include <AccelStepper.h>
#include <math.h> // llround

// ====== IMPORTA AS GLOBAIS DO main.cpp ======
extern WebServer server;
extern AccelStepper motorAz;
extern AccelStepper motorAlt;

extern float PASSOS_POR_GRAU_AZ;
extern float PASSOS_POR_GRAU_ALT;

extern long ultimaMetaAzPassos;
extern long ultimaMetaAltPassos;

extern volatile bool g_homingAz;
extern volatile bool g_homingAlt;

extern bool limiteAzAcionado();
extern bool limiteAltAcionado();

extern void pararAzImediato();
extern void pararAltImediato();

// ============================================
// LIMITES MECÂNICOS SEGUROS
// ============================================

static const float AZ_MIN_DEG = 0.0f;
static const float AZ_MAX_DEG = 350.0f;

static const float ALT_MIN_DEG = 0.0f;
static const float ALT_MAX_DEG = 90.0f;

// No HOME mecânico, o tubo está
// aproximadamente 27° abaixo do horizonte.
static const float ALT_HOME_DEG = -27.0f;

// Flag global (definida aqui, usada no main.cpp)
volatile bool g_tracking = false;

// ================================
// AJUSTE FINO DURANTE O TRACKING
// ================================

// Velocidade normal calculada pelo Skyfield,
// já convertida para passos/s
static float g_trackSpeedAzSteps = 0.0f;
static float g_trackSpeedAltSteps = 0.0f;

// Velocidade usada somente durante o NUDGE.
// Em bancada continua bem abaixo do limite do motor.
static const float NUDGE_SPEED_DEG_S = 9.0f;

// Estado do ajuste fino AZ
static bool g_nudgeAzActive = false;
static long g_nudgeAzTarget = 0;
static int g_nudgeAzDirection = 0;

// Estado do ajuste fino ALT
static bool g_nudgeAltActive = false;
static long g_nudgeAltTarget = 0;
static int g_nudgeAltDirection = 0;

static void aplicarVelocidadesTracking()
{
    float velocidadeAz = g_trackSpeedAzSteps;
    float velocidadeAlt = g_trackSpeedAltSteps;

    if (g_nudgeAzActive)
    {
        velocidadeAz +=
            g_nudgeAzDirection *
            NUDGE_SPEED_DEG_S *
            PASSOS_POR_GRAU_AZ;
    }

    if (g_nudgeAltActive)
    {
        velocidadeAlt +=
            g_nudgeAltDirection *
            NUDGE_SPEED_DEG_S *
            PASSOS_POR_GRAU_ALT;
    }

    motorAz.setSpeed(velocidadeAz);
    motorAlt.setSpeed(velocidadeAlt);
}

void cancelarNudgeTracking()
{
    g_nudgeAzActive = false;
    g_nudgeAltActive = false;

    g_nudgeAzDirection = 0;
    g_nudgeAltDirection = 0;
}

void atualizarTracking()
{
    // ========================
    // Verifica fim do NUDGE AZ
    // ========================

    if (g_nudgeAzActive)
    {
        long atual = motorAz.currentPosition();

        bool chegou =
            (g_nudgeAzDirection > 0 && atual >= g_nudgeAzTarget) ||
            (g_nudgeAzDirection < 0 && atual <= g_nudgeAzTarget);

        if (chegou)
        {
            g_nudgeAzActive = false;
            g_nudgeAzDirection = 0;

            Serial.println("[NUDGE] AZ concluído.");
        }
    }

    // =========================
    // Verifica fim do NUDGE ALT
    // =========================

    if (g_nudgeAltActive)
    {
        long atual = motorAlt.currentPosition();

        bool chegou =
            (g_nudgeAltDirection > 0 && atual >= g_nudgeAltTarget) ||
            (g_nudgeAltDirection < 0 && atual <= g_nudgeAltTarget);

        if (chegou)
        {
            g_nudgeAltActive = false;
            g_nudgeAltDirection = 0;

            Serial.println("[NUDGE] ALT concluído.");
        }
    }

    // Combina:
    // tracking astronômico + eventual ajuste manual
    aplicarVelocidadesTracking();


    // ============================================
    // SOFT LIMIT DURANTE TRACKING / NUDGE
    // ============================================

    float azAtualDeg =
        -motorAz.currentPosition() /
        PASSOS_POR_GRAU_AZ;

    float altAtualDeg = ALT_HOME_DEG + motorAlt.currentPosition() / PASSOS_POR_GRAU_ALT;


    // AZ:
    //
    // motor positivo = diminui AZ astronômico
    // motor negativo = aumenta AZ astronômico

    bool azSaindoPeloMinimo =
        azAtualDeg <= AZ_MIN_DEG &&
        motorAz.speed() > 0.0f;

    bool azSaindoPeloMaximo =
        azAtualDeg >= AZ_MAX_DEG &&
        motorAz.speed() < 0.0f;


    // ALT:
    //
    // motor negativo = diminui ALT
    // motor positivo = aumenta ALT

    bool altSaindoPeloMinimo =
        altAtualDeg <= ALT_MIN_DEG &&
        motorAlt.speed() < 0.0f;

    bool altSaindoPeloMaximo =
        altAtualDeg >= ALT_MAX_DEG &&
        motorAlt.speed() > 0.0f;


    if (
        azSaindoPeloMinimo ||
        azSaindoPeloMaximo ||
        altSaindoPeloMinimo ||
        altSaindoPeloMaximo
    )
    {
        pararAzImediato();
        pararAltImediato();

        g_trackSpeedAzSteps = 0.0f;
        g_trackSpeedAltSteps = 0.0f;

        cancelarNudgeTracking();

        g_tracking = false;

        Serial.println(
            "[SEGURANCA] Tracking interrompido por limite virtual."
        );

        return;
    }

    // ============================================
    // HARD LIMIT DURANTE TRACKING / NUDGE
    // ============================================

    // AZ:
    // velocidade positiva aponta para o switch.
    bool azTentandoEntrarNoLimite =
        limiteAzAcionado() &&
        motorAz.speed() > 0.0f;

    // ALT:
    // velocidade negativa aponta para o switch.
    bool altTentandoEntrarNoLimite =
        limiteAltAcionado() &&
        motorAlt.speed() < 0.0f;

    if (
        azTentandoEntrarNoLimite ||
        altTentandoEntrarNoLimite)
    {
        // Se qualquer eixo atingir um hard limit
        // durante tracking, interrompe todo o
        // acompanhamento por segurança.

        pararAzImediato();
        pararAltImediato();

        g_trackSpeedAzSteps = 0.0f;
        g_trackSpeedAltSteps = 0.0f;

        cancelarNudgeTracking();

        g_tracking = false;

        Serial.println(
            "[SEGURANCA] Tracking interrompido por fim de curso.");

        return;
    }

    motorAz.runSpeed();
    motorAlt.runSpeed();
}

// ====== CONFIGURA ROTAS ======
void configurarBuscarAstro()
{
    // --------- GoTo absoluto (usado só no início) ----------
    server.on("/mover", HTTP_GET, []()
              {
    if (!server.hasArg("az") || !server.hasArg("alt")) {
      server.send(400, "text/plain", "Parâmetros ausentes (az, alt)");
      return;
    }

    const float grausAz  = server.arg("az").toFloat();
    const float grausAlt = server.arg("alt").toFloat();

    
    // ============================================
    // VALIDA LIMITES MECÂNICOS
    // ============================================

    if (
        grausAz < AZ_MIN_DEG ||
        grausAz > AZ_MAX_DEG
    )
    {
        String erro =
            "{\"ok\":false,"
            "\"erro\":\"AZ fora da faixa segura\","
            "\"min\":0,"
            "\"max\":350}";

        Serial.println(
            "[SEGURANCA] GoTo AZ fora da faixa."
        );

        server.send(
            422,
            "application/json",
            erro
        );

        return;
    }


    if (
        grausAlt < ALT_MIN_DEG ||
        grausAlt > ALT_MAX_DEG
    )
    {
        String erro =
            "{\"ok\":false,"
            "\"erro\":\"ALT fora da faixa segura\","
            "\"min\":0,"
            "\"max\":90}";

        Serial.println(
            "[SEGURANCA] GoTo ALT fora da faixa."
        );

        server.send(
            422,
            "application/json",
            erro
        );

        return;
    }


    // ============================================
    // CONVERSÃO CÉU -> MOTOR
    // ============================================

    // AZ:
    // no telescópio real,
    // graus astronômicos positivos
    // correspondem a passos NEGATIVOS.
    //
    // Isso faz o eixo sair do HOME
    // para o lado seguro.
    long alvoAzPassos = (long)llround(
        -(double)grausAz *
        (double)PASSOS_POR_GRAU_AZ
    );


    // ALT:
    // graus positivos correspondem
    // a passos positivos.
    long alvoAltPassos = (long)llround(
        (
            (double)grausAlt -
            (double)ALT_HOME_DEG
        ) *
        (double)PASSOS_POR_GRAU_ALT
    );

    // Em GoTo, usamos controle de posição (run). Desliga tracking.
    g_homingAz = false;
    g_homingAlt = false;
    g_tracking = false;
    cancelarNudgeTracking();

    g_trackSpeedAzSteps = 0;
    g_trackSpeedAltSteps = 0;

    if (labs(alvoAzPassos  - motorAz.targetPosition())  >= 1) motorAz.moveTo(alvoAzPassos);
    if (labs(alvoAltPassos - motorAlt.targetPosition()) >= 1) motorAlt.moveTo(alvoAltPassos);

    ultimaMetaAzPassos  = alvoAzPassos;
    ultimaMetaAltPassos = alvoAltPassos;

    String msg = "GoTo AZ: " + String(grausAz, 3) + "° (" + String(alvoAzPassos) +
                 " passos), ALT: " + String(grausAlt, 3) + "° (" + String(alvoAltPassos) + " passos)";
    Serial.println("[/mover] " + msg);
    server.send(200, "text/plain", msg); });

    // --------- Seguimento por VELOCIDADE contínua ----------
    // Recebe velocidades em deg/s e ativa modo runSpeed()
    server.on("/set_speed", HTTP_GET, []()
              {
    if (!server.hasArg("vaz") || !server.hasArg("valt")) {
      server.send(400, "text/plain", "Parâmetros ausentes (vaz, valt) em deg/s");
      return;
    }

    const float vAz_deg_s  = server.arg("vaz").toFloat();   // deg/s
    const float vAlt_deg_s = server.arg("valt").toFloat();  // deg/s

    if (isnan(vAz_deg_s) || isnan(vAlt_deg_s)) {
      server.send(400, "text/plain", "vaz/valt invalidos");
      return;
    }

    // Converte deg/s -> passos/s
    const float vAz_steps_s  = -vAz_deg_s  * PASSOS_POR_GRAU_AZ;
    const float vAlt_steps_s = vAlt_deg_s * PASSOS_POR_GRAU_ALT;

    // Ativa modo tracking por velocidade
    g_homingAz = false;
    g_homingAlt = false;
    g_tracking = true;

    // Zera metas de posição (evita “puxões” residuais)
    motorAz.moveTo(motorAz.currentPosition());
    motorAlt.moveTo(motorAlt.currentPosition());

    g_trackSpeedAzSteps = vAz_steps_s;
    g_trackSpeedAltSteps = vAlt_steps_s;

    aplicarVelocidadesTracking();

    String msg = "speed AZ=" + String(vAz_deg_s, 6) + " deg/s (" + String(vAz_steps_s, 3) + " sps), "
                 "ALT=" + String(vAlt_deg_s, 6) + " deg/s (" + String(vAlt_steps_s, 3) + " sps)";
    Serial.println("[/set_speed] " + msg);
    server.send(200, "text/plain", msg); });

    server.on("/nudge", HTTP_GET, []()
              {

    if (!g_tracking)
    {
        server.send(
            409,
            "application/json",
            "{\"ok\":false,\"erro\":\"Tracking não está ativo.\"}"
        );

        return;
    }

    float deltaAz = 0.0f;
    float deltaAlt = 0.0f;

    if (server.hasArg("daz"))
    {
        deltaAz = server.arg("daz").toFloat();
    }

    if (server.hasArg("dalt"))
    {
        deltaAlt = server.arg("dalt").toFloat();
    }

    long deltaAzPassos = (long)llround(
        deltaAz * PASSOS_POR_GRAU_AZ
    );

    long deltaAltPassos = (long)llround(
        deltaAlt * PASSOS_POR_GRAU_ALT
    );

    if (deltaAzPassos == 0 && deltaAltPassos == 0)
    {
        server.send(
            400,
            "application/json",
            "{\"ok\":false,\"erro\":\"Ajuste menor que um passo do motor.\"}"
        );

        return;
    }


    // ============================================
    // VALIDA O ALVO DO NUDGE ANTES DE MOVIMENTAR
    // ============================================

    long baseAz =
        g_nudgeAzActive
            ? g_nudgeAzTarget
            : motorAz.currentPosition();

    long baseAlt =
        g_nudgeAltActive
            ? g_nudgeAltTarget
            : motorAlt.currentPosition();


    long novoAlvoAz =
        baseAz + deltaAzPassos;

    long novoAlvoAlt =
        baseAlt + deltaAltPassos;


    // Converte o possível novo alvo para
    // coordenadas astronômicas.
    float novoAzDeg =
        -novoAlvoAz /
        PASSOS_POR_GRAU_AZ;

    float novoAltDeg = ALT_HOME_DEG + novoAlvoAlt / PASSOS_POR_GRAU_ALT;


    // ---------- AZ ----------

    if (
        deltaAzPassos != 0 &&
        (
            novoAzDeg < AZ_MIN_DEG ||
            novoAzDeg > AZ_MAX_DEG
        )
    )
    {
        Serial.println(
            "[SEGURANCA] NUDGE AZ bloqueado por limite virtual."
        );

        server.send(
            422,
            "application/json",
            "{\"ok\":false,\"erro\":\"NUDGE AZ fora da faixa segura\"}"
        );

        return;
    }


    // ---------- ALT ----------

    if (
        deltaAltPassos != 0 &&
        (
            novoAltDeg < ALT_MIN_DEG ||
            novoAltDeg > ALT_MAX_DEG
        )
    )
    {
        Serial.println(
            "[SEGURANCA] NUDGE ALT bloqueado por limite virtual."
        );

        server.send(
            422,
            "application/json",
            "{\"ok\":false,\"erro\":\"NUDGE ALT fora da faixa segura\"}"
        );

        return;
    }


    // ============================================
    // HARD LIMIT ANTES DO NUDGE
    // ============================================

    // AZ positivo no motor = DIREITA = switch.
    if (
        deltaAzPassos > 0 &&
        limiteAzAcionado()
    )
    {
        server.send(
            409,
            "application/json",
            "{\"ok\":false,\"erro\":\"NUDGE AZ bloqueado pelo fim de curso\"}"
        );

        return;
    }


    // ALT negativo no motor = BAIXO = switch.
    if (
        deltaAltPassos < 0 &&
        limiteAltAcionado()
    )
    {
        server.send(
            409,
            "application/json",
            "{\"ok\":false,\"erro\":\"NUDGE ALT bloqueado pelo fim de curso\"}"
        );

        return;
    }


    // =========================
    // NUDGE AZ
    // =========================

    if (deltaAzPassos != 0)
    {
        g_nudgeAzTarget =
            novoAlvoAz;

        long restante =
            g_nudgeAzTarget -
            motorAz.currentPosition();

        if (restante > 0)
            g_nudgeAzDirection = 1;
        else if (restante < 0)
            g_nudgeAzDirection = -1;
        else
            g_nudgeAzDirection = 0;

        g_nudgeAzActive =
            (g_nudgeAzDirection != 0);
    }


    // =========================
    // NUDGE ALT
    // =========================

    if (deltaAltPassos != 0)
    {
        g_nudgeAltTarget =
            novoAlvoAlt;

        long restante =
            g_nudgeAltTarget -
            motorAlt.currentPosition();

        if (restante > 0)
            g_nudgeAltDirection = 1;
        else if (restante < 0)
            g_nudgeAltDirection = -1;
        else
            g_nudgeAltDirection = 0;

        g_nudgeAltActive =
            (g_nudgeAltDirection != 0);
    }


    aplicarVelocidadesTracking();


    Serial.printf(
        "[NUDGE] daz=%.3f° (%ld passos) | dalt=%.3f° (%ld passos)\n",
        deltaAz,
        deltaAzPassos,
        deltaAlt,
        deltaAltPassos
    );


    String json = "{";

    json += "\"ok\":true,";
    json += "\"deltaAzPassos\":" +
            String(deltaAzPassos) + ",";

    json += "\"deltaAltPassos\":" +
            String(deltaAltPassos);

    json += "}";

    server.send(
        200,
        "application/json",
        json
    ); });

    // (Opcional) Pausa o tracking (zera speed)
    server.on("/track_off", HTTP_GET, []()
              {

    g_tracking = false;

    cancelarNudgeTracking();

    g_trackSpeedAzSteps = 0;
    g_trackSpeedAltSteps = 0;

    motorAz.setSpeed(0);
    motorAlt.setSpeed(0);

    server.send(
        200,
        "text/plain",
        "tracking off"
    ); });

    server.on("/stop", HTTP_GET, []()
              {

    // Sai imediatamente do modo tracking
    g_tracking = false;
    cancelarNudgeTracking();

    g_trackSpeedAzSteps = 0;
    g_trackSpeedAltSteps = 0;
    g_homingAz = false;
    g_homingAlt = false;

    // Zera velocidades do runSpeed()
    motorAz.setSpeed(0);
    motorAlt.setSpeed(0);

    // Cancela qualquer GoTo pendente
    motorAz.moveTo(motorAz.currentPosition());
    motorAlt.moveTo(motorAlt.currentPosition());

    Serial.println("[STOP] Movimento interrompido.");

    server.send(200, "application/json",
                "{\"ok\":true,\"status\":\"stopped\"}"); });

    // Saúde
    server.on("/ping", HTTP_GET, []()
              { server.send(200, "text/plain", "pong"); });

    server.on("/motion_status", HTTP_GET, []()
              {

  long atualAz = motorAz.currentPosition();
  long atualAlt = motorAlt.currentPosition();

  long alvoAz = motorAz.targetPosition();
  long alvoAlt = motorAlt.targetPosition();

  long distanciaAz = motorAz.distanceToGo();
  long distanciaAlt = motorAlt.distanceToGo();


    float azGraus = -atualAz / PASSOS_POR_GRAU_AZ;

    float altGraus = ALT_HOME_DEG + atualAlt / PASSOS_POR_GRAU_ALT;

  String json = "{";

  json += "\"currentAz\":" + String(atualAz) + ",";
  json += "\"currentAlt\":" + String(atualAlt) + ",";

  json += "\"targetAz\":" + String(alvoAz) + ",";
  json += "\"targetAlt\":" + String(alvoAlt) + ",";

  json += "\"distanceAz\":" + String(distanciaAz) + ",";
  json += "\"distanceAlt\":" + String(distanciaAlt) + ",";

  json += "\"azDeg\":" + String(azGraus, 6) + ",";
  json += "\"altDeg\":" + String(altGraus, 6) + ",";

  json += "\"tracking\":";
  json += g_tracking ? "true" : "false";

  json += "}";

  server.send(200, "application/json", json); });
}

// Versão boa 02
//  #include <WebServer.h>
//  #include <AccelStepper.h>
//  #include <math.h> // lround

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

// Versão boa 01
//  #include <WebServer.h>
//  #include <AccelStepper.h>

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
