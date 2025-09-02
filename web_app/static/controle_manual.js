// controle_manual.js

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
