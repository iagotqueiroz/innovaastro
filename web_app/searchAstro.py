from optical_flow import iniciar_rastreamento, parar_rastreamento, set_camera_reference
from flask import Flask, render_template, request, jsonify, Response, url_for
from skyfield.api import Topos, load
from datetime import datetime, timezone
from flask import Flask, render_template, request, jsonify
import cv2
import requests
import os


# === CONFIGURAÇÃO DO FLASK ===
app = Flask(
    __name__,
    static_folder="static",     # pasta estática dentro de Web-App
    template_folder="templates" # pasta de templates dentro de Web-App
)

# === FUNÇÃO PARA ABRIR A PRIMEIRA CÂMERA DISPONÍVEL ===
def abrir_primeira_camera():
    """Procura e abre a primeira câmera física disponível"""
    for i in range(5):  # testa IDs de 0 a 4
        cam = cv2.VideoCapture(i)
        if cam.isOpened():
            print(f"[CÂMERA Flask] Usando dispositivo ID {i}")
            cam.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
            cam.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
            return cam
    raise RuntimeError("[ERRO] Nenhuma câmera disponível.")

# === INICIALIZA A CÂMERA E ARMAZENA A REFERÊNCIA GLOBAL ===
camera = abrir_primeira_camera()
set_camera_reference(camera)

# === CARREGAMENTO DOS DADOS ASTRONÔMICOS DO SKYFIELD ===
eph = load('de421.bsp')
terra = eph['earth']
ts = load.timescale()

# === ROTA PRINCIPAL DA PÁGINA ===
@app.route('/')
def index():
    return render_template('index.html')

# === ROTA PARA BUSCAR O ASTRO PELO NOME ===
@app.route('/buscar', methods=['POST'])
def buscar_astro():
    data = request.get_json()
    nome_astro = data.get('nome', '').strip()
    latitude = float(data.get('latitude'))
    longitude = float(data.get('longitude'))

    try:
        t = ts.from_datetime(datetime.now(timezone.utc))
        astro = eph[nome_astro.capitalize()]
        observador = terra + Topos(latitude_degrees=latitude, longitude_degrees=longitude)
        astrometria = observador.at(t).observe(astro).apparent()
        alt, az, _ = astrometria.altaz()
    except:
        return jsonify({'erro': f'Astro \"{nome_astro}\" não encontrado.'}), 404

    # Envia os graus para a ESP32
    try:
        url = f"http://192.168.15.2/mover?az={az.degrees:.2f}&alt={alt.degrees:.2f}"
        requests.get(url, timeout=2)
    except Exception as e:
        print(f"[ESP32] Falha: {e}")

    # Inicia o rastreamento do ponto mais brilhante após mover
    try:
        iniciar_rastreamento()
    except Exception as e:
        print(f"[OPTICAL FLOW] Erro ao iniciar rastreamento: {e}")

    return jsonify({
        'astro': nome_astro.capitalize(),
        'az': az.degrees,
        'alt': alt.degrees
    })

# === ROTA OPCIONAL PARA INICIAR RASTREAMENTO PELO FRONT ===
@app.route('/opticalflow/start', methods=['POST'])
def start_opticalflow():
    iniciar_rastreamento()
    return jsonify({"status": "Optical Flow iniciado"})

# === ROTA PARA PARAR O RASTREAMENTO ===
@app.route('/opticalflow/stop', methods=['POST'])
def stop_opticalflow():
    parar_rastreamento()
    return jsonify({"status": "Optical Flow parado"})

# === ROTA DE STREAMING DE VÍDEO PARA A INTERFACE ===
def gerar_frames():
    while True:
        success, frame = camera.read()
        if not success:
            break
        else:
            _, buffer = cv2.imencode('.jpg', frame)
            frame_bytes = buffer.tobytes()
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')

