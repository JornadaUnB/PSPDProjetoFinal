FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y \
    build-essential \
    openmpi-bin \
    openssh-server \
    libopenmpi-dev \
    librdkafka-dev \
    libcurl4-openssl-dev \
    libjansson-dev \
    ca-certificates && \
    apt-get clean

RUN mkdir /var/run/sshd

WORKDIR /app

COPY . .

RUN mpicc -Wall -fopenmp -o out jogodavidampiomp.c -lrdkafka -lcurl -ljansson

RUN useradd -m mpiuser && \
    echo "mpiuser ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
    mkdir -p /home/mpiuser/.ssh && \
    chown -R mpiuser:mpiuser /home/mpiuser/.ssh

ENV OMP_NUM_THREADS=4

CMD ["/usr/sbin/sshd", "-D"]
