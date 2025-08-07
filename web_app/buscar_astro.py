from skyfield.api import load, Topos
from datetime import datetime, timezone
from config import ESP32_IP
import requests

# Carregamento dos dados astronômicos (carregado só uma vez)
eph = load('de421.bsp')
terra = eph['earth']
ts = load.timescale()

def mover_para_astro(nome_astro, latitude, longitude):
    try:
        t = ts.from_datetime(datetime.now(timezone.utc))
        astro = eph[nome_astro.capitalize()]
        observador = terra + Topos(latitude_degrees=latitude, longitude_degrees=longitude)
        astrometria = observador.at(t).observe(astro).apparent()
        alt, az, _ = astrometria.altaz()
    except:
        print(f"[ERRO] Astro '{nome_astro}' não encontrado.")
        return None

    try:
        url = f"http://{ESP32_IP}/mover?az={az.degrees:.2f}&alt={alt.degrees:.2f}"
        requests.get(url, timeout=2)
        print(f"[ASTRO] Movendo para AZ={az.degrees:.2f}, ALT={alt.degrees:.2f}")
    except Exception as e:
        print(f"[ESP32] Erro ao enviar comando: {e}")

    return az.degrees, alt.degrees
