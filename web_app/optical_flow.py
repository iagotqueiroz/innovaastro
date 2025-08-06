import cv2
import numpy as np
import threading
import requests

# ===== CONFIGURAÇÕES =====
IP_ESP32 = "192.168.15.14"  # IP da sua ESP32
FOV_HORIZONTAL = 60.0       # Campo de visão horizontal da câmera (graus)
FOV_VERTICAL = 45.0         # Campo de visão vertical da câmera (graus)
BRILHO_MINIMO = 200         # Intensidade mínima (0-255) para considerar um ponto
PIXEL_MOV_MIN = 2           # Movimento mínimo em pixels para acionar motores
GANHO_MOVIMENTO = 3.0       # Multiplicador para aumentar velocidade do movimento
ROI_TAMANHO = 200           # Tamanho inicial da Região de Interesse (pixels)

# ===== VARIÁVEIS =====
optical_flow_ativo = False
az_atual = 0.0
alt_atual = 0.0
camera_ref = None  # Referência da câmera vinda do app.py

# ===== FUNÇÃO PARA ABRIR CÂMERA =====
def abrir_primeira_camera():
    for i in range(5):
        cam = cv2.VideoCapture(i)
        if cam.isOpened():
            print(f"[CÂMERA Optical Flow] Usando dispositivo ID {i}")
            cam.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
            cam.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
            return cam
    raise RuntimeError("[ERRO] Nenhuma câmera disponível.")

def set_camera_reference(cam):
    global camera_ref
    camera_ref = cam

def enviar_para_esp32(az, alt):
    try:
        url = f"http://{IP_ESP32}/mover?az={az:.2f}&alt={alt:.2f}"
        requests.get(url, timeout=1)
        print(f"[ESP32] Enviado: AZ={az:.2f} ALT={alt:.2f}")
    except Exception as e:
        print(f"[ERRO ESP32] {e}")

