# import cv2
# import numpy as np
# import threading
# import requests
# import time

# rastreamento_ativo = False
# camera_ref = None

# def set_camera_reference(camera):
#     global camera_ref
#     camera_ref = camera

# def iniciar_rastreamento():
#     global rastreamento_ativo
#     rastreamento_ativo = True

#     def rastrear():
#         global camera_ref
#         if camera_ref is None:
#             print("[ERRO] Nenhuma câmera configurada.")
#             return

#         largura = int(camera_ref.get(cv2.CAP_PROP_FRAME_WIDTH))
#         altura = int(camera_ref.get(cv2.CAP_PROP_FRAME_HEIGHT))
#         centro_x = largura // 2
#         centro_y = altura // 2

#         print(f"[INFO] Rastreamento por brilho ativado — resolução: {largura}x{altura}")

#         while rastreamento_ativo:
#             ret, frame = camera_ref.read()
#             if not ret:
#                 print("[ERRO] Falha ao capturar frame.")
#                 continue

#             gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
#             _, limiar = cv2.threshold(gray, 220, 255, cv2.THRESH_TOZERO)
#             minVal, maxVal, minLoc, maxLoc = cv2.minMaxLoc(limiar)

#             if maxVal <250:
#                 print("[RASTREAMENTO] Nenhum ponto brilhante encontrado.")
#                 time.sleep(0.3)
#                 continue

#             brilho_x, brilho_y = maxLoc
#             dx = brilho_x - centro_x
#             dy = brilho_y - centro_y

#             fator_pixel_grau = 0.5
#             delta_az = dx * fator_pixel_grau
#             delta_alt = -dy * fator_pixel_grau

#             tolerancia = 20

#             if abs(dx) > tolerancia or abs(dy) > tolerancia:
#                 try:
#                     url = f"http://192.168.15.14/mover?az={delta_az:.2f}&alt={delta_alt:.2f}"
#                     requests.get(url, timeout=1)
#                     print(f"[RASTREAMENTO] Movendo: ΔAZ={delta_az:.2f}, ΔALT={delta_alt:.2f}")
#                 except Exception as e:
#                     print(f"[ERRO] Falha ao mover motores: {e}")
#             else:
#                 print("[RASTREAMENTO] Ponto centralizado.")

#             cv2.circle(frame, (brilho_x, brilho_y), 10, (0, 255, 0), 2)
#             cv2.circle(frame, (centro_x, centro_y), 10, (255, 0, 0), 2)
#             cv2.imshow("Rastreamento por brilho", frame)

#             if cv2.waitKey(1) == 27:
#                 break

#             time.sleep(0.2)

#         cv2.destroyAllWindows()
#         print("[INFO] Rastreamento encerrado.")

#     threading.Thread(target=rastrear, daemon=True).start()

# def parar_rastreamento():
#     global rastreamento_ativo
#     rastreamento_ativo = False
#     print("[INFO] Rastreamento parado.")
