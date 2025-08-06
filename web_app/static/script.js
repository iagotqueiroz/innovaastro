let ultimoAz = 0;
let ultimoAlt = 0;

function buscarAstro() {
    let campoPesquisa = document.getElementById("campoPesquisa");
    let nomeAstro = campoPesquisa.value;
    let latitude = document.getElementById("latitude").value;
    let longitude = document.getElementById("longitude").value;

    campoPesquisa.value = "";

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
        // Guardar últimos valores para iniciar rastreamento
        ultimoAz = data.az;
        ultimoAlt = data.alt;
    })
    .catch(error => {
        console.error("Erro ao buscar astro:", error);
    });
}

function iniciarRastreamento() {
    // Mostrar vídeo
    document.getElementById("camContainer").innerHTML =
        '<img src="/video_feed" width="640" height="480">';
    
    // Inicia Optical Flow enviando posição atual
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


// function buscarAstro() {
//     let campoPesquisa = document.getElementById("campoPesquisa");
//     let nomeAstro = campoPesquisa.value;
//     let latitude = document.getElementById("latitude").value;
//     let longitude = document.getElementById("longitude").value;

//     campoPesquisa.value = "";

//     fetch('/buscar', {
//         method: 'POST',
//         headers: {
//             'Content-Type': 'application/json'
//         },
//         body: JSON.stringify({ 
//         nome: nomeAstro,
//         latitude: latitude,
//         longitude: longitude
// })

//     })
//     .then(response => response.json())
//     .then(data => {
//         const resultado = document.getElementById("resultado");
//         resultado.innerHTML = `
//             <strong>Astro:</strong> ${data.astro} <br>
//             <strong>Azimute:</strong> ${data.az.toFixed(2)}° <br>
//             <strong>Altitude:</strong> ${data.alt.toFixed(2)}°
//         `;
//     })
//     .catch(error => {
//         console.error("Erro ao buscar astro:", error);
//     });
// }

// function iniciarRastreamento() {
//   // Mostra o vídeo no camContainer
//   document.getElementById("camContainer").innerHTML =
//     '<img src="/video_feed" width="640" height="480">';

//   // Inicia o Optical Flow no backend
//   fetch('/opticalflow/start', {
//     method: 'POST'
//   })
//   .then(res => res.json())
//   .then(data => {
//     console.log(data.status);
//   })
//   .catch(err => console.error(err));
// }

// function pararRastreamento() {
//   fetch('/opticalflow/stop', {
//     method: 'POST'
//   })
//   .then(res => res.json())
//   .then(data => {
//     console.log(data.status);
//   })
//   .catch(err => console.error(err));
// }

//--------------------------------------------------------------------------------------

// function buscarAstro() {
//     let campoPesquisa = document.getElementById("campoPesquisa");
//     let nomeAstro = campoPesquisa.value;
//     let latitude = document.getElementById("latitude").value;
//     let longitude = document.getElementById("longitude").value;

//     campoPesquisa.value = "";

//     fetch('/buscar', {
//         method: 'POST',
//         headers: {
//             'Content-Type': 'application/json'
//         },
//         body: JSON.stringify({ 
//         nome: nomeAstro,
//         latitude: latitude,
//         longitude: longitude
// })

//     })
//     .then(response => response.json())
//     .then(data => {
//         const resultado = document.getElementById("resultado");
//         resultado.innerHTML = `
//             <strong>Astro:</strong> ${data.astro} <br>
//             <strong>Azimute:</strong> ${data.az.toFixed(2)}° <br>
//             <strong>Altitude:</strong> ${data.alt.toFixed(2)}°
//         `;
//     })
//     .catch(error => {
//         console.error("Erro ao buscar astro:", error);
//     });
// }