def optical_flow_loop():
    global optical_flow_ativo, az_atual, alt_atual, camera_ref

    if camera_ref is None:
        print("[AVISO] Nenhuma câmera recebida do app.py. Tentando abrir automaticamente...")
        camera_ref = abrir_primeira_camera()
    else:
        print("[INFO] Usando câmera já aberta no app.py para rastreamento.")

    ultima_pos = None

    while optical_flow_ativo:
        ret, frame = camera_ref.read()
        if not ret:
            break

        h, w = frame.shape[:2]
        centro_x, centro_y = w // 2, h // 2

        # Definir ROI adaptativa
        if ultima_pos:
            x_min = max(0, ultima_pos[0] - ROI_TAMANHO // 2)
            y_min = max(0, ultima_pos[1] - ROI_TAMANHO // 2)
            x_max = min(w, ultima_pos[0] + ROI_TAMANHO // 2)
            y_max = min(h, ultima_pos[1] + ROI_TAMANHO // 2)
            roi = frame[y_min:y_max, x_min:x_max]
            roi_offset = (x_min, y_min)
        else:
            roi = frame
            roi_offset = (0, 0)

        # Escala de cinza
        gray = cv2.cvtColor(roi, cv2.COLOR_BGR2GRAY)

        # Pega ponto mais brilhante
        (minVal, maxVal, minLoc, maxLoc) = cv2.minMaxLoc(gray)

        # Checa brilho mínimo
        if maxVal < BRILHO_MINIMO:
            cv2.putText(frame, "Nenhum ponto brilhante detectado", (30, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
            cv2.imshow("Rastreamento Ponto Brilhante", frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
            continue

        # Ajusta coordenadas para posição real na imagem
        ponto_x = maxLoc[0] + roi_offset[0]
        ponto_y = maxLoc[1] + roi_offset[1]
        ultima_pos = (ponto_x, ponto_y)

        # Diferença até o centro
        dx_pixels = ponto_x - centro_x
        dy_pixels = ponto_y - centro_y

        # Ignora micro-movimentos
        if abs(dx_pixels) < PIXEL_MOV_MIN and abs(dy_pixels) < PIXEL_MOV_MIN:
            cv2.imshow("Rastreamento Ponto Brilhante", frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
            continue

        # Pixels -> Graus
        deg_por_pixel_x = FOV_HORIZONTAL / w
        deg_por_pixel_y = FOV_VERTICAL / h
        delta_az = dx_pixels * deg_por_pixel_x * GANHO_MOVIMENTO
        delta_alt = -dy_pixels * deg_por_pixel_y * GANHO_MOVIMENTO

        # Atualiza posição e envia
        az_atual += delta_az
        alt_atual += delta_alt
        enviar_para_esp32(az_atual, alt_atual)

        # Debug visual
        cv2.circle(frame, (ponto_x, ponto_y), 10, (0, 0, 255), 2)  # ponto
        cv2.circle(frame, (centro_x, centro_y), 5, (255, 0, 0), -1)  # centro
        cv2.putText(frame, f"Brilho: {int(maxVal)}", (10, h - 20),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)
        cv2.putText(frame, f"Delta AZ: {delta_az:.2f}  Delta ALT: {delta_alt:.2f}",
                    (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
        cv2.imshow("Rastreamento Ponto Brilhante", frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cv2.destroyAllWindows()

def iniciar_rastreamento(az_inicial=0.0, alt_inicial=0.0):
    global optical_flow_ativo, az_atual, alt_atual
    az_atual = az_inicial
    alt_atual = alt_inicial
    optical_flow_ativo = True
    threading.Thread(target=optical_flow_loop, daemon=True).start()
    print(f"[INFO] Rastreamento do ponto brilhante iniciado. AZ={az_atual} ALT={alt_atual}")

def parar_rastreamento():
    global optical_flow_ativo
    optical_flow_ativo = False
    print("[INFO] Rastreamento parado.")





#--------------------------------------------------------------------------------------

# import cv2
# import numpy as np
# import threading
# import requests

# # ===== CONFIGURAÇÕES =====
# IP_ESP32 = "192.168.0.105"  # Atualize com o IP fixo da sua ESP32
# FOV_HORIZONTAL = 1.0  # Campo de visão horizontal (em graus)
# FOV_VERTICAL = 0.75   # Campo de visão vertical (em graus)

# # ===== VARIÁVEIS =====
# optical_flow_ativo = False
# az_atual = 0.0
# alt_atual = 0.0
# camera_ref = None  # Referência da câmera vinda do app.py

# def set_camera_reference(cam):
#     """Recebe a referência da câmera aberta no app.py"""
#     global camera_ref
#     camera_ref = cam

# def enviar_para_esp32(az, alt):
#     """Envia novos graus para a ESP32"""
#     try:
#         url = f"http://{IP_ESP32}/mover?az={az:.2f}&alt={alt:.2f}"
#         requests.get(url, timeout=1)
#         print(f"[ESP32] Enviado: AZ={az:.2f} ALT={alt:.2f}")
#     except Exception as e:
#         print(f"[ERRO ESP32] {e}")

# def optical_flow_loop():
#     """Loop do optical flow usando a câmera compartilhada"""
#     global optical_flow_ativo, az_atual, alt_atual, camera_ref

#     if camera_ref is None:
#         print("[ERRO] Nenhuma câmera configurada para Optical Flow!")
#         return

#     # Captura frame inicial
#     ret, frame1 = camera_ref.read()
#     if not ret:
#         print("[ERRO] Não foi possível ler da câmera")
#         return

#     prev_gray = cv2.cvtColor(frame1, cv2.COLOR_BGR2GRAY)

#     while optical_flow_ativo:
#         ret, frame2 = camera_ref.read()
#         if not ret:
#             break

#         gray = cv2.cvtColor(frame2, cv2.COLOR_BGR2GRAY)
#         flow = cv2.calcOpticalFlowFarneback(prev_gray, gray,
#                                             None, 0.5, 3, 15, 3, 5, 1.2, 0)

#         # Movimento médio
#         dx = np.mean(flow[..., 0])
#         dy = np.mean(flow[..., 1])

#         h, w = gray.shape
#         deg_por_pixel_x = FOV_HORIZONTAL / w
#         deg_por_pixel_y = FOV_VERTICAL / h

#         delta_az = dx * deg_por_pixel_x
#         delta_alt = -dy * deg_por_pixel_y  # negativo pois Y é invertido

#         az_atual += delta_az
#         alt_atual += delta_alt

#         enviar_para_esp32(az_atual, alt_atual)

#         prev_gray = gray

#         # Exibir a imagem para debug
#         cv2.imshow("Optical Flow", frame2)
#         if cv2.waitKey(1) & 0xFF == ord('q'):
#             break

#     cv2.destroyAllWindows()

# def iniciar_rastreamento(az_inicial=0.0, alt_inicial=0.0):
#     """Inicia o rastreamento óptico"""
#     global optical_flow_ativo, az_atual, alt_atual
#     az_atual = az_inicial
#     alt_atual = alt_inicial
#     optical_flow_ativo = True
#     threading.Thread(target=optical_flow_loop, daemon=True).start()
#     print("[INFO] Rastreamento óptico iniciado.")

# def parar_rastreamento():
#     """Para o rastreamento óptico"""
#     global optical_flow_ativo
#     optical_flow_ativo = False
#     print("[INFO] Rastreamento óptico parado.")
