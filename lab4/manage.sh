#!/bin/bash
set -e

wait_for_kafka() {
    echo "Waiting for Kafka readiness (max 3 minutes)..."
    MAX_ATTEMPTS=18
    ATTEMPT=0
    
    while [ $ATTEMPT -lt $MAX_ATTEMPTS ]; do
        if sudo docker compose exec kafka kafka-broker-api-versions --bootstrap-server localhost:9092 >/dev/null 2>&1; then
            echo "Kafka is ready!"
            return 0
        fi
        ATTEMPT=$((ATTEMPT+1))
        echo "Kafka not ready yet... Attempt $ATTEMPT/$MAX_ATTEMPTS"
        sleep 10
    done
    
    echo "Kafka failed to start within timeout"
    exit 1
}

manage_topic() {
    local partitions=$1
    
    if sudo docker compose exec kafka kafka-topics \
        --bootstrap-server localhost:9092 \
        --list | grep -q "internet_store"; then
        
        echo "Increasing partitions to $partitions..."
        sudo docker compose exec kafka kafka-topics \
          --bootstrap-server localhost:9092 \
          --alter \
          --topic internet_store \
          --partitions $partitions
    else
        echo "Creating topic with $partitions partitions..."
        sudo docker compose exec kafka kafka-topics \
          --bootstrap-server localhost:9092 \
          --create \
          --topic internet_store \
          --partitions $partitions \
          --replication-factor 1
    fi
}

scale_workers() {
    local workers=$1
    echo "Scaling workers to $workers instances..."
    sudo docker compose up -d --scale worker=$workers worker
}

case "$1" in
    start)
        echo "Starting system with 4 partitions and 12 workers..."
        
        sudo docker compose down -v
        
        sudo docker compose up -d zookeeper
        sleep 30
        
        sudo docker compose up -d kafka
        wait_for_kafka
        
        manage_topic 4
        sudo docker compose up -d --build producer
        scale_workers 12
        
        echo "System started successfully! For monitoring:"
        echo "sudo docker compose logs -f worker"
        ;;
        
    scale)
        if [ $# -lt 3 ]; then
            echo "Usage: $0 scale <partitions> <workers>"
            exit 1
        fi
        
        local partitions=$2
        local workers=$3
        
        echo "Dynamic scaling: $partitions partitions, $workers workers"
        
        if ! sudo docker compose ps kafka | grep -q "Up"; then
            echo "Kafka is not running. First execute: $0 start"
            exit 1
        fi
        
        wait_for_kafka
        
        manage_topic $partitions
        scale_workers $workers
        
        echo "Scaling completed!"
        echo "Current partition distribution:"
        sudo docker compose exec kafka kafka-consumer-groups \
          --bootstrap-server localhost:9092 \
          --group shop-workers \
          --describe
        ;;
        
    status)
        echo "System status:"
        sudo docker compose ps
        echo -e "\nPartition distribution:"
        sudo docker compose exec kafka kafka-consumer-groups \
          --bootstrap-server localhost:9092 \
          --group shop-workers \
          --describe 2>/dev/null || echo "Consumer group not yet created"
        ;;
        
    *)
        echo "Usage:"
        echo "  $0 start           - Start system (4 partitions, 12 workers)"
        echo "  $0 scale <n> <m>   - Scale: n partitions, m workers"
        echo "  $0 status          - Check system status"
        exit 1
        ;;
esac
