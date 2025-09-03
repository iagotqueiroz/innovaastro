from flask import Flask, render_template, request, jsonify
from controle_astro import mover_direita, mover_esquerda, mover_cima, mover_baixo
from buscar_astro import (
    mover_para_astro,
    iniciar_seguimento,
    parar_seguimento,
    status_seguimento,
)

app = Flask(__name__, static_folder="static", template_folder="templates")

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/rastreamento")
def rastreamento():
    return render_template("rastreamento.html")

@app.route("/controle")
def controle():
    return render_template("controle.html")

# GoTo pontual (aponta uma vez)
@app.route("/buscar", methods=["POST"])
def buscar():
    data = request.get_json()
    nome = data.get("nome", "").strip()
    lat = float(data.get("latitude"))
    lon = float(data.get("longitude"))
    resultado = mover_para_astro(nome, lat, lon)
    if resultado is None:
        return jsonify({"erro": f'Astro "{nome}" não encontrado.'}), 404
    az, alt = resultado
    return jsonify({"astro": nome.capitalize(), "az": az, "alt": alt})
 
# Seguir continuamente (GoTo + velocidade contínua com compensação)
@app.route("/seguir", methods=["POST", "GET"])
def seguir():
    data = request.args if request.method == "GET" else (request.get_json(silent=True) or {})
    try:
        lat = float(data.get("latitude"))
        lon = float(data.get("longitude"))
        astro = str(data.get("nome", "Saturn")).strip()
        # estes ainda são aceitos, mas o modo por velocidade não depende deles
        interval  = int(data.get("interval", 1))
        gain      = float(data.get("gain", 1.0))
        min_step  = float(data.get("min_step", 0.12))
        max_step  = float(data.get("max_step", 0.5))
    except Exception:
        return jsonify({"ok": False, "erro": "Parâmetros inválidos (latitude, longitude, nome, ...)"}), 400

    ok, msg = iniciar_seguimento(lat, lon, astro, interval, gain, min_step, max_step)
    return jsonify({"ok": ok, "msg": msg, "status": status_seguimento()}), (200 if ok else 500)

@app.route("/parar", methods=["POST", "GET"])
def parar():
    ok, msg = parar_seguimento()
    return jsonify({"ok": ok, "msg": msg, "status": status_seguimento()}), (200 if ok else 500)

@app.route("/status", methods=["GET"])
def status():
    return jsonify(status_seguimento())

@app.route("/mover", methods=["GET"])
def mover():
    comando = request.args.get('comando')
    
    if comando == "direita":
        mover_direita()
    elif comando == "esquerda":
        mover_esquerda()
    elif comando == "cima":
        mover_cima()
    elif comando == "baixo":
        mover_baixo()
    
    return jsonify({"status": "movimento realizado", "comando": comando})

if __name__ == "__main__":
    app.run(debug=True)




#Versão boa 02
# from flask import Flask, render_template, request, jsonify, Response
# from buscar_astro import mover_para_astro
# from controle_manual import enviar_comando_manual
# from config import ESP32_IP
# import cv2

# # === FLASK SETUP ===
# app = Flask(
#     __name__,
#     static_folder="static",
#     template_folder="templates"
# )

# # === CÂMERA ===
# def abrir_primeira_camera():
#     for i in range(5):
#         cam = cv2.VideoCapture(i)
#         if cam.isOpened():
#             print(f"[CÂMERA] Usando ID {i}")
#             cam.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
#             cam.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
#             return cam
#     raise RuntimeError("[ERRO] Nenhuma câmera disponível.")

# camera = None  # Não inicializar automaticamente a câmera

# # === ROTAS ===
# @app.route('/')
# def index():
#     return render_template('index.html')

# @app.route('/rastreamento')
# def rastreamento():
#     return render_template('rastreamento.html')

# @app.route('/buscar', methods=['POST'])
# def buscar():
#     data = request.get_json()
#     nome = data.get('nome', '').strip()
#     lat = float(data.get('latitude'))
#     lon = float(data.get('longitude'))

