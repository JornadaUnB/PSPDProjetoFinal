FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y build-essential libopenmpi-dev openmpi-bin && \
    apt-get clean

WORKDIR /app

COPY . .

RUN mpicc -fopenmp -o out omp-mpi-jogo-da-vida.c

#Configuração mais rápida para rodar o código
ENV OMP_NUM_THREADS=4
CMD ["mpirun", "-np", "3", "./out"]