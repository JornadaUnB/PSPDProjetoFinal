FROM openjdk:17-alpine

# Instala ferramentas básicas
RUN apk add --no-cache bash curl tar coreutils git unzip

# Instala sbt manualmente
RUN curl -sL https://github.com/sbt/sbt/releases/download/v1.9.4/sbt-1.9.4.tgz | tar -xz -C /opt \
    && ln -s /opt/sbt/bin/sbt /usr/bin/sbt

# Instala Spark manualmente
RUN curl -sL https://archive.apache.org/dist/spark/spark-4.0.0/spark-4.0.0-bin-hadoop3.tgz | tar -xz -C /opt
ENV SPARK_HOME=/opt/spark-4.0.0-bin-hadoop3
ENV PATH="${SPARK_HOME}/bin:$PATH"

WORKDIR /app

COPY . .

RUN cd spark-engine && sbt clean assembly

CMD ["spark-submit", "--class", "GameOfLifeSpark", "--master", "local[*]", "spark-engine/target/scala-2.13/GameOfLifeSpark-assembly-0.1.jar", "5", "8"]
