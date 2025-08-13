from flask import request, jsonify
from config import ESP32_IP
import requests

def enviar_comando_manual(comando):
    if not comando:
        return jsonify({'status': 'erro', 'mensagem': 'Comando vazio'}), 400

    try:
        response = requests.get(f"http://{ESP32_IP}/controle?comando={comando}")
        print(f"[ESP32] Resposta da ESP32: {response.content}")  # Verificando o conteúdo da resposta
        if response.status_code == 200:
            try:
                response_json = response.json()  # Tenta fazer o parse do JSON
                return jsonify({'status': 'sucesso', 'mensagem': response_json})
            except ValueError:
                return jsonify({'status': 'sucesso', 'mensagem': response.text})  # Caso não seja um JSON, retorna como texto
        else:
            print(f"[ESP32] Falha na comunicação com a ESP32. Status code: {response.status_code}")
            return jsonify({'status': 'erro', 'mensagem': 'Falha na ESP32'}), 500
    except Exception as e:
        print(f"[ERRO] Falha na comunicação com a ESP32: {e}")
        return jsonify({'status': 'erro', 'mensagem': str(e)}), 500
