FROM openjdk:17-slim

# 🛠 Instala ferramentas essenciais
RUN apt update
RUN apt install -y bash curl tar coreutils git unzip

# 📦 Instala sbt
RUN curl -sL https://github.com/sbt/sbt/releases/download/v1.9.4/sbt-1.9.4.tgz | tar -xz -C /opt \
    && ln -s /opt/sbt/bin/sbt /usr/bin/sbt

# ⚡ Instala Apache Spark
RUN curl -sL https://archive.apache.org/dist/spark/spark-4.0.0/spark-4.0.0-bin-hadoop3.tgz | tar -xz -C /opt
ENV SPARK_HOME=/opt/spark-4.0.0-bin-hadoop3
ENV PATH="${SPARK_HOME}/bin:$PATH"

# 📂 Cria diretório de trabalho
WORKDIR /app
COPY . .

# 🧠 Compila o projeto Scala
RUN cd spark-engine && sbt clean assembly

# 🔁 Modo contínuo: inicia Spark e mantém escuta ativa
CMD ["spark-submit", "--class", "GameOfLifeSpark", "--master", "local[*]", "spark-engine/target/scala-2.13/GameOfLifeSpark-assembly-0.1.jar"]
