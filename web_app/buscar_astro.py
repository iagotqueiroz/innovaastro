from skyfield.api import load, Topos
from datetime import datetime, timezone, timedelta
from config import ESP32_IP
import requests
import threading
import time
import math

# === Efemérides (carrega 1x) ===
eph = load("de421.bsp")
terra = eph["earth"]
ts = load.timescale()

# === Config geral do seguimento ===
SEGUIMENTO_CFG = {
    # controle legado (mantido, mas o modo atual usa velocidade contínua)
    "gain": 1.0,
    "min_step_deg": 0.12,
    "max_step_deg": 0.5,
    "accum_az": 0.0,
    "accum_alt": 0.0,
    "last_cmd_az": None,
    "last_cmd_alt": None,
    "hysteresis_deg": 0.02,
    "send_precision": 4,   # ↑ precisão para não “empatar” deg muito pequenos
    # perfil por velocidade contínua
    "tick_s": 0.10,        # 10 Hz → movimento suave
    "rate_refresh_s": 1.0, # reestima velocidade a cada 1 s
    "rate_dt_s": 1.0,      # janela para estimar dθ/dt
    "max_rate_deg_s": 2.0, # trava de segurança (deg/s)
    "p_correction": 0.0,  # correção proporcional lenta (anti-drift)
    # compensação de atraso (lead time)
    # estime aqui sua velocidade de “slew” típica em deg/s (por eixo)
    "slew_deg_s_az": 8.0,  # ajuste ao seu conjunto
    "slew_deg_s_alt": 6.0, # ajuste ao seu conjunto
    "extra_net_delay_s": 0.05,  # rede + firmware (médio)
}

# === Estado do seguimento ===
_track_thread = None
_stop_event = threading.Event()
_track_state = {
    "running": False,
    "target": None,
    "lat": None,
    "lon": None,
    "interval": 3,
    "last_az": None,
    "last_alt": None,
    "last_sent_at": None,
    "errors": 0,
}

# =========================
# AJUSTE FINO DO TRACKING
# =========================

# Bancada:
# 1 passo do NEMA17 = 1.8°
#
# Quando tivermos redução/microstepping,
# diminuiremos este valor.
NUDGE_STEP_DEG = 1.8

# =========================
# LIMITES DE SEGURANÇA
# =========================

ALT_MIN_SEGURA = 0.0
ALT_MAX_SEGURA = 90.0

def _altitude_segura(altitude):
    return ALT_MIN_SEGURA <= altitude <= ALT_MAX_SEGURA


# =========================
# ESTADO DO ALINHAMENTO
# =========================

ALINHAMENTO = {
    "ativo": False,
    "astro": None,
    "az_ceu": None,
    "alt_ceu": None,
    "az_motor": None,
    "alt_motor": None,
    "offset_az": 0.0,
    "offset_alt": 0.0,
    "data": None,
}


# =========================
# HISTÓRICO DE ALINHAMENTOS
# =========================

HISTORICO_ALINHAMENTOS = []

MAX_ALINHAMENTOS = 20

# Pontos muito baixos no horizonte não serão usados
# para construir o modelo de alinhamento.
ALT_MIN_MODELO_ALINHAMENTO = 15.0

# === Helpers ===
def _norm360(x: float) -> float:
    return (x % 360.0 + 360.0) % 360.0

def _shortest_delta_deg(target: float, current: float) -> float:
    return (target - current + 540.0) % 360.0 - 180.0

def _clamp(v: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, v))

def _resolve_body(name: str):
    if not name:
        return eph["SUN"]
    key = name.strip()
    tries = [key, key.capitalize(), key.upper(), key.lower(),
             f"{key} barycenter", f"{key.upper()} BARYCENTER"]
    for k in tries:
        try:
            return eph[k]
        except Exception:
            continue
    return eph["SUN"]

