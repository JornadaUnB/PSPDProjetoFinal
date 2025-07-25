#!/bin/bash

echo "[1/3] Inicializando cluster com kubeadm..."
kubeadm init --pod-network-cidr=10.244.0.0/16 --node-name=$(hostname)

echo "[2/3] Configurando kubectl para o root..."
mkdir -p $HOME/.kube
cp -f /etc/kubernetes/admin.conf $HOME/.kube/config
chown $(id -u):$(id -g) $HOME/.kube/config

echo "[3/3] Instalando Flannel CNI..."
kubectl apply -f https://raw.githubusercontent.com/flannel-io/flannel/master/Documentation/kube-flannel.yml

echo ""
echo "✅ Cluster Kubernetes inicializado com sucesso!"
echo ""
echo "👉 Use este comando para adicionar os workers ao cluster:"
kubeadm token create --print-join-command
