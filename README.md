# EMBEDDED_CAPSTONE_PROJECT
This is a peltier driven cooling system featuring a web interface, autonomous power management, and real-time text alert updates from the system.

The system diagram below shows the overall setup.
<img width="863" height="647" alt="image" src="https://github.com/user-attachments/assets/07baa29c-dc1e-4976-9e74-4350f5b4106b" />

The system is built around a Custom RTOS running 4 tasks including sampling, sending, relay control, and data logging.

Current, Voltage, and Power is sampled at 1khz. Every 1000 samples, the system averages the data, appends the statuses, and sends it all over UART to an ESP32. The ESP32 then transmits this data over WiFi to the webserver.

The webserver utilizes the Node.js framework as well as a locally run mysql database. Upon receiving data, the server processes the statuses, and acts accordingly. It logs one every 60 datapoints for the graph, and updates the frontend UI once every second through websockets.

The RTOS handles the logic for deciding when it is necessary to send a text update. It will send a text when the 1 minute running average reaches 10W and turns the system off, as well as when the temperature goes above 80 degrees and turns the system on. The texts are sent using the Twilio api.

Build/run Instructions:
The main project is split into 3 folders: ESP, STM32, and Webserver. 

ESP:
The ESP folder contains the PeltierMiddleMan.ino file, which should be flashed to the ESP32 with the Arduino IDE once the necessary libraries are installed. Before you flash, make sure to update the WiFi credentials as well as the host, which you can either put your laptop name if you have that set up or your laptop's IP address.

STM32:
The STM32 folder contains three important files. Before doing anything with these files, you should create a new project in the Arduino IDE. Then navigate the project folder and add in the HardwareAPI.h and HardwareAPI.c files. The last step is to copy the code inside of RTOS.c into the arduino .ino file.

Webserver:
In order to use the webserver, Node.js and MySQL will need to be installed. You can find easy tutorials online for this. Once they are installed, you can proceed with setting up the database. You will need to create a new database named Peltier, then run the SQL command inside of db.sql. After that, go ahead and copy the folder, cd into it, and run npm install. Once this is done, you should be able to run the server using the command: node app.js. After that, go to localhost:3000 and you should be able to see the dashboard.