def _estimate_rates_deg_per_s(observador, astro, t_now, dt_s: float):
    """Velocidades aparentes (deg/s) entre t_now e t_now+dt_s."""
    ast_t = observador.at(t_now).observe(astro).apparent()
    alt_t, az_t, _ = ast_t.altaz()
    alt0 = float(alt_t.degrees)
    az0  = float(az_t.degrees)

    t_future = ts.from_datetime(
        datetime.now(timezone.utc) + timedelta(seconds=dt_s)
    )
    ast_tf = observador.at(t_future).observe(astro).apparent()
    alt_tf, az_tf, _ = ast_tf.altaz()
    alt1 = float(alt_tf.degrees)
    az1  = float(az_tf.degrees)

    d_alt = (alt1 - alt0) / max(dt_s, 1e-6)
    d_az  = _shortest_delta_deg(az1, az0) / max(dt_s, 1e-6)

    maxw = SEGUIMENTO_CFG["max_rate_deg_s"]
    return _clamp(d_az, -maxw, maxw), _clamp(d_alt, -maxw, maxw)

# === Seguimento por velocidade contínua com correção e previsão de atraso ===
def _tracking_loop(lat, lon, target_name, _interval_unused: int):
    global _track_state
    try:
        observador = terra + Topos(latitude_degrees=lat, longitude_degrees=lon)
        astro = _resolve_body(target_name)
        _track_state["running"] = True

        tick_s         = float(SEGUIMENTO_CFG["tick_s"])
        rate_refresh_s = float(SEGUIMENTO_CFG["rate_refresh_s"])
        rate_dt_s      = float(SEGUIMENTO_CFG["rate_dt_s"])
        p_corr         = float(SEGUIMENTO_CFG["p_correction"])

        # fallback caso GoTo não tenha setado
        if SEGUIMENTO_CFG["last_cmd_az"] is None or SEGUIMENTO_CFG["last_cmd_alt"] is None:
            t0 = ts.from_datetime(datetime.now(timezone.utc))
            alt0, az0, _ = observador.at(t0).observe(astro).apparent().altaz()
            SEGUIMENTO_CFG["last_cmd_az"]  = _norm360(float(az0.degrees))
            SEGUIMENTO_CFG["last_cmd_alt"] = float(alt0.degrees)

        last_cmd_az  = float(SEGUIMENTO_CFG["last_cmd_az"])
        last_cmd_alt = float(SEGUIMENTO_CFG["last_cmd_alt"])

        # 1ª estimativa de velocidade
        t_now = ts.from_datetime(datetime.now(timezone.utc))
        rate_az, rate_alt = _estimate_rates_deg_per_s(observador, astro, t_now, rate_dt_s)

        last_rate_refresh = 0.0
        dbg_acc = 0.0

        while not _stop_event.is_set():
            if last_rate_refresh >= rate_refresh_s:
                t_now = ts.from_datetime(datetime.now(timezone.utc))
                new_rate_az, new_rate_alt = _estimate_rates_deg_per_s(observador, astro, t_now, rate_dt_s)

                # erro atual (astro observado vs. onde estamos apontando)
                alt_obs, az_obs, _ = observador.at(t_now).observe(astro).apparent().altaz()
                alt_obs = float(alt_obs.degrees)
                az_obs  = float(az_obs.degrees)
                err_az  = _shortest_delta_deg(az_obs,  last_cmd_az)
                err_alt = (alt_obs - last_cmd_alt)

                # leve correção proporcional na própria velocidade
                new_rate_az  = new_rate_az  + p_corr * (err_az  / max(rate_refresh_s, 1e-6))
                new_rate_alt = new_rate_alt + p_corr * (err_alt / max(rate_refresh_s, 1e-6))

                # === AQUI entra o boost de teste ===
                boost = 1.0  # aumenta 5x a velocidade (só pra ver o motor girar)
                new_rate_az  *= boost
                new_rate_alt *= boost

                maxw = SEGUIMENTO_CFG["max_rate_deg_s"]
                rate_az  = _clamp(new_rate_az,  -maxw, maxw)
                rate_alt = _clamp(new_rate_alt, -maxw, maxw)

                last_rate_refresh = 0.0
                _track_state["last_az"]  = az_obs
                _track_state["last_alt"] = alt_obs

            try:
                # Envia velocidades (deg/s) para a ESP32 -> rota /set_speed
                url = f"http://{ESP32_IP}/set_speed"
                requests.get(
                    url,
                    params={"vaz": f"{rate_az:.8f}", "valt": f"{rate_alt:.8f}"},
                    timeout=1.5
                )
                _track_state["last_sent_at"] = datetime.utcnow().isoformat() + "Z"
            except Exception as e:
                _track_state["errors"] += 1
                print(f"[seguir/vel] Falha ao enviar velocidade p/ ESP32: {e}")


            # debug leve 1x/seg
            dbg_acc += tick_s
            if dbg_acc >= 1.0:
                print(f"[seguir/vel] rateAZ={rate_az:.6f}°/s  rateALT={rate_alt:.6f}°/s")
                dbg_acc = 0.0


            if _stop_event.wait(tick_s):
                break
            last_rate_refresh += tick_s

    finally:
        _track_state["running"] = False

