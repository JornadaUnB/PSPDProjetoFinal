#!/bin/bash

set -e

echo "[1/11] Atualizando pacotes..."
apt update -y

echo "[2/11] Instalando dependências..."
apt install -y apt-transport-https ca-certificates curl gpg lsb-release software-properties-common

echo "[3/11] Instalando containerd..."
apt install -y containerd

echo "[4/11] Configurando containerd com suporte ao Kubernetes..."
mkdir -p /etc/containerd
containerd config default > /etc/containerd/config.toml

# Garante que o plugin CRI está ativado e usa systemd cgroup
sed -i 's/SystemdCgroup = false/SystemdCgroup = true/' /etc/containerd/config.toml
sed -i '/\[plugins."io.containerd.grpc.v1.cri"\]/,/^\[/ s/^#\? *sandbox_image =.*/  sandbox_image = "registry.k8s.io\/pause:3.9"/' /etc/containerd/config.toml

systemctl restart containerd
systemctl enable containerd

echo "[5/11] Carregando módulo br_netfilter e configurando sysctl para Flannel..."

# Carrega o módulo br_netfilter agora
modprobe br_netfilter

# Garante o carregamento automático no boot
echo "br_netfilter" > /etc/modules-load.d/br_netfilter.conf

# Configura parâmetros necessários para kube-flannel
cat <<EOF > /etc/sysctl.d/99-kubernetes-cri.conf
net.bridge.bridge-nf-call-iptables  = 1
net.ipv4.ip_forward                 = 1
net.bridge.bridge-nf-call-ip6tables = 1
EOF

# Aplica as configurações imediatamente
sysctl --system

echo "[6/11] Adicionando repositório do Kubernetes..."
curl -fsSL https://pkgs.k8s.io/core:/stable:/v1.30/deb/Release.key | gpg --dearmor -o /etc/apt/trusted.gpg.d/kubernetes.gpg
echo "deb [signed-by=/etc/apt/trusted.gpg.d/kubernetes.gpg] https://pkgs.k8s.io/core:/stable:/v1.30/deb/ /" > /etc/apt/sources.list.d/kubernetes.list

apt update -y

echo "[7/11] Instalando kubelet, kubeadm e kubectl..."
apt install -y kubelet kubeadm kubectl
apt-mark hold kubelet kubeadm kubectl

echo "[8/11] Desativando swap (requisito do Kubernetes)..."
swapoff -a
sed -i '/ swap / s/^/#/' /etc/fstab

echo "[9/11] Inicializando cluster com kubeadm..."
kubeadm init --pod-network-cidr=10.244.0.0/16 --node-name=$(hostname)

echo "[10/11] Configurando kubectl para o root..."
mkdir -p $HOME/.kube
cp -f /etc/kubernetes/admin.conf $HOME/.kube/config
chown $(id -u):$(id -g) $HOME/.kube/config

echo "[11/11] Instalando Flannel CNI..."
kubectl apply -f https://raw.githubusercontent.com/flannel-io/flannel/master/Documentation/kube-flannel.yml

echo ""
echo "✅ Cluster Kubernetes inicializado com sucesso!"
echo ""
echo "👉 Use este comando para adicionar os workers ao cluster:"
kubeadm token create --print-join-command
