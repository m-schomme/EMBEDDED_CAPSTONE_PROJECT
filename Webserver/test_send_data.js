async function sendData() {
    url = 'http://localhost:3000/api/data';

    fetch(url, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({
            fanVoltage: 12,
            fanCurrent: 1.00,
            fanPower: 12.00,
            pelVoltage: 12,
            pelCurrent: 1.00,
            pelPower: 12.00,
            temperature: 25.00,
            fanStatus: true,
            pelStatus: true,
            logData: true,  // Changed from logData to match server
        }),
    }).then(response => response.json())
    .then(data => {
        console.log(data);
    })
    .catch(error => {
        console.error('Error sending data:', error);
    });
}


sendData();