# === Compensação de atraso no GoTo (lead time) ===
def _lead_time_seconds(az_now, alt_now, az_target, alt_target):
    """Estima quanto tempo o movimento vai levar, para apontar um pouco à frente."""
    d_az  = abs(_shortest_delta_deg(az_target, az_now))
    d_alt = abs(alt_target - alt_now)
    t_az  = d_az  / max(SEGUIMENTO_CFG["slew_deg_s_az"],  0.1)
    t_alt = d_alt / max(SEGUIMENTO_CFG["slew_deg_s_alt"], 0.1)
    return max(t_az, t_alt) + float(SEGUIMENTO_CFG["extra_net_delay_s"])


def _aguardar_goto(timeout_s=120.0, poll_s=0.2):
    inicio = time.monotonic()

    print("[GOTO] Aguardando telescópio chegar ao alvo...")

    while time.monotonic() - inicio < timeout_s:

        try:
            response = requests.get(
                f"http://{ESP32_IP}/motion_status",
                timeout=1.0
            )

            status = response.json()

            distancia_az = abs(int(status["distanceAz"]))
            distancia_alt = abs(int(status["distanceAlt"]))

            if distancia_az <= 1 and distancia_alt <= 1:
                print("[GOTO] Alvo alcançado.")
                return True

        except Exception as e:
            print(f"[GOTO] Erro lendo posição: {e}")

        time.sleep(poll_s)

    print("[GOTO] Timeout aguardando chegada.")
    return False


