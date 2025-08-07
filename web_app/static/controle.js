function enviarComando(direcao) {
    fetch('/controle', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({ comando: direcao }),
    })
    .then(response => {
        if (response.ok) {
            console.log(`Comando "${direcao}" enviado com sucesso.`);
        } else {
            console.error('Erro ao enviar comando.');
        }
    })
    .catch(error => console.error('Erro na requisição:', error));
}
