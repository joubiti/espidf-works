from flask import Flask, jsonify, request

#Minimal RESTFul API service implementation using Flask

app = Flask(__name__)

counter = 0

@app.route('/counter', methods=['GET', 'POST'])
def counter_endpoint():
    global counter
    if request.method == 'POST':
        counter = request.json['data']
        return jsonify(status='success')
    elif request.method == 'GET':
        return jsonify(counter=counter)

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0')
