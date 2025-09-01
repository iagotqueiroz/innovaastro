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
    .then(response => response.json())
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
        iniciarRastreamento();
    })
    .catch(error => {
        console.error("Erro ao buscar astro:", error);
    });
}
