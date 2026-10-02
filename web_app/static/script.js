let ultimoAz = 0;
let ultimoAlt = 0;

let ultimoAstro = "";
let ultimaLatitude = null;
let ultimaLongitude = null;

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

            <div>
                <strong>Astro:</strong>
                ${data.astro}
            </div>

            <div>
                <strong>Azimute:</strong>
                ${data.az.toFixed(2)}°
            </div>

            <div>
                <strong>Altitude:</strong>
                ${data.alt.toFixed(2)}°
            </div>

        <div class="mt-3 p-4 border border-gray-200 rounded-md">

            <div class="font-bold mb-2">
                Ajuste fino durante o rastreio
            </div>

            <div class="text-sm text-gray-500 mb-3">
                Cada clique faz um ajuste fino de 0,25°.
            </div>

            <div class="grid grid-cols-3 gap-2 max-w-52">

                <div></div>

                <button
                    class="btnNudge bg-slate-700 text-white p-3 rounded disabled:opacity-40"
                    onclick="ajustarTracking('cima')"
                    disabled>
                    ↑
                </button>

                <div></div>


                <button
                    class="btnNudge bg-slate-700 text-white p-3 rounded disabled:opacity-40"
                    onclick="ajustarTracking('esquerda')"
                    disabled>
                    ←
                </button>

                <div class="flex items-center justify-center text-xs">
                    TRACK
                </div>

                <button
                    class="btnNudge bg-slate-700 text-white p-3 rounded disabled:opacity-40"
                    onclick="ajustarTracking('direita')"
                    disabled>
                    →
                </button>


                <div></div>

                <button
                    class="btnNudge bg-slate-700 text-white p-3 rounded disabled:opacity-40"
                    onclick="ajustarTracking('baixo')"
                    disabled>
                    ↓
                </button>

                <div></div>

            </div>

            <div
                id="nudgeStatus"
                class="text-sm mt-3">
                Aguardando tracking...
            </div>

        </div>

            <button
                id="btnAlinhar"
                onclick="alinharNesteAstro()"
                class="bg-green-600 text-white p-3 rounded-md">
                ALINHAR NESTE ASTRO
            </button>

            <div id="resultadoAlinhamento"></div>

        </div>
        `;

        // Atualiza as variáveis de azimute e altitude para o rastreamento
        ultimoAz = data.az;
        ultimoAlt = data.alt;

        ultimoAstro = data.astro;
        ultimaLatitude = parseFloat(latitude);
        ultimaLongitude = parseFloat(longitude);

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


async function alinharNesteAstro() {

    const btn = document.getElementById("btnAlinhar");
    const resultadoAlinhamento =
        document.getElementById("resultadoAlinhamento");


    if (!ultimoAstro) {

        resultadoAlinhamento.innerHTML =
            "Nenhum astro selecionado.";

        return;
    }


    btn.disabled = true;
    btn.textContent = "Alinhando...";


    try {

        const resposta = await fetch("/alinhar", {

            method: "POST",

            headers: {
                "Content-Type": "application/json"
            },

            body: JSON.stringify({
                nome: ultimoAstro,
                latitude: ultimaLatitude,
                longitude: ultimaLongitude
            })
        });


        const dados = await resposta.json();


        if (!resposta.ok || !dados.ok) {

            throw new Error(
                dados.erro ||
                "Não foi possível realizar o alinhamento."
            );
        }


        resultadoAlinhamento.innerHTML = `

            <div class="mt-4 p-4 border border-green-200 bg-green-50">

                <div class="font-bold mb-3">
                    ✓ Alinhamento registrado
                </div>

                <div>
                    <strong>Astro:</strong>
                    ${dados.astro}
                </div>

                <hr class="my-3">

                <div>
                    <strong>Posição real no céu</strong>
                </div>

                <div>
                    AZ:
                    ${dados.az_ceu.toFixed(2)}°
                </div>

                <div>
                    ALT:
                    ${dados.alt_ceu.toFixed(2)}°
                </div>

                <hr class="my-3">

                <div>
                    <strong>Posição dos motores</strong>
                </div>

                <div>
                    AZ:
                    ${dados.az_motor.toFixed(2)}°
                </div>

                <div>
                    ALT:
                    ${dados.alt_motor.toFixed(2)}°
                </div>

                <hr class="my-3">

                <div>
                    <strong>Correção calculada</strong>
                </div>

                <div>
                    AZ:
                    ${dados.offset_az >= 0 ? "+" : ""}
                    ${dados.offset_az.toFixed(2)}°
                </div>

                <div>
                    ALT:
                    ${dados.offset_alt >= 0 ? "+" : ""}
                    ${dados.offset_alt.toFixed(2)}°
                </div>

            </div>
        `;


    } catch (erro) {

        resultadoAlinhamento.innerHTML = `

            <div class="mt-4 p-4 border border-red-200 bg-red-50">

                Erro no alinhamento:
                ${erro.message}

            </div>
        `;

    } finally {

        btn.disabled = false;
        btn.textContent = "ALINHAR NESTE ASTRO";
    }
}


function habilitarNudge(ativo) {

    document
        .querySelectorAll(".btnNudge")
        .forEach(botao => {
            botao.disabled = !ativo;
        });

    const status =
        document.getElementById("nudgeStatus");

    if (status) {

        status.textContent = ativo
            ? "Tracking ativo. Ajuste fino liberado."
            : "Aguardando tracking...";
    }
}


async function ajustarTracking(direcao) {

    const status =
        document.getElementById("nudgeStatus");

    try {

        const resposta = await fetch(
            "/ajuste_tracking",
            {
                method: "POST",

                headers: {
                    "Content-Type": "application/json"
                },

                body: JSON.stringify({
                    direcao: direcao
                })
            }
        );

        const dados =
            await resposta.json();

        if (!resposta.ok || !dados.ok) {

            throw new Error(
                dados.msg ||
                "Não foi possível aplicar o ajuste."
            );
        }

        if (status) {

            status.textContent =
                `Ajuste: ${direcao}`;
        }

        console.log(
            "[NUDGE]",
            dados
        );

    } catch (erro) {

        if (status) {

            status.textContent =
                "Erro: " + erro.message;
        }

        console.error(
            "[NUDGE]",
            erro
        );
    }
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
            habilitarNudge(false);
            return;
        }

        console.log("[RASTREAMENTO] Seguimento contínuo ativo.");

        habilitarNudge(true);
    })
    .catch(error => {
        console.error("[RASTREAMENTO] Erro ao iniciar:", error);
        habilitarNudge(false);
    });
}


function pararTudo() {
    fetch('/parar', {
        method: 'POST'
    })
    .then(response => response.json())
    .then(data => {
        console.log("[STOP]", data);
        habilitarNudge(false);
    })
    .catch(error => {
        console.error("[STOP] Erro:", error);
    });
}