#!/bin/bash

echo "[1/6] Resetando cluster com kubeadm..."
sudo kubeadm reset -f

echo "[2/6] Parando serviços do Kubernetes..."
sudo systemctl stop kubelet
sudo systemctl stop containerd

echo "[3/6] Removendo arquivos residuais do Kubernetes..."
sudo rm -rf /etc/cni/net.d
sudo rm -rf /etc/kubernetes
sudo rm -rf /var/lib/etcd
sudo rm -rf /var/lib/kubelet
sudo rm -rf /etc/containerd
sudo rm -rf ~/.kube

echo "[4/6] (Opcional) Limpando configuração de rede do CNI..."
sudo rm -rf /opt/cni/bin

echo "[5/6] Reiniciando serviços..."
sudo systemctl restart containerd
sudo systemctl restart kubelet

echo "[6/6] Cluster removido. Este nó agora pode entrar em outro cluster com 'kubeadm join'"
