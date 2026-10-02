# controle_astro.py
from config import ESP32_IP
import requests
import time

# Funções para enviar comandos para a ESP32
def mover_direita():
    url = f"http://{ESP32_IP}/controle"
    # Enviar comando para mover para direita
    requests.get(url, params={"comando": "direita"})

def mover_esquerda():
    url = f"http://{ESP32_IP}/controle"
    # Enviar comando para mover para esquerda
    requests.get(url, params={"comando": "esquerda"})

def mover_cima():
    url = f"http://{ESP32_IP}/controle"
    # Enviar comando para mover para cima
    requests.get(url, params={"comando": "cima"})

def mover_baixo():
    url = f"http://{ESP32_IP}/controle"
    # Enviar comando para mover para baixo
    requests.get(url, params={"comando": "baixo"})


def executar_home():
    timeout_eixo = 125

    def ler_status_home():
        resposta = requests.get(
            f"http://{ESP32_IP}/home_status",
            timeout=2
        )

        resposta.raise_for_status()
        return resposta.json()


    def aguardar_home(eixo):
        inicio = time.monotonic()

        if eixo == "AZ":
            chave_homing = "homingAz"
            chave_done = "doneAz"
        else:
            chave_homing = "homingAlt"
            chave_done = "doneAlt"

        while time.monotonic() - inicio < timeout_eixo:

            try:
                estado = ler_status_home()

                # Terminou corretamente
                if estado.get(chave_done):
                    return "ok"

                # Estava em Home e foi cancelado pelo manual/STOP
                if not estado.get(chave_homing) and not estado.get(chave_done):
                    return "cancelado"

            except requests.RequestException as erro:
                print(
                    f"[HOME] Falha temporária lendo status {eixo}: {erro}"
                )

            time.sleep(0.2)

        return "timeout"


    # =========================
    # AZ
    # =========================

    try:
        resposta = requests.get(
            f"http://{ESP32_IP}/home_az",
            timeout=3
        )

        resposta.raise_for_status()

    except requests.RequestException as erro:
        return False, f"Falha ao iniciar HOME AZ: {erro}"

    resultado = aguardar_home("AZ")

    if resultado == "cancelado":
        return False, "HOME cancelado pelo controle manual."

    if resultado == "timeout":
        try:
            requests.get(
                f"http://{ESP32_IP}/stop",
                timeout=2
            )
        except:
            pass

        return False, "Timeout no HOME do AZ."


    # =========================
    # ALT
    # =========================

    try:
        resposta = requests.get(
            f"http://{ESP32_IP}/home_alt",
            timeout=3
        )

        resposta.raise_for_status()

    except requests.RequestException as erro:
        return False, f"Falha ao iniciar HOME ALT: {erro}"

    resultado = aguardar_home("ALT")

    if resultado == "cancelado":
        return False, "HOME cancelado pelo controle manual."

    if resultado == "timeout":
        try:
            requests.get(
                f"http://{ESP32_IP}/stop",
                timeout=2
            )
        except:
            pass

        return False, "Timeout no HOME do ALT."

    return True, "Telescópio inicializado. AZ = 0 / ALT = 0."

# def executar_home():
#     """
#     Faz o HOME completo do telescópio:
#     1. AZ até o switch
#     2. ALT até o switch
#     """

#     timeout_eixo = 17  # ESP tem timeout interno de 15 s

#     # =========================
#     # HOME AZ
#     # =========================
#     resposta = requests.get(
#         f"http://{ESP32_IP}/home_az",
#         timeout=3
#     )
#     resposta.raise_for_status()

#     inicio = time.monotonic()

#     while time.monotonic() - inicio < timeout_eixo:

#         estado = requests.get(
#             f"http://{ESP32_IP}/limit_az",
#             timeout=3
#         )
#         estado.raise_for_status()

#         if estado.json().get("acionado"):
#             # dá tempo para a ESP finalizar AZ = 0
#             time.sleep(0.2)
#             break

#         time.sleep(0.2)

#     else:
#         requests.get(
#             f"http://{ESP32_IP}/stop",
#             timeout=3
#         )

#         return False, "Timeout no HOME do AZ."


#     # =========================
#     # HOME ALT
#     # =========================
#     resposta = requests.get(
#         f"http://{ESP32_IP}/home_alt",
#         timeout=3
#     )
#     resposta.raise_for_status()

#     inicio = time.monotonic()

#     while time.monotonic() - inicio < timeout_eixo:

#         estado = requests.get(
#             f"http://{ESP32_IP}/limit_alt",
#             timeout=3
#         )
#         estado.raise_for_status()

#         if estado.json().get("acionado"):
#             # dá tempo para a ESP finalizar ALT = 0
#             time.sleep(0.2)
#             break

#         time.sleep(0.2)

#     else:
#         requests.get(
#             f"http://{ESP32_IP}/stop",
#             timeout=3
#         )

#         return False, "Timeout no HOME do ALT."


#     return True, "Telescópio inicializado. AZ = 0 / ALT = 0."