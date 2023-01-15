# Short esp-idf applications on an ESP32S Node MCU module
![lastupdate](https://img.shields.io/github/last-commit/joubiti/espidf-works)

## Overview
Projects so far:
- GPIO with hardware interrupt triggered by a button press
- Implementation of a RESTful API in which the ESP32 sends data (global counter) to a Flask web server every 5 seconds (HTTP and WiFi client)
- Modified version of the RESTful API in which the ESP32 sends weather data (temperature and humidity using DHT11) using the DHT esp-idf component, to the API endpoint
- Using a shared queue for multiple tasks for multiple real-time data transmission pipelines, using a task for counter incrementation and another task for DHT data processing, tested with Postman:

![image](https://user-images.githubusercontent.com/104909670/212535317-79d6d1b7-dbd0-4b71-8e98-4b056568aa7e.png)




