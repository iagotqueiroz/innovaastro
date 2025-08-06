import cv2
import numpy as np
import threading
import requests
import time

rastreamento_ativo = False
camera_ref = None

def set_camera_reference(camera):
    global camera_ref
    camera_ref = camera

def iniciar_rastreamento():
    global rastreamento_ativo
    rastreamento_ativo = True

    def rastrear():
        global camera_ref
        if camera_ref is None:
            print("[ERRO] Nenhuma câmera configurada.")
            return

        largura = int(camera_ref.get(cv2.CAP_PROP_FRAME_WIDTH))
        altura = int(camera_ref.get(cv2.CAP_PROP_FRAME_HEIGHT))
        centro_x = largura // 2
        centro_y = altura // 2

        print(f"[INFO] Rastreamento por brilho ativado — resolução: {largura}x{altura}")

        while rastreamento_ativo:
            ret, frame = camera_ref.read()
            if not ret:
                print("[ERRO] Falha ao capturar frame.")
                continue

            # Converte para escala de cinza
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

            # Aplica limiar: ignora pontos abaixo de 220 de intensidade
            _, limiar = cv2.threshold(gray, 220, 255, cv2.THRESH_TOZERO)

            # Localiza o pixel mais brilhante acima do limiar
            minVal, maxVal, minLoc, maxLoc = cv2.minMaxLoc(limiar)

            if maxVal <250:
                print("[RASTREAMENTO] Nenhum ponto brilhante encontrado.")
                time.sleep(0.3)
                continue

            brilho_x, brilho_y = maxLoc

            # Diferença em pixels em relação ao centro
            dx = brilho_x - centro_x
            dy = brilho_y - centro_y

            # Converte deslocamento para graus
            fator_pixel_grau = 0.5
            delta_az = dx * fator_pixel_grau
            delta_alt = -dy * fator_pixel_grau

            tolerancia = 20  # zona morta

            if abs(dx) > tolerancia or abs(dy) > tolerancia:
                try:
                    url = f"http://192.168.15.13/mover?az={delta_az:.2f}&alt={delta_alt:.2f}"
                    requests.get(url, timeout=1)
                    print(f"[RASTREAMENTO] Movendo: ΔAZ={delta_az:.2f}, ΔALT={delta_alt:.2f}")
                except Exception as e:
                    print(f"[ERRO] Falha ao mover motores: {e}")
            else:
                print("[RASTREAMENTO] Ponto centralizado.")

            # Desenho visual para debug
            cv2.circle(frame, (brilho_x, brilho_y), 10, (0, 255, 0), 2)  # ponto mais brilhante
            cv2.circle(frame, (centro_x, centro_y), 10, (255, 0, 0), 2)  # centro da imagem
            cv2.imshow("Rastreamento por brilho", frame)

            if cv2.waitKey(1) == 27:  # ESC para sair manualmente
                break

            time.sleep(0.2)  # controle de frequência

        cv2.destroyAllWindows()
        print("[INFO] Rastreamento encerrado.")

    threading.Thread(target=rastrear, daemon=True).start()

def parar_rastreamento():
    global rastreamento_ativo
    rastreamento_ativo = False
    print("[INFO] Rastreamento parado.")





# import cv2
# import numpy as np
# import threading
# import requests

# # ===== CONFIGURAÇÕES =====
# IP_ESP32 = "192.168.15.13"  # IP da sua ESP32
# FOV_HORIZONTAL = 60.0       # Campo de visão horizontal da câmera (graus)
# FOV_VERTICAL = 45.0         # Campo de visão vertical da câmera (graus)
# BRILHO_MINIMO = 200         # Intensidade mínima (0-255) para considerar um ponto
# PIXEL_MOV_MIN = 2           # Movimento mínimo em pixels para acionar motores
# GANHO_MOVIMENTO = 3.0       # Multiplicador para aumentar velocidade do movimento
# ROI_TAMANHO = 200           # Tamanho inicial da Região de Interesse (pixels)

# # ===== VARIÁVEIS =====
# optical_flow_ativo = False
# az_atual = 0.0
# alt_atual = 0.0
# camera_ref = None  # Referência da câmera vinda do app.py

# # ===== FUNÇÃO PARA ABRIR CÂMERA =====
# def abrir_primeira_camera():
#     for i in range(5):
#         cam = cv2.VideoCapture(i)
#         if cam.isOpened():
#             print(f"[CÂMERA Optical Flow] Usando dispositivo ID {i}")
#             cam.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
#             cam.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
#             return cam
#     raise RuntimeError("[ERRO] Nenhuma câmera disponível.")

# def set_camera_reference(cam):
#     global camera_ref
#     camera_ref = cam

