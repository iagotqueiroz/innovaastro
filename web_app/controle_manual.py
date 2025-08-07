from flask import request, jsonify
from config import ESP32_IP
import requests

def enviar_comando_manual():
    data = request.get_json()
    comando = data.get('comando', '')

    if not comando:
        return jsonify({'status': 'erro', 'mensagem': 'Comando vazio'}), 400

    try:
        response = requests.get(f"http://{ESP32_IP}/controle?comando={comando}")
        if response.status_code == 200:
            return jsonify({'status': 'sucesso', 'mensagem': f'Comando "{comando}" enviado com sucesso'})
        else:
            return jsonify({'status': 'erro', 'mensagem': 'Falha na ESP32'}), 500
    except Exception as e:
        return jsonify({'status': 'erro', 'mensagem': str(e)}), 500
