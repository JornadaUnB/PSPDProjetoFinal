#!/bin/bash

echo "[1/4] Resetando cluster com kubeadm..."
sudo kubeadm reset -f

echo "[2/4] Parando serviços do Kubernetes..."
sudo systemctl stop kubelet
sudo systemctl stop containerd

echo "[3/4] Reiniciando serviços..."
sudo systemctl restart containerd
sudo systemctl restart kubelet

echo "[4/4] Cluster removido. Este nó agora pode entrar em outro cluster com 'kubeadm join'"