# def enviar_para_esp32(az, alt):
#     try:
#         url = f"http://{IP_ESP32}/mover?az={az:.2f}&alt={alt:.2f}"
#         requests.get(url, timeout=1)
#         print(f"[ESP32] Enviado: AZ={az:.2f} ALT={alt:.2f}")
#     except Exception as e:
#         print(f"[ERRO ESP32] {e}")

# def optical_flow_loop():
#     global optical_flow_ativo, az_atual, alt_atual, camera_ref

#     if camera_ref is None:
#         print("[AVISO] Nenhuma câmera recebida do app.py. Tentando abrir automaticamente...")
#         camera_ref = abrir_primeira_camera()
#     else:
#         print("[INFO] Usando câmera já aberta no app.py para rastreamento.")

#     ultima_pos = None

#     while optical_flow_ativo:
#         ret, frame = camera_ref.read()
#         if not ret:
#             break

#         h, w = frame.shape[:2]
#         centro_x, centro_y = w // 2, h // 2

#         # Definir ROI adaptativa
#         if ultima_pos:
#             x_min = max(0, ultima_pos[0] - ROI_TAMANHO // 2)
#             y_min = max(0, ultima_pos[1] - ROI_TAMANHO // 2)
#             x_max = min(w, ultima_pos[0] + ROI_TAMANHO // 2)
#             y_max = min(h, ultima_pos[1] + ROI_TAMANHO // 2)
#             roi = frame[y_min:y_max, x_min:x_max]
#             roi_offset = (x_min, y_min)
#         else:
#             roi = frame
#             roi_offset = (0, 0)

#         # Escala de cinza
#         gray = cv2.cvtColor(roi, cv2.COLOR_BGR2GRAY)

#         # Pega ponto mais brilhante
#         (minVal, maxVal, minLoc, maxLoc) = cv2.minMaxLoc(gray)

#         # Checa brilho mínimo
#         if maxVal < BRILHO_MINIMO:
#             cv2.putText(frame, "Nenhum ponto brilhante detectado", (30, 30),
#                         cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
#             cv2.imshow("Rastreamento Ponto Brilhante", frame)
#             if cv2.waitKey(1) & 0xFF == ord('q'):
#                 break
#             continue

#         # Ajusta coordenadas para posição real na imagem
#         ponto_x = maxLoc[0] + roi_offset[0]
#         ponto_y = maxLoc[1] + roi_offset[1]
#         ultima_pos = (ponto_x, ponto_y)

#         # Diferença até o centro
#         dx_pixels = ponto_x - centro_x
#         dy_pixels = ponto_y - centro_y

#         # Ignora micro-movimentos
#         if abs(dx_pixels) < PIXEL_MOV_MIN and abs(dy_pixels) < PIXEL_MOV_MIN:
#             cv2.imshow("Rastreamento Ponto Brilhante", frame)
#             if cv2.waitKey(1) & 0xFF == ord('q'):
#                 break
#             continue

#         # Pixels -> Graus
#         deg_por_pixel_x = FOV_HORIZONTAL / w
#         deg_por_pixel_y = FOV_VERTICAL / h
#         delta_az = dx_pixels * deg_por_pixel_x * GANHO_MOVIMENTO
#         delta_alt = -dy_pixels * deg_por_pixel_y * GANHO_MOVIMENTO

#         # Atualiza posição e envia
#         az_atual += delta_az
#         alt_atual += delta_alt
#         enviar_para_esp32(az_atual, alt_atual)

#         # Debug visual
#         cv2.circle(frame, (ponto_x, ponto_y), 10, (0, 0, 255), 2)  # ponto
#         cv2.circle(frame, (centro_x, centro_y), 5, (255, 0, 0), -1)  # centro
#         cv2.putText(frame, f"Brilho: {int(maxVal)}", (10, h - 20),
#                     cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)
#         cv2.putText(frame, f"Delta AZ: {delta_az:.2f}  Delta ALT: {delta_alt:.2f}",
#                     (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
#         cv2.imshow("Rastreamento Ponto Brilhante", frame)

#         if cv2.waitKey(1) & 0xFF == ord('q'):
#             break

#     cv2.destroyAllWindows()

# def iniciar_rastreamento(az_inicial=0.0, alt_inicial=0.0):
#     global optical_flow_ativo, az_atual, alt_atual
#     az_atual = az_inicial
#     alt_atual = alt_inicial
#     optical_flow_ativo = True
#     threading.Thread(target=optical_flow_loop, daemon=True).start()
#     print(f"[INFO] Rastreamento do ponto brilhante iniciado. AZ={az_atual} ALT={alt_atual}")

# def parar_rastreamento():
#     global optical_flow_ativo
#     optical_flow_ativo = False
#     print("[INFO] Rastreamento parado.")
