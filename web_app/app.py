from flask import Flask, render_template, request, jsonify, Response
from buscar_astro import mover_para_astro
from controle_manual import enviar_comando_manual
from optical_flow import iniciar_rastreamento, parar_rastreamento, set_camera_reference
from config import ESP32_IP
import cv2

# === FLASK SETUP ===
app = Flask(
    __name__,
    static_folder="static",
    template_folder="templates"
)

# === CÂMERA ===
def abrir_primeira_camera():
    for i in range(5):
        cam = cv2.VideoCapture(i)
        if cam.isOpened():
            print(f"[CÂMERA] Usando ID {i}")
            cam.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
            cam.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
            return cam
    raise RuntimeError("[ERRO] Nenhuma câmera disponível.")

camera = abrir_primeira_camera()
set_camera_reference(camera)

# === ROTAS ===
@app.route('/')
def index():
    return render_template('index.html')


@app.route('/buscar', methods=['POST'])
def buscar():
    data = request.get_json()
    nome = data.get('nome', '').strip()
    lat = float(data.get('latitude'))
    lon = float(data.get('longitude'))

    resultado = mover_para_astro(nome, lat, lon)

    if resultado is None:
        return jsonify({'erro': f'Astro "{nome}" não encontrado.'}), 404

    az, alt = resultado
    try:
        iniciar_rastreamento()
    except Exception as e:
        print(f"[OPTICAL FLOW] Falha ao iniciar: {e}")

    return jsonify({
        'astro': nome.capitalize(),
        'az': az,
        'alt': alt
    })


@app.route('/controle')
def pagina_controle():
    return render_template('controle.html')


@app.route('/controle', methods=['POST'])
def controle_post():
    data = request.get_json()
    comando = data.get('comando', '')
    print(f"[COMANDO RECEBIDO] {comando}")
    enviar_comando_manual(comando)
    return jsonify({"status": "comando enviado", "comando": comando})


@app.route('/opticalflow/start', methods=['POST'])
def iniciar_flow():
    iniciar_rastreamento()
    return jsonify({"status": "rastreamento iniciado"})


@app.route('/opticalflow/stop', methods=['POST'])
def parar_flow():
    parar_rastreamento()
    return jsonify({"status": "rastreamento parado"})


def gerar_frames():
    while True:
        success, frame = camera.read()
        if not success:
            break
        _, buffer = cv2.imencode('.jpg', frame)
        frame_bytes = buffer.tobytes()
        yield (b'--frame\r\nContent-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')


@app.route('/video_feed')
def video_feed():
    return Response(gerar_frames(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')


# === EXECUTA SERVIDOR ===
if __name__ == '__main__':
    app.run(debug=True)
