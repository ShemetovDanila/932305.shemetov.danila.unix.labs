from confluent_kafka import Consumer, KafkaException, KafkaError
import json
import time
import os
import random

conf = {
    'bootstrap.servers': os.environ['KAFKA_BROKER'],
    'group.id': 'shop-workers',
    'auto.offset.reset': 'earliest',
    'enable.auto.commit': False,
    'session.timeout.ms': 45000,
    'max.poll.interval.ms': 300000
}

consumer = Consumer(conf)
consumer.subscribe(['internet_store'])

worker_id = os.getenv('HOSTNAME', 'worker-unknown')

def process_account(message):
    print(f"[{worker_id}] Processing account action: {message['action']} for user {message['user_id']}")
    time.sleep(0.05)
    return True

def process_cart(message):
    action = "added" if message['action'] == 'add_to_cart' else "removed"
    print(f"[{worker_id}] Processing cart: product {message['product_id']} {action} for user {message['user_id']}")
    time.sleep(0.1)
    return True

def process_order(message):
    priority = message.get('priority', 'medium')
    delay = {'low': 0.3, 'medium': 0.5, 'high': 0.8}[priority]
    
    print(f"[{worker_id}] Processing order ({priority}): {message['order_id']} for user {message['user_id']}")
    print(f"   Items: {len(message['items'])} items")
    time.sleep(delay)
    return True

def process_message(message):
    msg_type = message.get('type', 'unknown')
    
    handlers = {
        'account': process_account,
        'cart': process_cart,
        'order': process_order
    }
    
    handler = handlers.get(msg_type)
    if not handler:
        print(f"[{worker_id}] Unknown message type: {msg_type}")
        return False
    
    try:
        return handler(message)
    except Exception as e:
        print(f"[{worker_id}] Processing error for {msg_type}: {str(e)}")
        return False

print(f"Worker {worker_id} started. Waiting for messages...")
try:
    while True:
        msg = consumer.poll(timeout=1.0)
        if msg is None:
            continue
            
        if msg.error():
            if msg.error().code() == KafkaError._PARTITION_EOF:
                continue
            raise KafkaException(msg.error())
        
        message = json.loads(msg.value().decode('utf-8'))
        partition = msg.partition()
        offset = msg.offset()
        
        print(f"[{worker_id}] Received message (partition: {partition}, offset: {offset})")
        
        start_time = time.time()
        success = process_message(message)
        duration = time.time() - start_time
        
        if success:
            consumer.commit(msg)
            print(f"[{worker_id}] Successfully processed in {duration:.2f}s (partition {partition})")
        else:
            print(f"[{worker_id}] Message will be reprocessed (partition {partition})")
            
except KeyboardInterrupt:
    print(f"[{worker_id}] Shutting down gracefully")
finally:
    consumer.close()
