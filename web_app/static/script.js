let ultimoAz = 0;
let ultimoAlt = 0;

function buscarAstro() {
    const resultado = document.getElementById("resultado");
    const nomeAstro = document.getElementById("campoPesquisa").value;
    const latitude = document.getElementById("latitude").value;
    const longitude = document.getElementById("longitude").value;
    const btnBuscar = document.getElementById("buscar");
    resultado.innerHTML = "";
    btnBuscar.textContent = 'Buscando...';
    document.getElementById("campoPesquisa").value = "";

    fetch('/buscar', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            nome: nomeAstro,
            latitude: latitude,
            longitude: longitude
        })
    })
    .then(async response => {
        const data = await response.json();

        if (!response.ok || data.ok === false) {
            throw data;
        }

        return data;
    })
    .then(data => {
        btnBuscar.textContent = 'Buscar';
        // document.getElementById("latitude").value = "";
        // document.getElementById("longitude").value = "";

        resultado.innerHTML = `
        <div class="flex flex-col gap-4 p-4 bg-gray-50 border-1 border-gray-100 mt-4">
            <div><strong>Astro:</strong> ${data.astro} <br></div>
            <div><strong>Azimute:</strong> ${data.az.toFixed(2)}° <br></div>
            <div><strong>Altitude:</strong> ${data.alt.toFixed(2)}°</div>
        </div>
        `;

        // Atualiza as variáveis de azimute e altitude para o rastreamento
        ultimoAz = data.az;
        ultimoAlt = data.alt;

        // Inicia o rastreamento (se necessário)
        iniciarRastreamento(nomeAstro, latitude, longitude);
    })
    .catch(error => {
        btnBuscar.textContent = 'Buscar';

        console.error("Erro ao buscar astro:", error);

        if (error.alt !== undefined) {

            resultado.innerHTML = `
            <div class="flex flex-col gap-2 p-4 bg-gray-50 border border-gray-200 mt-4">
                <div><strong>Movimento bloqueado por segurança</strong></div>
                <div>${error.erro}</div>
                <div>Azimute: ${error.az.toFixed(2)}°</div>
                <div>Altitude: ${error.alt.toFixed(2)}°</div>
            </div>
            `;

        } else {

            resultado.innerHTML = `
                <div class="p-4 mt-4">
                    Erro: ${error.erro || "Não foi possível buscar o astro."}
                </div>
            `;
        }
    });
}


function iniciarRastreamento(nomeAstro, latitude, longitude) {
    console.log("[RASTREAMENTO] Iniciando:", nomeAstro, latitude, longitude);

    fetch('/seguir', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({
            nome: nomeAstro,
            latitude: latitude,
            longitude: longitude
        })
    })
    .then(response => response.json())
    .then(data => {
        console.log("[RASTREAMENTO] Resposta:", data);

        if (!data.ok) {
            console.error("[RASTREAMENTO] Falha:", data.msg);
            return;
        }

        console.log("[RASTREAMENTO] Seguimento contínuo ativo.");
    })
    .catch(error => {
        console.error("[RASTREAMENTO] Erro ao iniciar:", error);
    });
}


function pararTudo() {
    fetch('/parar', {
        method: 'POST'
    })
    .then(response => response.json())
    .then(data => {
        console.log("[STOP]", data);
    })
    .catch(error => {
        console.error("[STOP] Erro:", error);
    });
}