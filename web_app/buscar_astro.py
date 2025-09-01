from skyfield.api import load, Topos
from datetime import datetime, timezone
from config import ESP32_IP
import requests
# --- ADICIONE ---
import threading
import time


# Carregamento dos dados astronômicos (carregado só uma vez)
eph = load('de421.bsp')
terra = eph['earth']
ts = load.timescale()


# === SEGUIMENTO: ganho e acumuladores p/ movimento fino ===
# === SEGUIMENTO: ganho e acumuladores p/ movimento fino ===
SEGUIMENTO_CFG = {
    "gain": 1.0,           # multiplicador
    "min_step_deg": 0.12,  # deg mínimos por iteração
    "max_step_deg": 0.5,   # deg máximos por iteração
    "accum_az": 0.0,       # resíduos acumulados (az)
    "accum_alt": 0.0,      # resíduos acumulados (alt)
    "last_cmd_az": None,   # último AZ enviado (graus)
    "last_cmd_alt": None,  # último ALT enviado (graus)
    "hysteresis_deg": 0.02,# janela morta anti-oscilação
    "send_precision": 3,   # enviar com 3 casas decimais
}


def _norm360(x: float) -> float:
    return (x % 360.0 + 360.0) % 360.0

def _shortest_delta_deg(target: float, current: float) -> float:
    # menor diferença angular com wrap 0/360
    return (target - current + 540.0) % 360.0 - 180.0

def _clamp(v: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, v))

def _decide_step(delta_raw: float, axis: str) -> float:
    """
    Recebe delta bruto (graus) astro - último_comando.
    Soma resíduos, aplica ganho e libera passo se >= min_step_deg.
    O que não for usado fica acumulado para a próxima iteração.
    """
    total_raw = delta_raw + SEGUIMENTO_CFG[f"accum_{axis}"]
    scaled = total_raw * SEGUIMENTO_CFG["gain"]

    if abs(scaled) >= SEGUIMENTO_CFG["min_step_deg"]:
        # limita por iteração para evitar tranco
        if scaled > SEGUIMENTO_CFG["max_step_deg"]:
            step = SEGUIMENTO_CFG["max_step_deg"]
        elif scaled < -SEGUIMENTO_CFG["max_step_deg"]:
            step = -SEGUIMENTO_CFG["max_step_deg"]
        else:
            step = scaled
        # remove dos resíduos o que foi efetivamente usado (converter de volta para "raw")
        SEGUIMENTO_CFG[f"accum_{axis}"] = total_raw - (step / SEGUIMENTO_CFG["gain"])
        return step
    else:
        SEGUIMENTO_CFG[f"accum_{axis}"] = total_raw
        return 0.0

def _avoid_overshoot(step: float, delta_raw: float) -> float:
    """
    Se o passo proposto cruzar o alvo (|step| > |delta_raw|), limita para não cruzar,
    deixando uma folguinha (histerese). Se a folga ficar negativa, não move.
    """
    if step == 0.0:
        return 0.0
    if abs(step) > abs(delta_raw):
        margin = SEGUIMENTO_CFG.get("hysteresis_deg", 0.02)
        capped = abs(delta_raw) - margin
        if capped <= 0:
            return 0.0
        # mantém o sinal original do passo
        return (step / abs(step)) * min(abs(step), capped)
    return step


    """
    Recebe delta bruto (graus) astro - último_comando.
    Soma resíduos, aplica ganho e libera passo se >= min_step_deg.
    O que não for usado fica acumulado para a próxima iteração.
    """
    total_raw = delta_raw + SEGUIMENTO_CFG[f"accum_{axis}"]
    scaled = total_raw * SEGUIMENTO_CFG["gain"]

    if abs(scaled) >= SEGUIMENTO_CFG["min_step_deg"]:
        step = _clamp(
            scaled,
            -SEGUIMENTO_CFG["max_step_deg"],
            SEGUIMENTO_CFG["max_step_deg"]
        )
        # remove dos resíduos o que foi efetivamente usado (converter de volta para "raw")
        SEGUIMENTO_CFG[f"accum_{axis}"] = total_raw - (step / SEGUIMENTO_CFG["gain"])
        return step
    else:
        SEGUIMENTO_CFG[f"accum_{axis}"] = total_raw
        return 0.0


# --- ADICIONE ---
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

# --- ADICIONE ---
def _resolve_body(name: str):
    """
    Tenta mapear o nome enviado (ex.: 'Saturn', 'saturno') para uma chave válida do eph.
    """
    if not name:
        return eph["SUN"]
    key = name.strip()
    tries = [
        key,
        key.capitalize(),
        key.upper(),
        key.lower(),
        f"{key} barycenter",
        f"{key.upper()} BARYCENTER",
    ]
    for k in tries:
        try:
            return eph[k]
        except Exception:
            continue
    return eph["SUN"]  # fallback seguro


