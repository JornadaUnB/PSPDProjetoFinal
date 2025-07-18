#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <mpi.h>
#include <omp.h>

#include <librdkafka/rdkafka.h>
#include <curl/curl.h>
#include <jansson.h>

#define ind2d(i, j) (i) * (tam + 2) + j
#define POWMIN 3
#define POWMAX 10

double wall_time(void)
{
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);
    return (tv.tv_sec + tv.tv_usec / 1000000.0);
}

void send_metrics_to_elasticsearch(const char *json_data) {
  CURL *curl;
  CURLcode res;

  curl = curl_easy_init();
  if (curl) {
      // TO-DO: Substitua pela URL do seu Elasticsearch
      curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:9200/jogo_da_vida_metrics/_doc");
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);

      struct curl_slist *headers = NULL;
      headers = curl_slist_append(headers, "Content-Type: application/json");
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

      res = curl_easy_perform(curl);
      if (res != CURLE_OK) {
          fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
      }

      curl_easy_cleanup(curl);
      curl_slist_free_all(headers);
  }
}

void UmaVida(int *tabulIn, int *tabulOut, int tam, int start_row, int end_row)
{
    int i, j, vizviv;
    #pragma omp parallel for private(j, vizviv) schedule(static)
    for (i = start_row; i <= end_row; i++){
        for (j = 1; j <= tam; j++){
            vizviv = tabulIn[ind2d(i - 1, j - 1)] + tabulIn[ind2d(i - 1, j)] +
                     tabulIn[ind2d(i - 1, j + 1)] + tabulIn[ind2d(i, j - 1)] +
                     tabulIn[ind2d(i, j + 1)] + tabulIn[ind2d(i + 1, j - 1)] +
                     tabulIn[ind2d(i + 1, j)] + tabulIn[ind2d(i + 1, j + 1)];
            if (tabulIn[ind2d(i, j)] && vizviv < 2)
                tabulOut[ind2d(i, j)] = 0;
            else if (tabulIn[ind2d(i, j)] && vizviv > 3)
                tabulOut[ind2d(i, j)] = 0;
            else if (!tabulIn[ind2d(i, j)] && vizviv == 3)
                tabulOut[ind2d(i, j)] = 1;
            else
                tabulOut[ind2d(i, j)] = tabulIn[ind2d(i, j)];
        }
    }
}

void InitTabul(int *tabulIn, int *tabulOut, int tam)
{
    int ij;
    for (ij = 0; ij < (tam + 2) * (tam + 2); ij++) {
        tabulIn[ij] = 0;
        tabulOut[ij] = 0;
    }
    tabulIn[ind2d(1, 2)] = 1;
    tabulIn[ind2d(2, 3)] = 1;
    tabulIn[ind2d(3, 1)] = 1;
    tabulIn[ind2d(3, 2)] = 1;
    tabulIn[ind2d(3, 3)] = 1;
}

int Correto(int *tabul, int tam)
{
    int ij, cnt;
    cnt = 0;
    for (ij = 0; ij < (tam + 2) * (tam + 2); ij++)
        cnt = cnt + tabul[ij];
    return (cnt == 5 && tabul[ind2d(tam - 2, tam - 1)] &&
            tabul[ind2d(tam - 1, tam)] && tabul[ind2d(tam, tam - 2)] &&
            tabul[ind2d(tam, tam - 1)] && tabul[ind2d(tam, tam)]);
}

