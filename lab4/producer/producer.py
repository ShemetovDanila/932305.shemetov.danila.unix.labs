import time
import random
import json
import uuid
from confluent_kafka import Producer

conf = {'bootstrap.servers': 'kafka:9092'}
producer = Producer(conf)

users = [f"user_{i}" for i in range(1, 101)]
products = [f"product_{i}" for i in range(101, 201)]

def delivery_report(err, msg):
    if err:
        print(f'[Producer] Message delivery failed: {err}')
    else:
        print(f'[Producer] Delivered to partition {msg.partition()}: {msg.value().decode()[:50]}...')

print("Producer started. Generating events...")
while True:
    user_id = random.choice(users)
    actions = [
        ('login', 0.1),
        ('logout', 0.1),
        ('add_to_cart', 0.3),
        ('remove_from_cart', 0.2),
        ('place_order', 0.3)
    ]
    
    action = random.choices([a[0] for a in actions], weights=[a[1] for a in actions])[0]
    
    if action in ['login', 'logout']:
        msg = {
            'type': 'account',
            'action': action,
            'user_id': user_id,
            'timestamp': time.time()
        }
        key = f"ACCOUNT::{user_id}"
    
    elif action in ['add_to_cart', 'remove_from_cart']:
        product_id = random.choice(products)
        quantity = random.randint(1, 5)
        msg = {
            'type': 'cart',
            'action': action,
            'user_id': user_id,
            'product_id': product_id,
            'quantity': quantity,
            'timestamp': time.time()
        }
        key = f"CART::{user_id}"
    
    elif action == 'place_order':
        order_id = f"order_{uuid.uuid4().hex[:8]}"
        items_count = random.randint(1, 10)
        items = [
            {
                'product_id': random.choice(products),
                'quantity': random.randint(1, 3)
            } for _ in range(items_count)
        ]
        msg = {
            'type': 'order',
            'action': action,
            'user_id': user_id,
            'order_id': order_id,
            'items': items,
            'timestamp': time.time(),
            'priority': random.choice(['low', 'medium', 'high'])
        }
        key = f"ORDER::{order_id}"

    producer.produce(
        topic='internet_store',
        key=key.encode('utf-8'),
        value=json.dumps(msg),
        callback=delivery_report
    )
    
    producer.poll(0)
    delay = 0.1 if action == 'place_order' else random.uniform(0.3, 1.0)
    time.sleep(delay)

producer.flush()
