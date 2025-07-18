# Projeto de Sistemas Paralelos e Distribuídos

## 📝 Descrição do Projeto
Implementação do Jogo da Vida de Conway utilizando:
- Apache Spark para processamento distribuído
- OpenMP/MPI para computação paralela
- Kubernetes para orquestração de containers
- ElasticSearch/Kibana para monitoramento

## 📋 Instruções de execução

### 1. Execute o comando para preparar os nós do cluster:
**No MASTER e WORKERS**
```bash
sudo bash install_local_deb.sh
```

### 2. Substitua `nomeDoNode` pelo nome do seu nó master (use `sudo kubectl get nodes` para ver)
**No MASTER**
```bash
sudo kubectl taint nodes nomeDoNode node-role.kubernetes.io/control-plane-
```

### 3. Use o comando que apareceu no master, no terminal dos workers para faze-los se juntarem-se ao cluster.
**Nos WORKERS**
```bash
sudo kubeadm join 192.199.15.755:6483 --token w4vxtr.vw3gqp6yphaq2tfj --discovery-token-ca-cert-hash sha256:f299f0497f401ea85acaca185b52d2df180fb
```

### 4. Use o comando para implementar a stack basica:
**No MASTER**
```bash
sudo kubectl apply -f stack.yaml
```

## 📋 Instruções de parada

### Use o comando para parar tudo:
**No MASTER**
```bash
sudo kubectl delete -f stack.yaml
```
