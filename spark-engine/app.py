import time
from kafka import KafkaConsumer
from kafka.errors import NoBrokersAvailable
from pyspark.sql import SparkSession
import requests
import json


KAFKA_TOPIC = "spark-topic"
KAFKA_SERVER = "kafka:9092"
GROUP_ID = "engine-spark-group"

ELASTIC_HOST = "http://elasticsearch:9200"



def send_metrics_to_elasticsearch(tam, resultado_str, t_init, t_comp, t_fim, t_total):
    url = ELASTIC_HOST+"/spark-metrics/_doc"
    headers = {"Content-Type": "application/json"}
    
    json_data = {
        "tam": tam,
        "resultado": resultado_str,
        "tempo_init_s": t_init,
        "tempo_comp_s": t_comp,
        "tempo_verif_s": t_fim,
        "tempo_total_s": t_total,
        "engine": "Spark"
    }

    try:
        response = requests.post(url, headers=headers, data=json.dumps(json_data))
        if response.status_code == 201:
            print("Métricas enviadas para o Elasticsearch com sucesso.")
        else:
            print(f"Falha ao enviar métricas: {response.status_code} - {response.text}")
    except Exception as e:
        print(f"Erro ao conectar ao Elasticsearch: {e}")


def ind2d(i, j, tam):
    return i * (tam + 2) + j

def wall_time():
    return time.time()

def uma_vida(tabulIn, tabulOut, tam):
    for i in range(1, tam + 1):
        for j in range(1, tam + 1):
            vizviv = (
                tabulIn[ind2d(i - 1, j - 1, tam)] +
                tabulIn[ind2d(i - 1, j, tam)] +
                tabulIn[ind2d(i - 1, j + 1, tam)] +
                tabulIn[ind2d(i, j - 1, tam)] +
                tabulIn[ind2d(i, j + 1, tam)] +
                tabulIn[ind2d(i + 1, j - 1, tam)] +
                tabulIn[ind2d(i + 1, j, tam)] +
                tabulIn[ind2d(i + 1, j + 1, tam)]
            )
            idx = ind2d(i, j, tam)
            if tabulIn[idx] and vizviv < 2:
                tabulOut[idx] = 0
            elif tabulIn[idx] and vizviv > 3:
                tabulOut[idx] = 0
            elif not tabulIn[idx] and vizviv == 3:
                tabulOut[idx] = 1
            else:
                tabulOut[idx] = tabulIn[idx]

def init_tabul(tabulIn, tabulOut, tam):
    for ij in range((tam + 2) * (tam + 2)):
        tabulIn[ij] = 0
        tabulOut[ij] = 0
    tabulIn[ind2d(1, 2, tam)] = 1
    tabulIn[ind2d(2, 3, tam)] = 1
    tabulIn[ind2d(3, 1, tam)] = 1
    tabulIn[ind2d(3, 2, tam)] = 1
    tabulIn[ind2d(3, 3, tam)] = 1

def correto(tabul, tam):
    cnt = sum(tabul)
    return (cnt == 5 and
            tabul[ind2d(tam - 2, tam - 1, tam)] and
            tabul[ind2d(tam - 1, tam, tam)] and
            tabul[ind2d(tam, tam - 2, tam)] and
            tabul[ind2d(tam, tam - 1, tam)] and
            tabul[ind2d(tam, tam, tam)])

def simular(tam):
    t0 = wall_time()
    tabulIn = [0] * ((tam + 2) * (tam + 2))
    tabulOut = [0] * ((tam + 2) * (tam + 2))
    init_tabul(tabulIn, tabulOut, tam)
    t1 = wall_time()

    for _ in range(2 * (tam - 3)):
        uma_vida(tabulIn, tabulOut, tam)
        uma_vida(tabulOut, tabulIn, tam)

    t2 = wall_time()
    resultado = correto(tabulIn, tam)
    t3 = wall_time()

    return (tam, t1 - t0, t2 - t1, t3 - t2, t3 - t0, resultado)

consumer = None
for attempt in range(10):
    try:
        print(f"[Tentativa {attempt+1}] Conectando ao Kafka em {KAFKA_SERVER}...")
        consumer = KafkaConsumer(
            KAFKA_TOPIC,
            bootstrap_servers=KAFKA_SERVER,
            group_id=GROUP_ID,
            auto_offset_reset='earliest'
        )
        print("Conectado com sucesso ao Kafka.")
        break
    except NoBrokersAvailable:
        print("Kafka ainda não está disponível. Aguardando...")
        time.sleep(3)

if consumer is None:
    print("Não foi possível conectar ao Kafka após várias tentativas.")
    exit(1)

try:
    spark = SparkSession.builder \
        .appName("JogoDaVidaParalelo") \
        .master("spark://spark-master:7077") \
        .getOrCreate()
except Exception as e:
    print(f"Erro ao iniciar SparkSession: {e}")
    exit(1)

print("Aguardando mensagens Kafka com POWMIN,POWMAX...")

for msg in consumer:
    texto = msg.value.decode('utf-8').strip()
    print(f"Mensagem recebida: {texto}")

    try:
        POWMIN, POWMAX = map(int, texto.split(","))
    except ValueError:
        print("Formato inválido. Use: POWMIN,POWMAX (ex: 3,10)")
        continue

    powers = list(range(POWMIN, POWMAX + 1))
    tamanhos = [2 ** p for p in powers]

    rdd = spark.sparkContext.parallelize(tamanhos)
    resultados = rdd.map(simular).collect()

    for tam, t_init, t_comp, t_fim, t_total, status in resultados:
        print("Ok, RESULTADO CORRETO" if status else "Nok, RESULTADO ERRADO")
        print(f"tam={tam}; tempos: init={t_init:.7f}, comp={t_comp:.7f}, fim={t_fim:.7f}, tot={t_total:.7f}")


        send_metrics_to_elasticsearch(tam,"CORRETO" if status else "ERRADO",t_init,t_comp,t_fim,t_total)
        try:
            print(f"Resultado enviado ao Elasticsearch: tam={tam}")
        except Exception as e:
            print(f"Erro ao enviar resultado para Elasticsearch: {e}")
