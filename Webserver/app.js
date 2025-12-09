const express = require('express');
const mysql = require('mysql2/promise');
const path = require('path');
require('dotenv').config();
const WebSocket = require('ws');
const http = require('http');
const twilio = require('twilio')(process.env.TWILIO_ACCOUNT_SID, process.env.TWILIO_AUTH_TOKEN);

const app = express();
const PORT = process.env.PORT || 3000;

const server = http.createServer(app);

// WebSocket server
const wss = new WebSocket.Server({ server });

app.use(express.json());
app.use(express.urlencoded({ extended: true }));


// MySQL pool
const pool = mysql.createPool({
	host: 'localhost',
	user: process.env.DB_USERNAME,
	password: process.env.DB_PASSWORD,
	database: 'Peltier',
	waitForConnections: true,
	connectionLimit: 10,
	queueLimit: 0,
	dateStrings: true    // Prevents timezone conversion
});

// Test database connection
pool.getConnection()
.then(connection => {
    console.log('Connected to MySQL database');
    connection.release();
})
.catch(err => {
    console.error('Error connecting to MySQL:', err.message);
});

// Track last text message send time to prevent over sending
let lastTextMessageTime = 0;
const TEXT_MESSAGE_COOLDOWN = 5000;

// Twilio text message function
async function sendTextTwilio(number, message) {
    const now = Date.now();
    const timeSinceLastMessage = now - lastTextMessageTime;
    
    if (timeSinceLastMessage < TEXT_MESSAGE_COOLDOWN) {
        console.log(`Text message rate limited. Last message sent ${timeSinceLastMessage}ms ago. Skipping.`);
        return;
    }
    
    let msgOptions = {
        from: process.env.TWILIO_PHONE_NUMBER,
        to: number,
        body: message
    }
    try {
        const result = await twilio.messages.create(msgOptions);
        lastTextMessageTime = Date.now(); // Update timestamp only on successful send
        return result;
    } catch (error) {
        // console.log(error);
    }
}

// Websocket
wss.on('connection', (ws, req) => {

	const urlParams = new URL(req.url, 'http://localhost:3000').searchParams;

	// 'esp' for ESP32 client, 'web' for web client
	clientId = urlParams.get('id');

	if (!clientId) {
		console.log('No client ID provided');
		ws.terminate();
		return;
	}

	console.log('Client connected with ID:', clientId);
	ws.clientId = clientId;

	ws.on('pong', () => {
		ws.isAlive = true;
	});

	ws.on('close', () => {
		ws.terminate();
		console.log('Client disconnected:', ws.clientId);
	});

	ws.on('error', (error) => {
		console.error('Error from client:', error);
	});

	// From client
	ws.on('message', async (message) => {
		// console.log(message.toString());
		if (ws.clientId === 'web' && message.toString() === 'refresh') {
			broadcastMoreData(new Date(Date.now() - (60 * 60 * 24 * 1000)));
			return;
		}
		
		let messageData;
        try {
            messageData = JSON.parse(message);
        } catch (error) {
            console.error(`Failed to parse JSON from ${ws.clientId}:`, message);
            return;
        }
		if (ws.clientId === 'esp') {
			// console.log('Received data from ESP32 client:', messageData);
			if (messageData.type === 'sensorData') {
				if (messageData.data.logData === true) {
					await pool.execute('INSERT INTO DataPoint (datetime, fanVoltage, fanCurrent, fanPower, pelVoltage, pelCurrent, pelPower, temperature, fan_status, pel_status) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)', [new Date(), messageData.data.fanVoltage, messageData.data.fanCurrent, messageData.data.fanPower, messageData.data.pelVoltage, messageData.data.pelCurrent, messageData.data.pelPower, messageData.data.temperature, messageData.data.fanStatus, messageData.data.pelStatus]);
				}
				broadcastIndividualData(messageData.data);
				if ('textStatus' in messageData.data && !isNaN(messageData.data.textStatus) && parseInt(messageData.data.textStatus) > 0) {
					sendTextTwilio(process.env.USER_PHONE_NUMBER, parseInt(messageData.data.textStatus) == 1 ? "1 minute running average exceeded 10W, powering system off." : "Temperature exceeded 80 degrees, turning system on.");
				}
			}
		} else if (ws.clientId === 'web') {
			console.log('Received data from web client:', messageData);
		}
	});


});



