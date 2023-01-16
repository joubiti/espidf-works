from flask import Flask, jsonify, request
from datetime import datetime
from openpyxl import Workbook, load_workbook

app = Flask(__name__)

wb = load_workbook("data_log.xlsx")
ws = wb.worksheets[0]
counter = 0
temperature = 0
humidity = 0

@app.route('/weather', methods=['POST', 'GET'])
def weather_endpoint():
    global counter, temperature, humidity
    if request.method== 'POST':
        data = request.json
        counter = data['counter']
        temperature = data['temp']
        humidity = data['humidity']
        now = datetime.now()
        timestamp = now.strftime("%d-%m-%Y %H:%M:%S")
        ws.append([timestamp, counter, f'{temperature}C', f'{humidity}%'])
        wb.save("data_log.xlsx")
        return jsonify(status='success')
    elif request.method== 'GET':
        return(jsonify(counter= counter, temperature= temperature, humidity= humidity))

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0')