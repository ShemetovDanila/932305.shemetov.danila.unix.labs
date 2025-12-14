import json, time, random
from kafka import KafkaProducer

p = KafkaProducer(bootstrap_servers='kafka:9092', 
                 value_serializer=lambda v: json.dumps(v).encode('utf-8'))

while True:
    p.send('tasks', {'duration': random.choice([5,10,15,20,25,30])})
    time.sleep(2)
