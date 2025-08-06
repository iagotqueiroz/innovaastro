# 🌌 InnovaAstro

Sistema de rastreamento de astros que combina **ESP32 + motores de passo** para movimentação física do telescópio e **interface web** com **Flask + Skyfield** para cálculo da posição dos astros e rastreamento por câmera.

---

## 📂 Estrutura do Projeto

InnovaAstro/
│
├── esp32_astro/ # Código da ESP32 (PlatformIO)
│ ├── src/ # Código-fonte C++ (PlatformIO)
│ └── .gitignore # Arquivos a ignorar no PlatformIO
│
├── web_app/ # Aplicação Flask
│ ├── static/ # CSS, JS, imagens
│ ├── templates/ # HTML
│ ├── optical_flow.py # Rastreamento de ponto brilhante
│ ├── searchAstro.py # Servidor Flask
│ └── de421.bsp # Arquivo de efemérides Skyfield
│
├── .gitignore # Arquivos a ignorar (Python/gerais)
├── README.md # Este documento
└── requirements.txt # Dependências Python


---

## 🚀 Funcionalidades

✅ **Busca por astro** usando Skyfield e movimentação automática para posição inicial  
✅ **Rastreamento por câmera** usando detecção de ponto mais brilhante (laser/estrela)  
✅ **Controle de motores via ESP32** com comunicação HTTP  
✅ **Interface web responsiva** para busca e controle  
✅ **Streaming de vídeo** no navegador para visualização ao vivo  

---

## 💻 Como rodar a parte Web

1. **Ativar ambiente virtual** (se existir):
   ```bash
   .venv\Scripts\activate   # Windows
   source .venv/bin/activate # Linux/Mac

Instalar dependências:

bash
Copiar
Editar
pip install -r requirements.txt
Iniciar servidor Flask:

bash
Copiar
Editar
python web_app/searchAstro.py
Acessar no navegador:

cpp
Copiar
Editar
http://127.0.0.1:5000
🔧 Como programar a ESP32
Abrir a pasta esp32_astro no PlatformIO.

Conectar a ESP32 via USB.

Compilar e enviar o código.

Garantir que a ESP32 está conectada ao Wi-Fi e com IP fixo.

🛠 Tecnologias Utilizadas
Python (Flask, OpenCV, Skyfield)

HTML, CSS, JavaScript

ESP32 + C++ (PlatformIO)

Motores de passo + drivers A4988

HTTP Requests para comunicação

📄 Licença
Projeto de uso pessoal e educacional.

✦ Desenvolvido por Queiroz