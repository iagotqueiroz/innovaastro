let ultimoAz = 0;
let ultimoAlt = 0;

function buscarAstro() {
    const nomeAstro = document.getElementById("campoPesquisa").value;
    const latitude = document.getElementById("latitude").value;
    const longitude = document.getElementById("longitude").value;

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
        const resultado = document.getElementById("resultado");
        resultado.innerHTML = `
            <strong>Astro:</strong> ${data.astro} <br>
            <strong>Azimute:</strong> ${data.az.toFixed(2)}° <br>
            <strong>Altitude:</strong> ${data.alt.toFixed(2)}°
        `;
        ultimoAz = data.az;
        ultimoAlt = data.alt;
    })
    .catch(error => {
        console.error("Erro ao buscar astro:", error);
    });
}

function iniciarRastreamento() {
    document.getElementById("camContainer").innerHTML =
        '<img src="/video_feed" width="640" height="480">';

    fetch('/opticalflow/start', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ az: ultimoAz, alt: ultimoAlt })
    })
    .then(res => res.json())
    .then(data => console.log(data.status))
    .catch(err => console.error(err));
}

function pararRastreamento() {
    fetch('/opticalflow/stop', {
        method: 'POST'
    })
    .then(res => res.json())
    .then(data => console.log(data.status))
    .catch(err => console.error(err));
}