@app.route('/video_feed')
def video_feed():
    return Response(gerar_frames(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')


# ROTA CONTROLES

@app.route('/controle')
def pagina_controle():
    return render_template('controle.html')

@app.route('/controle', methods=['POST'])
def receber_comando():
    data = request.get_json()
    comando = data.get('comando', '')

    print(f"[COMANDO RECEBIDO] {comando}")

    # Em breve: Aqui enviaremos esse comando para a ESP32 via HTTP

    return jsonify({"status": "comando recebido", "comando": comando})


# === EXECUÇÃO DO SERVIDOR FLASK ===
if __name__ == '__main__':
    app.run(debug=True)




# from optical_flow import iniciar_rastreamento, parar_rastreamento, set_camera_reference
# from flask import Flask, render_template, request, jsonify, Response
# from skyfield.api import Topos, load
# from datetime import datetime, timezone
# import cv2
# import requests

# app = Flask(__name__)

# from flask import Flask, render_template, request, jsonify, Response, url_for
# import os

# app = Flask(
#     __name__,
#     static_folder="static",     # pasta estática dentro de Web-App
#     template_folder="templates" # pasta de templates dentro de Web-App
# )



# # ======= FUNÇÃO PARA ABRIR PRIMEIRA CÂMERA DISPONÍVEL =======
# def abrir_primeira_camera():
#     """Procura e abre a primeira câmera física disponível"""
#     for i in range(5):  # testa IDs de 0 a 4
#         cam = cv2.VideoCapture(i)
#         if cam.isOpened():
#             print(f"[CÂMERA Flask] Usando dispositivo ID {i}")
#             cam.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
#             cam.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
#             return cam
#     raise RuntimeError("[ERRO] Nenhuma câmera disponível.")

# # ======= INICIALIZA CÂMERA UMA VEZ E REUTILIZA =======
# camera = abrir_primeira_camera()
# set_camera_reference(camera)

# # ======= SKYFIELD =======
# eph = load('de421.bsp')
# terra = eph['earth']
# ts = load.timescale()

# @app.route('/')
# def index():
#     return render_template('index.html')

# @app.route('/buscar', methods=['POST'])
# def buscar_astro():
#     data = request.get_json()
#     nome_astro = data.get('nome', '').strip()
#     latitude = float(data.get('latitude'))
#     longitude = float(data.get('longitude'))

#     try:
#         t = ts.from_datetime(datetime.now(timezone.utc))
#         astro = eph[nome_astro.capitalize()]
#         observador = terra + Topos(latitude_degrees=latitude, longitude_degrees=longitude)
#         astrometria = observador.at(t).observe(astro).apparent()
#         alt, az, _ = astrometria.altaz()
#     except:
#         return jsonify({'erro': f'Astro \"{nome_astro}\" não encontrado.'}), 404

#     try:
#         url = f"http://192.168.15.13/mover?az={az.degrees:.2f}&alt={alt.degrees:.2f}"
#         requests.get(url, timeout=2)
#     except Exception as e:
#         print(f"[ESP32] Falha: {e}")

#     # ✅ ATIVAR O RASTREAMENTO ÓPTICO APÓS MOVER
#     try:
#         iniciar_rastreamento(az.degrees, alt.degrees)
#     except Exception as e:
#         print(f"[OPTICAL FLOW] Erro ao iniciar rastreamento: {e}")

#     return jsonify({
#         'astro': nome_astro.capitalize(),
#         'az': az.degrees,
#         'alt': alt.degrees
#     })


# @app.route('/opticalflow/start', methods=['POST'])
# def start_opticalflow():
#     data = request.get_json() or {}
#     az = float(data.get('az', 0.0))
#     alt = float(data.get('alt', 0.0))
#     iniciar_rastreamento(az, alt)
#     return jsonify({"status": "Optical Flow iniciado", "az": az, "alt": alt})

# @app.route('/opticalflow/stop', methods=['POST'])
# def stop_opticalflow():
#     parar_rastreamento()
#     return jsonify({"status": "Optical Flow parado"})

# # ======= STREAM DA CÂMERA =======
# def gerar_frames():
#     while True:
#         success, frame = camera.read()
#         if not success:
#             break
#         else:
#             _, buffer = cv2.imencode('.jpg', frame)
#             frame_bytes = buffer.tobytes()
#             yield (b'--frame\r\n'
#                    b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')

# @app.route('/video_feed')
# def video_feed():
#     return Response(gerar_frames(),
#                     mimetype='multipart/x-mixed-replace; boundary=frame')

# if __name__ == '__main__':
#     app.run(debug=True)