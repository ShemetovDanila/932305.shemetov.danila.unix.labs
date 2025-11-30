import os
import json
import time
import zipfile
import pika
import psycopg2
from PIL import Image

UPLOAD_FOLDER = '/app/uploads'
RESULT_FOLDER = '/app/results'
os.makedirs(RESULT_FOLDER, exist_ok=True)

def process_image(task):
    img_path = task['file']
    output_path = img_path.replace('/uploads/', '/results/').replace('.jpg', '.webp').replace('.jpeg', '.webp').replace('.png', '.webp')
    
    try:
        with Image.open(img_path) as img:
            if img.width > 1024:
                new_height = int((1024 / img.width) * img.height)
                img = img.resize((1024, new_height), Image.LANCZOS)
            
            os.makedirs(os.path.dirname(output_path), exist_ok=True)
            img.save(output_path, 'WEBP', quality=85)
        
        return output_path
    except Exception as e:
        print(f"Error processing {img_path}: {str(e)}")
        return None

def create_zip(archive_name, processed_files):
    zip_path = os.path.join(RESULT_FOLDER, f"{archive_name}.zip")
    with zipfile.ZipFile(zip_path, 'w') as zipf:
        for file in processed_files:
            if file and os.path.exists(file):
                zipf.write(file, os.path.basename(file))
    return zip_path

def update_db(task_id, result_path):
    conn = psycopg2.connect(
        host="db",
        database="postgres",
        user="postgres",
        password="lab123"
    )
    cur = conn.cursor()
    cur.execute(
        "UPDATE tasks SET status = 'done', result_path = %s WHERE id = %s",
        (result_path, task_id)
    )
    conn.commit()
    cur.close()
    conn.close()

def callback(ch, method, properties, body):
    task = json.loads(body)
    print(f"Received task: {task['archive_name']} (ID: {task['task_id']})")
    
    processed_files = []
    for file_path in task['files']:
        result = process_image({'file': file_path})
        if result:
            processed_files.append(result)
    
    zip_path = create_zip(task['archive_name'], processed_files)
    update_db(task['task_id'], zip_path)
    
    print(f"Task {task['task_id']} completed. Archive: {zip_path}")
    ch.basic_ack(delivery_tag=method.delivery_tag)

while True:
    try:
        connection = pika.BlockingConnection(pika.ConnectionParameters('rabbitmq'))
        channel = connection.channel()
        channel.queue_declare(queue='image_processing')
        channel.basic_qos(prefetch_count=1)
        channel.basic_consume(queue='image_processing', on_message_callback=callback)
        print(" [*] Waiting for tasks. Press CTRL+C to exit")
        channel.start_consuming()
    except pika.exceptions.AMQPConnectionError:
        print("RabbitMQ unavailable. Retrying in 5 seconds...")
        time.sleep(5)