// Websocket functions
function broadcastIndividualData(data) {
	wss.clients.forEach((client) => {
		if (client.readyState === WebSocket.OPEN && client.clientId === 'web') {
			client.send(JSON.stringify({'data': data, 'type': 'individualData'}));
		}
	});
}

async function broadcastMoreData(after) {
	const r = await getDataPoints(after);
	wss.clients.forEach((client) => {
		if (client.readyState === WebSocket.OPEN && client.clientId === 'web') {
			client.send(JSON.stringify({'data': r, 'type': 'moreData'}));
		}
	})
}

function broadcastControlData(data) {
	wss.clients.forEach((client) => {
		if (client.readyState === WebSocket.OPEN && client.clientId === 'esp') {
			client.send(JSON.stringify({'data': data, 'type': 'controlData'}));
		}
	})
}


// Updates clients every 10 seconds with more data
setInterval(() => {
	if (wss.clients.size > 0) {
		broadcastMoreData(after=new Date(Date.now() - (60 * 60 * 24 * 1000)));  // Send all data in the last 24 hours
	}
}, 5000);

// Ping clients to keep connections alive every 30 seconds
setInterval(() => {
	wss.clients.forEach((client) => {
		if (client.isAlive == false) return client.terminate();
		client.isAlive = false;
		client.ping(() => {});
	});
}, 30000);




// Get data functions
async function getDataPoints(after) {
	try {
		const r = (await pool.execute('SELECT * FROM DataPoint WHERE datetime > ? ORDER BY datetime ASC', [after]))[0];
		return r;
	} catch (error) {
		console.log('Error in getDataPoints:', error);
		return [];
	}
}


// Routes
app.get('/', (req, res) => {
	res.sendFile(path.join(__dirname, 'public', 'static', 'dashboard.html'));
});


// Front end data request
app.get('/api/data', async (req, res) => {
	try {
		const r = await getDataPoints(after=new Date(Date.now() - (60 * 60 * 24 * 1000)));
		res.json({'data': r, 'type': 'moreData'});
	} catch (error) {
		console.log('Error in /api/data:', error);
		res.status(500).json({ error: 'Error getting data' });
	}
});

// Insert data
app.post('/api/data', async (req, res) => {
	try {
		const { fanVoltage, fanCurrent, fanPower, pelVoltage, pelCurrent, pelPower, temperature, fanStatus, pelStatus, logData } = req.body;
		if (logData === true) {
			console.log('Inserting data into database');
			const r = await pool.execute('INSERT INTO DataPoint (datetime, fanVoltage, fanCurrent, fanPower, pelVoltage, pelCurrent, pelPower, temperature, fan_status, pel_status) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)', [new Date(), fanVoltage, fanCurrent, fanPower, pelVoltage, pelCurrent, pelPower, temperature, fanStatus, pelStatus]);
		}
		// Broadcast data to front end clients
		broadcastIndividualData({fanVoltage, fanCurrent, fanPower, pelVoltage, pelCurrent, pelPower, temperature, fanStatus, pelStatus, logData});
		return res.send({'success': true, 'message': 'Data inserted successfully'});
	} catch (error) {
		console.log('Error in /api/data:', error);
		res.status(500).json({ error: 'Error inserting data' });
	}
});


app.post('/api/control', async (req, res) => {
	try {
		const { device, status } = req.body;
		if (device === 'fan') {
			broadcastControlData({fanStatus: status});
		} else if (device === 'peltier') {
			broadcastControlData({pelStatus: status});
		}
		return res.send({'success': true, 'message': 'Control data sent successfully'});
	} catch (error) {
		console.log('Error in /api/control:', error);
		res.status(500).json({ success: false, error: 'Error sending control data' });
	}
});



// For adding test data
async function addSampleData(points=100) {
	for (let i = 0; i < points; i++) {
		const r = await pool.execute('INSERT INTO DataPoint (datetime, fanVoltage, fanCurrent, fanPower, pelVoltage, pelCurrent, pelPower, temperature, fan_status, pel_status) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)', [new Date(), Math.random() * 100, Math.random() * 100, Math.random() * 100, Math.random() * 100, Math.random() * 100, Math.random() * 100, Math.random() * 100, Math.random() > 0.5, Math.random() > 0.5]);
	}
}

// addSampleData(points=20);



// Start server
server.listen(PORT, () => {
  	console.log(`Server is running on http://localhost:${PORT}`);
	console.log(`WebSocket server ready on ws://localhost:${PORT}`);
});