def iniciar_seguimento(lat: float, lon: float, astro: str, interval: int = 3,
                       gain: float = 1.0, min_step: float = 0.12, max_step: float = 0.5):
    """GoTo com previsão de atraso + sobe thread de seguimento por velocidade."""
    global _track_thread, _stop_event, _track_state

    if _track_state["running"]:
        parar_seguimento()

    # (legado reset)
    SEGUIMENTO_CFG.update({
        "gain": float(gain),
        "min_step_deg": float(min_step),
        "max_step_deg": float(max_step),
        "accum_az": 0.0,
        "accum_alt": 0.0,
        "last_cmd_az": None,
        "last_cmd_alt": None,
    })

    _stop_event.clear()
    _track_state.update({
        "running": False,
        "target": astro,
        "lat": lat,
        "lon": lon,
        "interval": int(interval),
        "last_az": None,
        "last_alt": None,
        "last_sent_at": None,
        "errors": 0,
    })

    # === GoTo com previsão (compensa atraso de deslocamento + rede) ===
    try:
        t0 = ts.from_datetime(datetime.now(timezone.utc))
        astro_obj = _resolve_body(astro)
        observador = terra + Topos(latitude_degrees=lat, longitude_degrees=lon)

        alt_now, az_now, _ = observador.at(t0).observe(astro_obj).apparent().altaz()
        az_now_deg  = float(az_now.degrees)
        alt_now_deg = float(alt_now.degrees)

        # posição "alvo" no agora
        alt0, az0, _ = observador.at(t0).observe(astro_obj).apparent().altaz()
        az0_deg  = float(az0.degrees)
        alt0_deg = float(alt0.degrees)

        # estima tempo de deslocamento
        t_lead = _lead_time_seconds(az_now_deg, alt_now_deg, az0_deg, alt0_deg)

        # prevê posição do astro em t0 + t_lead (aponta um pouco à frente)
        t_lead_ts = ts.from_datetime(datetime.now(timezone.utc) + timedelta(seconds=t_lead))
        alt_lead, az_lead, _ = observador.at(t_lead_ts).observe(astro_obj).apparent().altaz()
        az_lead_deg  = float(az_lead.degrees)
        alt_lead_deg = float(alt_lead.degrees)

        az_motor_alvo, alt_motor_alvo = _converter_ceu_para_motor(
            az_lead_deg,
            alt_lead_deg
        )

        try:
            url = f"http://{ESP32_IP}/mover"
            requests.get(url, params={
                "az": f"{az_motor_alvo:.3f}", 
                "alt": f"{alt_motor_alvo:.3f}"
                }, timeout=2.5)
        except Exception as e:
            _track_state["errors"] += 1
            print(
                f"[GOTO ALINHADO] "
                f"Céu AZ={az_lead_deg:.3f} ALT={alt_lead_deg:.3f} | "
                f"Motor AZ={az_motor_alvo:.3f} ALT={alt_motor_alvo:.3f}"
            )

        # base de integração = a posição que pedimos agora (com lead)
        SEGUIMENTO_CFG["last_cmd_az"]  = _norm360(az_lead_deg)
        SEGUIMENTO_CFG["last_cmd_alt"] = alt_lead_deg

    except Exception as e:
        return False, f"Falha no GoTo inicial: {e}"

    # Não iniciar tracking enquanto o GoTo ainda estiver acontecendo
    if not _aguardar_goto():
        return False, "Timeout: telescópio não chegou ao alvo."

    # === Thread do seguimento por velocidade ===
    _track_thread = threading.Thread(
        target=_tracking_loop, args=(lat, lon, astro, int(interval)), daemon=True
    )
    _track_thread.start()
    return True, (
        f"Seguimento iniciado (modo velocidade + lead): {astro} | "
        f"tick={SEGUIMENTO_CFG['tick_s']}s, refresh={SEGUIMENTO_CFG['rate_refresh_s']}s, "
        f"rate_dt={SEGUIMENTO_CFG['rate_dt_s']}s, p_corr={SEGUIMENTO_CFG['p_correction']}."
    )


def parar_seguimento():
    global _track_thread, _stop_event, _track_state

    _stop_event.set()

    if _track_thread and _track_thread.is_alive():
        _track_thread.join(timeout=0.8)

    _track_state["running"] = False

    try:
        response = requests.get(
            f"http://{ESP32_IP}/stop",
            timeout=1.5
        )

        print("[STOP] ESP32:", response.text)

    except Exception as e:
        print(f"[STOP] Falha ao parar ESP32: {e}")
        return False, f"Falha ao parar ESP32: {e}"

    return True, "Movimento parado."

# def parar_seguimento():
#     global _track_thread, _stop_event, _track_state
#     _stop_event.set()
#     if _track_thread and _track_thread.is_alive():
#         _track_thread.join(timeout=0.8)
#     _track_state["running"] = False
#     try:
#         requests.get(
#             f"http://{ESP32_IP}/track_off",
#             timeout=1.5
#         )
#         print("[TRACKING] ESP32 saiu do modo tracking.")
#     except Exception as e:
#         print(f"[TRACKING] Falha ao enviar track_off: {e}")


    
#     return True, "Seguimento parado."




def status_seguimento():
    return dict(_track_state)


def ajustar_tracking(direcao):
    """
    Faz um pequeno ajuste de posição sem desligar
    o tracking astronômico.
    """

    if not _track_state["running"]:
        return False, "Tracking ainda não está ativo.", None

    mapa = {
        "direita": (NUDGE_STEP_DEG, 0.0),
        "esquerda": (-NUDGE_STEP_DEG, 0.0),
        "cima": (0.0, NUDGE_STEP_DEG),
        "baixo": (0.0, -NUDGE_STEP_DEG),
    }

    if direcao not in mapa:
        return False, "Direção inválida.", None

    delta_az, delta_alt = mapa[direcao]

    try:
        resposta = requests.get(
            f"http://{ESP32_IP}/nudge",
            params={
                "daz": f"{delta_az:.4f}",
                "dalt": f"{delta_alt:.4f}",
            },
            timeout=2
        )

        try:
            dados = resposta.json()
        except Exception:
            dados = {}

        if not resposta.ok:

            return (
                False,
                dados.get(
                    "erro",
                    "ESP32 recusou o ajuste."
                ),
                dados
            )

        print(
            f"[NUDGE] {direcao} | "
            f"dAZ={delta_az:+.3f}° "
            f"dALT={delta_alt:+.3f}°"
        )

        return (
            True,
            f"Ajuste {direcao} aplicado.",
            dados
        )

    except Exception as erro:

        print(
            f"[NUDGE] Falha de comunicação: {erro}"
        )

        return (
            False,
            f"Falha ao executar ajuste: {erro}",
            None
        )

