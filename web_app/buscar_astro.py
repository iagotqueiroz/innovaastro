from skyfield.api import load, Topos
from datetime import datetime, timezone, timedelta
from config import ESP32_IP
import requests
import threading

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
    "p_correction": 0.35,  # correção proporcional lenta (anti-drift)
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
                boost = 5.0  # aumenta 5x a velocidade (só pra ver o motor girar)
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

        try:
            url = f"http://{ESP32_IP}/mover"
            requests.get(url, params={"az": f"{az_lead_deg:.3f}", "alt": f"{alt_lead_deg:.3f}"}, timeout=2.5)
        except Exception as e:
            _track_state["errors"] += 1
            print(f"[GoTo inicial] Falha ao enviar p/ ESP32: {e}")

        # base de integração = a posição que pedimos agora (com lead)
        SEGUIMENTO_CFG["last_cmd_az"]  = _norm360(az_lead_deg)
        SEGUIMENTO_CFG["last_cmd_alt"] = alt_lead_deg

    except Exception as e:
        return False, f"Falha no GoTo inicial: {e}"

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
    return True, "Seguimento parado."

def status_seguimento():
    return dict(_track_state)

# GoTo pontual (sem seguimento)
def mover_para_astro(nome_astro, latitude, longitude):
    try:
        t = ts.from_datetime(datetime.now(timezone.utc))
        astro = eph[nome_astro.capitalize()]
        observador = terra + Topos(latitude_degrees=latitude, longitude_degrees=longitude)
        alt, az, _ = observador.at(t).observe(astro).apparent().altaz()
    except Exception as e:
        print(f"[ERRO] Falha ao calcular o astro: {e}")
        return None

    try:
        url = f"http://{ESP32_IP}/mover?az={az.degrees:.3f}&alt={alt.degrees:.3f}"
        requests.get(url, timeout=2.5)
        print(f"[ASTRO] GoTo AZ={az.degrees:.3f}, ALT={alt.degrees:.3f}")
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
