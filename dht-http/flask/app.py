from flask import Flask, jsonify, request

app = Flask(__name__)


temp = 0
humidity = 0

@app.route('/weather', methods=['GET', 'POST'])
def weather_endpoint():
    global temp, humidity
    if request.method == 'POST':
        temp = request.json['temp']
        humidity = request.json['humidity']
        return jsonify(status='success')
    elif request.method == 'GET':
        return jsonify(temp=temp, humidity=humidity)

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0')