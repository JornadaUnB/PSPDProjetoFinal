import os
import time
from flask import Flask, render_template, request, redirect
from kafka import KafkaProducer
from kafka.errors import NoBrokersAvailable

app = Flask(__name__)

def create_kafka_producer(bootstrap_servers, retries=10, delay=5):
    for i in range(retries):
        try:
            print(f"[Kafka] Tentando conectar... ({i+1}/{retries})")
            return KafkaProducer(bootstrap_servers=bootstrap_servers)
        except NoBrokersAvailable:
            time.sleep(delay)

    print("[Kafka] Falha ao conectar após várias tentativas. Aguardando para debug...")
    time.sleep(3600)
    raise RuntimeError("Kafka não disponível após várias tentativas.")

@app.route('/', methods=['GET', 'POST'])
def index():
    if request.method == 'POST':
        powmin = request.form.get('powmin')
        powmax = request.form.get('powmax')
        engine = request.form.get('engine')

        if powmin and powmax and engine:
            message = f"{powmin},{powmax}"
            topic = "spark-topic" if engine == "spark" else "omp-mpi-topic"

            # Conexão só aqui:
            kafka_bootstrap = os.getenv("KAFKA_BOOTSTRAP_SERVERS", "kafka:9092")
            producer = create_kafka_producer(kafka_bootstrap)

            producer.send(topic, message.encode('utf-8'))
            producer.flush()
            print(f"[Kafka] Enviado: {message} para tópico {topic}")

        return redirect('/')

    return render_template('form.html')
