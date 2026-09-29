// controle_manual.js

const btnHome = document.getElementById("btnHome");
const homeStatus = document.getElementById("homeStatus");

const botoesManuais = [
    document.getElementById("btnUp"),
    document.getElementById("btnDown"),
    document.getElementById("btnLeft"),
    document.getElementById("btnRight")
];

btnHome.addEventListener("click", async function () {

    btnHome.disabled = true;


    homeStatus.textContent =
        "Inicializando telescópio: HOME AZ → HOME ALT...";

    try {

        const resposta = await fetch("/home", {
            method: "POST"
        });

        const dados = await resposta.json();

        if (!resposta.ok || !dados.ok) {
            throw new Error(
                dados.msg || "Falha ao executar HOME."
            );
        }

        homeStatus.textContent =
            "Telescópio inicializado ✓ AZ = 0 / ALT = 0";

    } catch (erro) {

        homeStatus.textContent =
            "Erro no HOME: " + erro.message;

    } finally {

        btnHome.disabled = false;
    }
});

document.getElementById("btnUp").addEventListener("click", function() {
    fetch('/mover?comando=cima')
        .then(response => response.json())
        .then(data => console.log(data));
});

document.getElementById("btnDown").addEventListener("click", function() {
    fetch('/mover?comando=baixo')
        .then(response => response.json())
        .then(data => console.log(data));
});

document.getElementById("btnLeft").addEventListener("click", function() {
    fetch('/mover?comando=esquerda')
        .then(response => response.json())
        .then(data => console.log(data));
});

document.getElementById("btnRight").addEventListener("click", function() {
    fetch('/mover?comando=direita')
        .then(response => response.json())
        .then(data => console.log(data));
});





// function enviarComando(direcao) {
//     fetch('/controle/manual', {
//         method: 'POST',
//         headers: {
//             'Content-Type': 'application/json',
//         },
//         body: JSON.stringify({ comando: direcao }),
//     })
//     .then(response => {
//         if (response.ok) {
//             console.log(`Comando "${direcao}" enviado com sucesso.`);
//         } else {
//             console.error('Erro ao enviar comando.');
//         }
//     })
//     .catch(error => {
//         console.error('Erro na requisição:', error);
//     });
// }