# GoTo pontual (sem seguimento)

def alinhar_com_astro(nome_astro, latitude, longitude):
    """
    Registra um ponto de alinhamento.

    NÃO altera GoTo nem tracking ainda.
    Apenas compara:

    posição real do astro no céu
    X
    posição lógica atual dos motores
    """

    try:
        # =========================
        # POSIÇÃO ATUAL DOS MOTORES
        # =========================

        resposta = requests.get(
            f"http://{ESP32_IP}/motion_status",
            timeout=2
        )

        resposta.raise_for_status()
        status_motor = resposta.json()

        az_motor = float(status_motor["azDeg"])
        alt_motor = float(status_motor["altDeg"])

    except Exception as erro:
        print(f"[ALINHAMENTO] Erro lendo ESP32: {erro}")

        return {
            "ok": False,
            "erro": f"Não foi possível ler a posição dos motores: {erro}"
        }


    try:
        # =========================
        # POSIÇÃO REAL DO ASTRO
        # =========================

        t = ts.from_datetime(datetime.now(timezone.utc))

        astro = eph[nome_astro.capitalize()]

        observador = terra + Topos(
            latitude_degrees=latitude,
            longitude_degrees=longitude
        )

        alt, az, _ = (
            observador
            .at(t)
            .observe(astro)
            .apparent()
            .altaz()
        )

        az_ceu = float(az.degrees)
        alt_ceu = float(alt.degrees)

    except Exception as erro:
        print(f"[ALINHAMENTO] Erro calculando astro: {erro}")

        return {
            "ok": False,
            "erro": f'Não foi possível calcular o astro "{nome_astro}".'
        }


    # =========================
    # SEGURANÇA
    # =========================

    if not _altitude_segura(alt_ceu):

        return {
            "ok": False,
            "erro": "Não é possível alinhar com um astro abaixo do horizonte.",
            "az": az_ceu,
            "alt": alt_ceu
        }


    # =========================
    # CALCULA OS OFFSETS
    # =========================

    offset_az = _shortest_delta_deg(
        az_ceu,
        az_motor
    )

    offset_alt = (
        alt_ceu
        - alt_motor
    )


    # =========================
    # SALVA O ALINHAMENTO
    # =========================

    ALINHAMENTO.update({
        "ativo": True,
        "astro": nome_astro.capitalize(),

        "az_ceu": az_ceu,
        "alt_ceu": alt_ceu,

        "az_motor": az_motor,
        "alt_motor": alt_motor,

        "offset_az": offset_az,
        "offset_alt": offset_alt,

        "data": datetime.now(timezone.utc).isoformat()
    })


    nome_normalizado = nome_astro.capitalize()

    usar_no_modelo = (
        alt_ceu >= ALT_MIN_MODELO_ALINHAMENTO
        and nome_normalizado.lower() != "sun"
    )


    # =========================
    # SALVA PONTO NO HISTÓRICO
    # =========================

    ponto_alinhamento = {
        "astro": nome_astro.capitalize(),

        "latitude": float(latitude),
        "longitude": float(longitude),

        "az_ceu": az_ceu,
        "alt_ceu": alt_ceu,

        "az_motor": az_motor,
        "alt_motor": alt_motor,

        "offset_az": offset_az,
        "offset_alt": offset_alt,

        "data": ALINHAMENTO["data"],

        "usar_no_modelo": usar_no_modelo
    }

    HISTORICO_ALINHAMENTOS.append(
        ponto_alinhamento
    )

    # Evita crescimento infinito da lista.
    # Mantém os 20 pontos mais recentes.
    if len(HISTORICO_ALINHAMENTOS) > MAX_ALINHAMENTOS:
        HISTORICO_ALINHAMENTOS.pop(0)


    print(
        f"[ALINHAMENTO] {nome_astro} | "
        f"Céu AZ={az_ceu:.3f} ALT={alt_ceu:.3f} | "
        f"Motor AZ={az_motor:.3f} ALT={alt_motor:.3f} | "
        f"Offset AZ={offset_az:.3f} ALT={offset_alt:.3f}"
    )


    return {
        "ok": True,

        "astro": nome_astro.capitalize(),

        "az_ceu": az_ceu,
        "alt_ceu": alt_ceu,

        "az_motor": az_motor,
        "alt_motor": alt_motor,

        "offset_az": offset_az,
        "offset_alt": offset_alt
    }


