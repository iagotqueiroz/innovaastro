alert("Controle.js carregado com sucesso!"); // Verifica se o JS foi carregado

function enviarComando(direcao) {
    alert("Comando: " + direcao); // Verifica se a função foi chamada corretamente
    fetch('/controle', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({ comando: direcao }),
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'sucesso') {
            console.log(`Comando "${direcao}" enviado com sucesso.`);
        } else {
            console.error('Erro ao enviar comando:', data.mensagem);
        }
    })
    .catch(error => {
        console.error('Erro na requisição:', error);
    });
}





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
