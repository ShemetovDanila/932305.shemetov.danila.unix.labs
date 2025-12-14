import json, time
from kafka import KafkaConsumer

c = KafkaConsumer('tasks', bootstrap_servers='kafka:9092', 
                 group_id='group', enable_auto_commit=False)

for msg in c:
    d = msg.value['duration']
    print(f"Working {d} sec on partition {msg.partition}")
    time.sleep(d)
    c.commit()