def status_alinhamento():
    return dict(ALINHAMENTO)


def status_historico_alinhamentos():

    pontos_validos = [
        ponto
        for ponto in HISTORICO_ALINHAMENTOS
        if ponto.get("usar_no_modelo", False)
    ]

    return {
        "quantidade": len(HISTORICO_ALINHAMENTOS),
        "quantidade_validos": len(pontos_validos),

        "pontos": [
            dict(ponto)
            for ponto in HISTORICO_ALINHAMENTOS
        ]
    }


def _distancia_angular_graus(
    az1,
    alt1,
    az2,
    alt2
):
    """
    Distância angular entre dois pontos Alt/Az.
    Resultado em graus.
    """

    az1_rad = math.radians(az1)
    alt1_rad = math.radians(alt1)

    az2_rad = math.radians(az2)
    alt2_rad = math.radians(alt2)

    cos_distancia = (
        math.sin(alt1_rad) *
        math.sin(alt2_rad)
        +
        math.cos(alt1_rad) *
        math.cos(alt2_rad) *
        math.cos(az1_rad - az2_rad)
    )

    cos_distancia = max(
        -1.0,
        min(1.0, cos_distancia)
    )

    return math.degrees(
        math.acos(cos_distancia)
    )


def _calcular_offset_modelo(
    az_ceu,
    alt_ceu
):
    """
    Calcula o offset usando os pontos válidos
    do histórico.

    Pontos mais próximos da região atual do céu
    recebem peso maior.
    """

    pontos_validos = [
        ponto
        for ponto in HISTORICO_ALINHAMENTOS
        if ponto.get(
            "usar_no_modelo",
            False
        )
    ]


    # Nenhum alinhamento válido
    if len(pontos_validos) == 0:

        return 0.0, 0.0, 0


    # Apenas um ponto:
    # mantém comportamento equivalente
    # ao alinhamento simples antigo.
    if len(pontos_validos) == 1:

        ponto = pontos_validos[0]

        return (
            float(ponto["offset_az"]),
            float(ponto["offset_alt"]),
            1
        )


    soma_pesos = 0.0

    soma_offset_az = 0.0
    soma_offset_alt = 0.0


    for ponto in pontos_validos:

        distancia = _distancia_angular_graus(
            az_ceu,
            alt_ceu,
            float(ponto["az_ceu"]),
            float(ponto["alt_ceu"])
        )


        # Se o alvo estiver praticamente
        # no mesmo ponto de um alinhamento,
        # usamos exatamente aquele ponto.
        if distancia < 0.1:

            return (
                float(ponto["offset_az"]),
                float(ponto["offset_alt"]),
                len(pontos_validos)
            )


        # Peso inversamente proporcional
        # ao quadrado da distância.
        #
        # O +1 impede pesos extremos.
        peso = 1.0 / (
            (distancia + 1.0) ** 2
        )


        soma_offset_az += (
            float(ponto["offset_az"])
            * peso
        )

        soma_offset_alt += (
            float(ponto["offset_alt"])
            * peso
        )

        soma_pesos += peso


    if soma_pesos <= 0:

        return 0.0, 0.0, 0


    offset_az = (
        soma_offset_az /
        soma_pesos
    )

    offset_alt = (
        soma_offset_alt /
        soma_pesos
    )


    return (
        offset_az,
        offset_alt,
        len(pontos_validos)
    )