# --- ADICIONE ---
def _tracking_loop(lat, lon, target_name, interval_s: int):
    global _track_state
    try:
        observador = terra + Topos(latitude_degrees=lat, longitude_degrees=lon)
        astro = _resolve_body(target_name)
        _track_state["running"] = True

        while not _stop_event.is_set():
            t = ts.from_datetime(datetime.now(timezone.utc))

            # ALT/AZ do astro (posição real no céu)
            astrometria = observador.at(t).observe(astro).apparent()
            alt, az, _ = astrometria.altaz()
            alt_now = float(alt.degrees)
            az_now  = float(az.degrees)

            _track_state["last_alt"] = alt_now
            _track_state["last_az"]  = az_now

            # Garantia de inicialização
            if SEGUIMENTO_CFG["last_cmd_az"] is None:
                SEGUIMENTO_CFG["last_cmd_az"] = _norm360(az_now)
            if SEGUIMENTO_CFG["last_cmd_alt"] is None:
                SEGUIMENTO_CFG["last_cmd_alt"] = alt_now

            # deltas brutos (astro - último enviado)
            delta_az_raw  = _shortest_delta_deg(az_now,  SEGUIMENTO_CFG["last_cmd_az"])
            delta_alt_raw = (alt_now - SEGUIMENTO_CFG["last_cmd_alt"])

            # decide o passo com ganho + resíduos
            step_az  = _decide_step(delta_az_raw,  "az")
            step_alt = _decide_step(delta_alt_raw, "alt")

            # evita cruzar o alvo (anti-oscilação)
            step_az  = _avoid_overshoot(step_az,  delta_az_raw)
            step_alt = _avoid_overshoot(step_alt, delta_alt_raw)

            if step_az != 0.0 or step_alt != 0.0:
                new_az  = _norm360(SEGUIMENTO_CFG["last_cmd_az"]  + step_az)
                new_alt = _clamp(SEGUIMENTO_CFG["last_cmd_alt"] + step_alt, 0.0, 90.0)

                try:
                    url = f"http://{ESP32_IP}/mover"
                    prec = SEGUIMENTO_CFG.get("send_precision", 3)
                    requests.get(
                        url,
                        params={"az": f"{new_az:.{prec}f}", "alt": f"{new_alt:.{prec}f}"},
                        timeout=1.5
                    )
                    SEGUIMENTO_CFG["last_cmd_az"]  = new_az
                    SEGUIMENTO_CFG["last_cmd_alt"] = new_alt
                    _track_state["last_sent_at"] = datetime.utcnow().isoformat() + "Z"
                except Exception as e:
                    _track_state["errors"] += 1
                    print(f"[seguir] Falha ao enviar p/ ESP32: {e}")

            # Espera com possibilidade de cancelamento
            _stop_event.wait(max(1, int(interval_s)))
    finally:
        _track_state["running"] = False




def iniciar_seguimento(lat: float, lon: float, astro: str, interval: int = 3,
                       gain: float = 1.0, min_step: float = 0.12, max_step: float = 0.5):
    """
    Inicia (ou reinicia) a thread de seguimento.
    O ganho/limiares valem apenas para o seguimento (NÃO alteram o GoTo inicial).
    """
    global _track_thread, _stop_event, _track_state

    # Se já estiver rodando, pare primeiro
    if _track_state["running"]:
        parar_seguimento()

    # Config do seguimento
    SEGUIMENTO_CFG.update({
        "gain": float(gain),
        "min_step_deg": float(min_step),
        "max_step_deg": float(max_step),
        "accum_az": 0.0,
        "accum_alt": 0.0,
        "last_cmd_az": None,
        "last_cmd_alt": None,
    })

    # Estado
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

    # === GoTo inicial (SEM ganho): posiciona certinho antes de seguir ===
    try:
        t0 = ts.from_datetime(datetime.now(timezone.utc))
        astro_obj = _resolve_body(astro)
        observador = terra + Topos(latitude_degrees=lat, longitude_degrees=lon)
        alt0, az0, _ = observador.at(t0).observe(astro_obj).apparent().altaz()
        az0_deg, alt0_deg = float(az0.degrees), float(alt0.degrees)

        try:
            url = f"http://{ESP32_IP}/mover"
            requests.get(url, params={"az": f"{az0_deg:.2f}", "alt": f"{alt0_deg:.2f}"}, timeout=2)
        except Exception as e:
            _track_state["errors"] += 1
            print(f"[GoTo inicial] Falha ao enviar p/ ESP32: {e}")

        # salva ponto absoluto recebido pela ESP como "last_cmd_*"
        SEGUIMENTO_CFG["last_cmd_az"] = _norm360(az0_deg)
        SEGUIMENTO_CFG["last_cmd_alt"] = alt0_deg

    except Exception as e:
        return False, f"Falha no GoTo inicial: {e}"

    # === Sobe a thread do seguimento (com ganho) ===
    _track_thread = threading.Thread(
        target=_tracking_loop,
        args=(lat, lon, astro, int(interval)),
        daemon=True
    )
    _track_thread.start()
    return True, f"Seguimento iniciado: {astro} a cada {interval}s (gain={gain}, min={min_step}°, max={max_step}°)."


def parar_seguimento():
    """
    Solicita parada e aguarda a thread encerrar.
    """
    global _track_thread, _stop_event, _track_state
    _stop_event.set()
    if _track_thread and _track_thread.is_alive():
        _track_thread.join(timeout=0.5)
    _track_state["running"] = False
    return True, "Seguimento parado."


def status_seguimento():
    """
    Retorna snapshot do estado do seguimento.
    """
    return dict(_track_state)



def mover_para_astro(nome_astro, latitude, longitude):
    try:
        t = ts.from_datetime(datetime.now(timezone.utc))
        # Não usamos get() aqui, já que o astro é acessado diretamente
        astro = eph[nome_astro.capitalize()]  # Acessando diretamente o astro
        observador = terra + Topos(latitude_degrees=latitude, longitude_degrees=longitude)
        astrometria = observador.at(t).observe(astro).apparent()
        alt, az, _ = astrometria.altaz()
    except Exception as e:
        print(f"[ERRO] Falha ao calcular o astro: {e}")
        return None

    # Envia as coordenadas para a ESP32
    try:
        url = f"http://{ESP32_IP}/mover?az={az.degrees:.2f}&alt={alt.degrees:.2f}"
        requests.get(url, timeout=2)
        print(f"[ASTRO] Movendo para AZ={az.degrees:.2f}, ALT={alt.degrees:.2f}")
    except Exception as e:
        print(f"[ESP32] Erro ao enviar comando: {e}")

    return az.degrees, alt.degrees



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
