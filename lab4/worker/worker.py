import os
import time
import zipfile
from confluent_kafka import Consumer, KafkaException

# Конфигурация
KAFKA_BOOTSTRAP = os.getenv('KAFKA_BOOTSTRAP_SERVERS', 'kafka:9092')
UPLOAD_DIR = '/app/uploads'
ARCHIVE_DIR = '/app/archives'
os.makedirs(ARCHIVE_DIR, exist_ok=True)

# Kafka Consumer
conf = {
    'bootstrap.servers': KAFKA_BOOTSTRAP,
    'group.id': 'photo-archive-group',
    'auto.offset.reset': 'earliest'
}

consumer = Consumer(conf)
consumer.subscribe(['photo-archive'])

def archive_photo(filepath, filename):
    """Создает ZIP-архив из фото"""
    archive_path = os.path.join(ARCHIVE_DIR, f"{filename}.zip")
    with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
        zipf.write(filepath, os.path.basename(filepath))
    return archive_path

try:
    print("Worker запущен и ожидает задач...")
    while True:
        msg = consumer.poll(1.0)
        if msg is None:
            continue
        if msg.error():
            raise KafkaException(msg.error())
        
        # Обработка задачи
        filename = msg.key().decode('utf-8')
        filepath = msg.value().decode('utf-8')
        
        print(f"Обрабатываю файл: {filename}")
        try:
            archive_path = archive_photo(filepath, filename)
            print(f"Архив создан: {archive_path}")
            
            # Удаляем исходный файл
            os.remove(filepath)
        except Exception as e:
            print(f"Ошибка обработки: {str(e)}")

except KeyboardInterrupt:
    print("Worker остановлен")
finally:
    consumer.close()
