# controle_astro.py
from config import ESP32_IP
import requests

# Funções para enviar comandos para a ESP32
def mover_direita():
    url = f"http://{ESP32_IP}/mover"
    # Enviar comando para mover para direita
    requests.get(url, params={"comando": "direita"})

def mover_esquerda():
    url = f"http://{ESP32_IP}/mover"
    # Enviar comando para mover para esquerda
    requests.get(url, params={"comando": "esquerda"})

def mover_cima():
    url = f"http://{ESP32_IP}/mover"
    # Enviar comando para mover para cima
    requests.get(url, params={"comando": "cima"})

def mover_baixo():
    url = f"http://{ESP32_IP}/mover"
    # Enviar comando para mover para baixo
    requests.get(url, params={"comando": "baixo"})