def _converter_ceu_para_motor(
    az_ceu,
    alt_ceu
):
    """
    Converte posição do céu para posição
    mecânica usando o modelo de alinhamento.
    """

    az_ceu = float(az_ceu)
    alt_ceu = float(alt_ceu)


    offset_az, offset_alt, quantidade = (
        _calcular_offset_modelo(
            az_ceu,
            alt_ceu
        )
    )


    az_motor = (
        az_ceu -
        offset_az
    )

    alt_motor = (
        alt_ceu -
        offset_alt
    )


    print(
        f"[MODELO] Pontos={quantidade} | "
        f"Offset AZ={offset_az:+.3f}° "
        f"ALT={offset_alt:+.3f}°"
    )


    return (
        az_motor,
        alt_motor
    )


def mover_para_astro(nome_astro, latitude, longitude):
    try:
        t = ts.from_datetime(datetime.now(timezone.utc))
        astro = eph[nome_astro.capitalize()]
        observador = terra + Topos(latitude_degrees=latitude, longitude_degrees=longitude)
        alt, az, _ = observador.at(t).observe(astro).apparent().altaz()

        az_deg = float(az.degrees)
        alt_deg = float(alt.degrees)

        if not _altitude_segura(alt_deg):
            print(
                f"[SEGURANÇA] Movimento bloqueado. "
                f"AZ={az_deg:.2f}°, ALT={alt_deg:.2f}°"
            )

            return {
                "ok": False,
                "erro": "Astro abaixo do horizonte.",
                "az": az_deg,
                "alt": alt_deg
            }

    except Exception as e:
        print(f"[ERRO] Falha ao calcular o astro: {e}")
        return None

    try:
        url = f"http://{ESP32_IP}/mover"

        az_motor_alvo, alt_motor_alvo = _converter_ceu_para_motor(
            az_deg,
            alt_deg
        )

        requests.get(
            url,
            params={
                "az": f"{az_motor_alvo:.3f}",
                "alt": f"{alt_motor_alvo:.3f}"
            },
            timeout=2.5
        )

        print(
            f"[ASTRO] Céu AZ={az_deg:.3f} ALT={alt_deg:.3f} | "
            f"Motor AZ={az_motor_alvo:.3f} ALT={alt_motor_alvo:.3f} | "
            f"Alinhamento={'ATIVO' if ALINHAMENTO['ativo'] else 'INATIVO'}"
        )

    except Exception as e:
        print(f"[ESP32] Erro ao enviar comando: {e}")

    return {
        "ok": True,
        "az": az_deg,
        "alt": alt_deg,
        "az_motor": az_motor_alvo,
        "alt_motor": alt_motor_alvo,
        "alinhamento_ativo": ALINHAMENTO["ativo"]
    }




# from skyfield.api import load, Topos
# from datetime import datetime, timezone
# from config import ESP32_IP
# import requests

# # Carregamento dos dados astronômicos (carregado só uma vez)
# eph = load('de421.bsp')
# terra = eph['earth']
# ts = load.timescale()

# def mover_para_astro(nome_astro, latitude, longitude):
#     try:
#         t = ts.from_datetime(datetime.now(timezone.utc))
#         # Não usamos get() aqui, já que o astro é acessado diretamente
#         astro = eph[nome_astro.capitalize()]  # Acessando diretamente o astro
#         observador = terra + Topos(latitude_degrees=latitude, longitude_degrees=longitude)
#         astrometria = observador.at(t).observe(astro).apparent()
#         alt, az, _ = astrometria.altaz()
#     except Exception as e:
#         print(f"[ERRO] Falha ao calcular o astro: {e}")
#         return None

#     # Envia as coordenadas para a ESP32
#     try:
#         url = f"http://{ESP32_IP}/mover?az={az.degrees:.2f}&alt={alt.degrees:.2f}"
#         requests.get(url, timeout=2)
#         print(f"[ASTRO] Movendo para AZ={az.degrees:.2f}, ALT={alt.degrees:.2f}")
#     except Exception as e:
#         print(f"[ESP32] Erro ao enviar comando: {e}")

#     return az.degrees, alt.degrees
