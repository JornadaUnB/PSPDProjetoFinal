FROM python:3.12-slim

WORKDIR /app

# Requisitos para Spark
RUN apt-get update && apt-get install -y openjdk-17-jre-headless netcat-openbsd && \
    apt-get clean

    
# Variável de ambiente do Java
ENV JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
ENV PATH="${JAVA_HOME}/bin:${PATH}"

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY app.py .

CMD ["python", "app.py"]