int main(int argc, char *argv[]) {
  int rank, size;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  // ----------------------------------------------------------------
  // LÓGICA DO PROCESSO MESTRE (COORDENADOR, RANK 0)
  // ----------------------------------------------------------------
  if (rank == 0) {
      rd_kafka_t *rk;          // Handle do consumidor Kafka
      rd_kafka_conf_t *conf;   // Objeto de configuração
      char errstr[512];        // String para mensagens de erro

      // 1. CONFIGURAR O CONSUMIDOR KAFKA
      conf = rd_kafka_conf_new();

      // ATENÇÃO: Substitua pelo endereço do seu broker Kafka (host:porta)
      if (rd_kafka_conf_set(conf, "bootstrap.servers", "localhost:29092", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
          fprintf(stderr, "%% Erro de configuração Kafka: %s\n", errstr);
          MPI_Abort(MPI_COMM_WORLD, 1);
      }
      // Define um ID de grupo para o consumidor
      if (rd_kafka_conf_set(conf, "group.id", "engine-mpi-omp-group", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
          fprintf(stderr, "%% Erro de configuração Kafka: %s\n", errstr);
          MPI_Abort(MPI_COMM_WORLD, 1);
      }

      // Cria a instância do consumidor
      rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
      if (!rk) {
          fprintf(stderr, "%% Falha ao criar consumidor: %s\n", errstr);
          MPI_Abort(MPI_COMM_WORLD, 1);
      }
      
      rd_kafka_poll_set_consumer(rk);

      // 2. INSCREVER-SE NO TÓPICO
      rd_kafka_topic_partition_list_t *topics = rd_kafka_topic_partition_list_new(1);
      // TO-DO: Use o nome do tópico definido no seu produtor
      rd_kafka_topic_partition_list_add(topics, "jogo-da-vida-requests", -1); // -1 para qualquer partição
      rd_kafka_subscribe(rk, topics);
      rd_kafka_topic_partition_list_destroy(topics);

      printf("[Rank 0] Serviço iniciado. Aguardando por tarefas do Kafka...\n");

      // 3. LOOP INFINITO PARA CONSUMIR TAREFAS
      while (1) {
          // Busca por uma nova mensagem, com timeout de 1000ms
          rd_kafka_message_t *rkm = rd_kafka_consumer_poll(rk, 1000);
          
          if (!rkm) continue; // Nenhuma mensagem, volta a esperar

          if (rkm->err) {
              // Ignora erros de timeout, mas loga outros erros
              if (rkm->err != RD_KAFKA_RESP_ERR__PARTITION_EOF) {
                  fprintf(stderr, "%% Erro de consumo: %s\n", rd_kafka_message_errstr(rkm));
              }
              rd_kafka_message_destroy(rkm);
              continue;
          }

          // MENSAGEM RECEBIDA COM SUCESSO!
          json_error_t error;
          json_t *root = json_loads((const char *)rkm->payload, 0, &error);
          rd_kafka_message_destroy(rkm); // Libera a mensagem o mais rápido possível

          if (!root) {
              fprintf(stderr, "[Rank 0] Erro ao parsear JSON da tarefa: %s\n", error.text);
              continue;
          }
          
          // Assume que a mensagem JSON tem uma chave como "pow_max"
          json_t* pow_json = json_object_get(root, "pow_max");
          if (!json_is_integer(pow_json)){
              fprintf(stderr, "[Rank 0] Chave 'pow_max' não encontrada ou não é um inteiro.\n");
              json_decref(root);
              continue;
          }
          int pow = json_integer_value(pow_json);
          json_decref(root);
          
          int tam = 1 << pow;
          printf("\n[Rank 0] Nova tarefa recebida. Processando tabuleiro de tamanho %d (pow=%d)\n", tam, pow);

          // 4. DISTRIBUIR TAREFA E DADOS PARA OS WORKERS
          MPI_Bcast(&tam, 1, MPI_INT, 0, MPI_COMM_WORLD);
          
          double t0, t1, t2, t3;
          int *tabulIn = (int *)malloc((tam + 2) * (tam + 2) * sizeof(int));
          int *tabulOut = (int *)malloc((tam + 2) * (tam + 2) * sizeof(int));
          
          t0 = wall_time();
          InitTabul(tabulIn, tabulOut, tam);
          t1 = wall_time();

          MPI_Bcast(tabulIn, (tam + 2) * (tam + 2), MPI_INT, 0, MPI_COMM_WORLD);

          // 5. PROCESSAMENTO (Rank 0 também participa)
          int effective_size = (size > tam && tam > 0) ? tam : size;
          if (effective_size == 0) effective_size = 1;
          int rows_per_process_base = tam / effective_size;
          int remainder = tam % effective_size;
          int start_row = 1;
          int end_row = start_row + rows_per_process_base + (rank < remainder ? 1 : 0) - 1;

          for (int i = 0; i < 2 * (tam - 3); i++) {
              if (rank < effective_size - 1) {
                   MPI_Sendrecv(&tabulIn[ind2d(end_row, 0)], tam + 2, MPI_INT, rank + 1, 0,
                                &tabulIn[ind2d(end_row + 1, 0)], tam + 2, MPI_INT, rank + 1, 0,
                                MPI_COMM_WORLD, MPI_STATUS_IGNORE);
              }
              UmaVida(tabulIn, tabulOut, tam, start_row, end_row);
              int *temp = tabulIn; tabulIn = tabulOut; tabulOut = temp;
          }

          // 6. RECEBER RESULTADOS DOS WORKERS
          for (int p = 1; p < effective_size; p++) {
              int p_rows_base = tam / effective_size;
              int p_rem = tam % effective_size;
              int p_start = p * p_rows_base + (p < p_rem ? p : p_rem) + 1;
              int p_end = p_start + p_rows_base + (p < p_rem ? 1 : 0) - 1;
              MPI_Recv(&tabulIn[ind2d(p_start, 0)], (p_end - p_start + 1) * (tam + 2), MPI_INT, p, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
          }
          
          t2 = wall_time();
          const char *resultado_str = Correto(tabulIn, tam) ? "CORRETO" : "ERRADO";
          t3 = wall_time();

          // 7. ENVIAR MÉTRICAS PARA O ELASTICSEARCH
          printf("[Rank 0] Processamento concluído. Resultado: %s\n", resultado_str);
          
          json_t *metrics_json = json_pack("{s:i, s:i, s:s, s:f, s:f, s:f, s:f, s:s}",
                                           "tam", tam,
                                           "mpi_size", size,
                                           "resultado", resultado_str,
                                           "tempo_init_s", t1 - t0,
                                           "tempo_comp_s", t2 - t1,
                                           "tempo_verif_s", t3 - t2,
                                           "tempo_total_s", t3 - t0,
                                           "engine", "MPI_OpenMP");

          char *json_dump = json_dumps(metrics_json, JSON_INDENT(2));
          send_metrics_to_elasticsearch(json_dump);
          
          // Limpeza da tarefa atual
          free(json_dump);
          json_decref(metrics_json);
          free(tabulIn);
          free(tabulOut);
      }

      // Limpeza final do Kafka
      rd_kafka_consumer_close(rk);
      rd_kafka_destroy(rk);

  } 
  // ----------------------------------------------------------------
  // LÓGICA DOS PROCESSOS WORKERS (RANK > 0)
  // ----------------------------------------------------------------
  else {
      while (1) {
          int tam;
          // 1. Aguarda o rank 0 enviar um novo tamanho de tabuleiro
          MPI_Bcast(&tam, 1, MPI_INT, 0, MPI_COMM_WORLD);
          
          int effective_size = (size > tam && tam > 0) ? tam : size;
          if (rank >= effective_size) continue; // Worker não é necessário para este job

          // 2. Aloca memória e recebe o tabuleiro inicial
          int *tabulIn = (int *)malloc((tam + 2) * (tam + 2) * sizeof(int));
          int *tabulOut = (int *)malloc((tam + 2) * (tam + 2) * sizeof(int));
          MPI_Bcast(tabulIn, (tam + 2) * (tam + 2), MPI_INT, 0, MPI_COMM_WORLD);

          // 3. Calcula sua porção de trabalho
          int rows_per_process_base = tam / effective_size;
          int remainder = tam % effective_size;
          int start_row = rank * rows_per_process_base + (rank < remainder ? rank : remainder) + 1;
          int end_row = start_row + rows_per_process_base + (rank < remainder ? 1 : 0) - 1;
          
          // 4. Loop de processamento com troca de bordas
          for (int i = 0; i < 2 * (tam - 3); i++) {
              MPI_Sendrecv(&tabulIn[ind2d(start_row, 0)], tam + 2, MPI_INT, rank - 1, 0,
                           &tabulIn[ind2d(start_row - 1, 0)], tam + 2, MPI_INT, rank - 1, 0,
                           MPI_COMM_WORLD, MPI_STATUS_IGNORE);
              if (rank < effective_size - 1) {
                  MPI_Sendrecv(&tabulIn[ind2d(end_row, 0)], tam + 2, MPI_INT, rank + 1, 0,
                               &tabulIn[ind2d(end_row + 1, 0)], tam + 2, MPI_INT, rank + 1, 0,
                               MPI_COMM_WORLD, MPI_STATUS_IGNORE);
              }
              UmaVida(tabulIn, tabulOut, tam, start_row, end_row);
              int *temp = tabulIn; tabulIn = tabulOut; tabulOut = temp;
          }

          // 5. Envia a parte final do tabuleiro de volta para o rank 0
          MPI_Send(&tabulIn[ind2d(start_row, 0)], (end_row - start_row + 1) * (tam + 2), MPI_INT, 0, 0, MPI_COMM_WORLD);
          
          // 6. Limpeza da tarefa atual
          free(tabulIn);
          free(tabulOut);
      }
  }

  MPI_Finalize();
  return 0;
}
