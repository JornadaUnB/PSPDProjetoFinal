FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y \
    build-essential \
    openmpi-bin \
    libopenmpi-dev \
    librdkafka-dev \
    libcurl4-openssl-dev \
    libjansson-dev && \
    apt-get clean

WORKDIR /app

COPY . .

RUN mpicc -Wall -fopenmp -o out jogodavidampiomp.c -lrdkafka -lcurl -ljansson

ENV OMP_NUM_THREADS=4

CMD ["mpirun", "-np", "3", "--allow-run-as-root", "./out"]