#     # A lógica de mover para o astro agora está exclusivamente no buscar_astro.py
#     resultado = mover_para_astro(nome, lat, lon)

#     if resultado is None:
#         return jsonify({'erro': f'Astro "{nome}" não encontrado.'}), 404

#     az, alt = resultado

#     return jsonify({
#         'astro': nome.capitalize(),
#         'az': az,
#         'alt': alt
#     })

# @app.route('/iniciar_rastreamento', methods=['POST'])
# def iniciar_rastreamento():
#     global camera
#     # Inicializa a câmera somente quando o botão for clicado
#     if camera is None:
#         camera = abrir_primeira_camera()

#     return jsonify({"status": "rastreamento iniciado"})

# @app.route('/controle')
# def pagina_controle():
#     return render_template('controle.html')

# @app.route('/controle', methods=['POST'])
# def controle_post():
#     data = request.get_json()
#     comando = data.get('comando', '')
#     print(f"[COMANDO RECEBIDO] {comando}")

#     try:
#         response = enviar_comando_manual(comando)
#         print(f"[ESP32] Resposta da ESP32: {response.text}")  # Log de resposta da ESP32
#         return jsonify({"status": "comando enviado", "comando": comando})
#     except Exception as e:
#         print(f"[ERRO] Falha ao enviar comando: {e}")
#         return jsonify({"status": "erro", "mensagem": str(e)}), 500

# # === EXECUTA SERVIDOR ===
# if __name__ == '__main__':
#     app.run(debug=True)


#----------------------------------------------------


# from flask import Flask, render_template, request, jsonify, Response
# from buscar_astro import mover_para_astro
# from controle_manual import enviar_comando_manual
# from config import ESP32_IP
# import cv2

# # === FLASK SETUP ===
# app = Flask(
#     __name__,
#     static_folder="static",
#     template_folder="templates"
# )

# # === CÂMERA ===
# def abrir_primeira_camera():
#     for i in range(5):
#         cam = cv2.VideoCapture(i)
#         if cam.isOpened():
#             print(f"[CÂMERA] Usando ID {i}")
#             cam.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
#             cam.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
#             return cam
#     raise RuntimeError("[ERRO] Nenhuma câmera disponível.")

# camera = None  # Não inicializar automaticamente a câmera

# # === ROTAS ===
# @app.route('/')
# def index():
#     return render_template('index.html')

# @app.route('/rastreamento')
# def rastreamento():
#     return render_template('rastreamento.html')

# @app.route('/buscar', methods=['POST'])
# def buscar():
#     data = request.get_json()
#     nome = data.get('nome', '').strip()
#     lat = float(data.get('latitude'))
#     lon = float(data.get('longitude'))

#     resultado = mover_para_astro(nome, lat, lon)

#     if resultado is None:
#         return jsonify({'erro': f'Astro "{nome}" não encontrado.'}), 404

#     az, alt = resultado

#     return jsonify({
#         'astro': nome.capitalize(),
#         'az': az,
#         'alt': alt
#     })

# @app.route('/iniciar_rastreamento', methods=['POST'])
# def iniciar_rastreamento():
#     global camera
#     # Inicializa a câmera somente quando o botão for clicado
#     if camera is None:
#         camera = abrir_primeira_camera()

#     return jsonify({"status": "rastreamento iniciado"})

# @app.route('/controle')
# def pagina_controle():
#     return render_template('controle.html')

# @app.route('/controle', methods=['POST'])
# def controle_post():
#     data = request.get_json()
#     comando = data.get('comando', '')
#     print(f"[COMANDO RECEBIDO] {comando}")

#     try:
#         response = enviar_comando_manual(comando)
#         print(f"[ESP32] Resposta da ESP32: {response.text}")  # Log de resposta da ESP32
#         return jsonify({"status": "comando enviado", "comando": comando})
#     except Exception as e:
#         print(f"[ERRO] Falha ao enviar comando: {e}")
#         return jsonify({"status": "erro", "mensagem": str(e)}), 500

# # === EXECUTA SERVIDOR ===
# if __name__ == '__main__':
#     app.run(debug=True)
