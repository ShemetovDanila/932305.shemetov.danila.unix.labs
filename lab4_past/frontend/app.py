import os
import json
import pika
import psycopg2
from flask import Flask, render_template, request, send_from_directory, redirect

app = Flask(__name__)
UPLOAD_FOLDER = '/app/uploads'
RESULT_FOLDER = '/app/results'
os.makedirs(UPLOAD_FOLDER, exist_ok=True)
os.makedirs(RESULT_FOLDER, exist_ok=True)

def get_db():
    return psycopg2.connect(
        host="db",
        database="postgres",
        user="postgres",
        password="lab123"
    )

with get_db() as conn:
    with conn.cursor() as cur:
        cur.execute("""
        CREATE TABLE IF NOT EXISTS tasks (
            id SERIAL PRIMARY KEY,
            archive_name VARCHAR(255) NOT NULL,
            status VARCHAR(20) DEFAULT 'pending',
            result_path VARCHAR(255)
        )
        """)
    conn.commit()

@app.route('/', methods=['GET', 'POST'])
def index():
    if request.method == 'POST':
        archive_name = request.form['archive_name']
        files = request.files.getlist("photos")
        
        file_paths = []
        for file in files:
            path = os.path.join(UPLOAD_FOLDER, file.filename)
            file.save(path)
            file_paths.append(path)
        
        with get_db() as conn:
            with conn.cursor() as cur:
                cur.execute(
                    "INSERT INTO tasks (archive_name) VALUES (%s) RETURNING id",
                    (archive_name,)
                )
                task_id = cur.fetchone()[0]
            conn.commit()
        
        connection = pika.BlockingConnection(pika.ConnectionParameters('rabbitmq'))
        channel = connection.channel()
        channel.queue_declare(queue='image_processing')
        channel.basic_publish(
            exchange='',
            routing_key='image_processing',
            body=json.dumps({
                'task_id': task_id,
                'archive_name': archive_name,
                'files': file_paths
            })
        )
        connection.close()
        
        return redirect(f'/task/{task_id}')
    
    return render_template('index.html')

@app.route('/task/<int:task_id>')
def task_status(task_id):
    with get_db() as conn:
        with conn.cursor() as cur:
            cur.execute("SELECT status, result_path FROM tasks WHERE id = %s", (task_id,))
            task = cur.fetchone()
    
    if not task:
        return "Task not found", 404
    
    status, result_path = task
    if status == 'done' and result_path:
        return f'''
        <h2>Done!</h2>
        <p>Archive: {os.path.basename(result_path)}</p>
        <a href="/results/{os.path.basename(result_path)}" download>Download ZIP</a>
        '''
    
    return f'''
    <h2>Task #{task_id}</h2>
    <p>Status: {status}</p>
    <progress value="50" max="100"></progress>
    <p><small>Auto-refresh in 5 seconds...</small></p>
    <meta http-equiv="refresh" content="5">
    '''

@app.route('/results/<filename>')
def download(filename):
    return send_from_directory(RESULT_FOLDER, filename)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
