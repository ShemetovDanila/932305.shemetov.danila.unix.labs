import os
from flask import Flask, request, jsonify
from confluent_kafka import Producer
import uuid

app = Flask(__name__)
UPLOAD_FOLDER = '/app/uploads'
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

# Конфигурация Kafka
conf = {'bootstrap.servers': os.getenv('KAFKA_BOOTSTRAP_SERVERS', 'kafka:9092')}
producer = Producer(conf)

@app.route('/health', methods=['GET'])
def health():
    return jsonify({"status": "ok", "service": "photo-archive-web"})

@app.route('/api/upload', methods=['POST'])
def upload_file():
    if 'file' not in request.files:
        return jsonify({"error": "No file part"}), 400
    
    file = request.files['file']
    if file.filename == '':
        return jsonify({"error": "No selected file"}), 400
    
    # Сохраняем файл
    filename = f"{uuid.uuid4().hex}_{file.filename}"
    filepath = os.path.join(UPLOAD_FOLDER, filename)
    file.save(filepath)
    
    # Отправляем задачу в Kafka
    producer.produce('photo-archive', key=filename, value=filepath)
    producer.flush()
    
    return jsonify({
        "message": "File queued for archiving",
        "filename": filename,
        "download_url": f"http://localhost/archives/{filename}.zip"
    }), 202

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
