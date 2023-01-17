# Short esp-idf applications on an ESP32S Node MCU module
![lastupdate](https://img.shields.io/github/last-commit/joubiti/espidf-works)

## Overview
Projects so far:
- GPIO with hardware interrupt triggered by a button press
- Implementation of a RESTful API in which the ESP32 sends data (global counter) to a Flask web server every 5 seconds (HTTP and WiFi client)
- Modified version of the RESTful API in which the ESP32 sends weather data (temperature and humidity using DHT11) using the DHT esp-idf component, to the API endpoint
- Using a shared queue for multiple tasks for multiple real-time data transmission pipelines, using a task for counter incrementation and another task for DHT data processing, tested with Postman:

![image](https://user-images.githubusercontent.com/104909670/212535317-79d6d1b7-dbd0-4b71-8e98-4b056568aa7e.png)

- Timestamping POST requests on the Flask server and importing an excel spreadsheet for weather data logging and ensuring the ESP32 is sending out messages every 5 seconds

![image](https://user-images.githubusercontent.com/104909670/212550797-a411c40e-0c66-445c-84ab-99a4b07e8ccf.png)

- Implementation of deep sleep state in between data transmissions (every minute) over HTTP using the real time clock
- Using RTC memory to store data in between deep sleep resets
- Using Event Groups to synchronize the tasks such as the HTTP POST request only occurs after all the data fetching tasks have run
- Using MQTT on subscribe data events to trigger ESP32 data transmission using task notifications and MQTT callback function



