#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <mpi.h>
#include <omp.h>

// Bibliotecas para Kafka, requisições HTTP e manipulação de JSON
#include <librdkafka/rdkafka.h>
#include <curl/curl.h>
#include <jansson.h>

#define ind2d(i, j, tam) (i) * (tam + 2) + j

// Função para medir o tempo
double wall_time(void)
{
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);
    return (tv.tv_sec + tv.tv_usec / 1000000.0);
}

// Função para enviar os dados para o Elasticsearch
void send_metrics_to_elasticsearch(const char *json_data) {
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl) {
        // A URL agora aponta para o serviço 'elasticsearch' do docker-compose
        curl_easy_setopt(curl, CURLOPT_URL, "http://elasticsearch:9200/jogo_da_vida_metrics/_doc");
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

// Lógica de uma geração do Jogo da Vida
void UmaVida(int *tabulIn, int *tabulOut, int tam, int start_row, int end_row)
{
    int i, j, vizviv;
    #pragma omp parallel for private(j, vizviv) schedule(static)
    for (i = start_row; i <= end_row; i++){
        for (j = 1; j <= tam; j++){
            vizviv = tabulIn[ind2d(i - 1, j - 1, tam)] + tabulIn[ind2d(i - 1, j, tam)] +
                     tabulIn[ind2d(i - 1, j + 1, tam)] + tabulIn[ind2d(i, j - 1, tam)] +
                     tabulIn[ind2d(i, j + 1, tam)] + tabulIn[ind2d(i + 1, j - 1, tam)] +
                     tabulIn[ind2d(i + 1, j, tam)] + tabulIn[ind2d(i + 1, j + 1, tam)];
            if (tabulIn[ind2d(i, j, tam)] && vizviv < 2)
                tabulOut[ind2d(i, j, tam)] = 0;
            else if (tabulIn[ind2d(i, j, tam)] && vizviv > 3)
                tabulOut[ind2d(i, j, tam)] = 0;
            else if (!tabulIn[ind2d(i, j, tam)] && vizviv == 3)
                tabulOut[ind2d(i, j, tam)] = 1;
            else
                tabulOut[ind2d(i, j, tam)] = tabulIn[ind2d(i, j, tam)];
        }
    }
}

// Inicializa o tabuleiro com o padrão "Glider"
void InitTabul(int *tabulIn, int *tabulOut, int tam)
{
    int ij;
    for (ij = 0; ij < (tam + 2) * (tam + 2); ij++) {
        tabulIn[ij] = 0;
        tabulOut[ij] = 0;
    }
    tabulIn[ind2d(1, 2, tam)] = 1;
    tabulIn[ind2d(2, 3, tam)] = 1;
    tabulIn[ind2d(3, 1, tam)] = 1;
    tabulIn[ind2d(3, 2, tam)] = 1;
    tabulIn[ind2d(3, 3, tam)] = 1;
}

// Verifica se o resultado final está correto
int Correto(int *tabul, int tam)
{
    int ij, cnt;
    cnt = 0;
    for (ij = 0; ij < (tam + 2) * (tam + 2); ij++)
        cnt = cnt + tabul[ij];
    return (cnt == 5 && tabul[ind2d(tam - 2, tam - 1, tam)] &&
            tabul[ind2d(tam - 1, tam, tam)] && tabul[ind2d(tam, tam - 2, tam)] &&
            tabul[ind2d(tam, tam - 1, tam)] && tabul[ind2d(tam, tam, tam)]);
}

// Função principal
// Função main COMPLETA E CORRIGIDA
int main(int argc, char *argv[]) {
    int rank, size;
    int pow_min = 0, pow_max = 0; // Valores iniciais

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Bloco de código do processo mestre (rank 0)
    if (rank == 0) {
        rd_kafka_t *rk;
        rd_kafka_conf_t *conf;
        char errstr[512];

        // Configuração do consumidor Kafka
        conf = rd_kafka_conf_new();
        rd_kafka_conf_set(conf, "bootstrap.servers", "kafka:9092", NULL, 0);
        rd_kafka_conf_set(conf, "group.id", "engine-mpi-omp-group", NULL, 0);
        // Garante que o consumidor leia desde o início se for um novo grupo
        rd_kafka_conf_set(conf, "auto.offset.reset", "earliest", NULL, 0);

        rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
        rd_kafka_poll_set_consumer(rk);
        rd_kafka_topic_partition_list_t *topics = rd_kafka_topic_partition_list_new(1);
        rd_kafka_topic_partition_list_add(topics, "jogo-da-vida-requests", -1);
        rd_kafka_subscribe(rk, topics);
        rd_kafka_topic_partition_list_destroy(topics);

        printf("[Rank 0] Serviço iniciado. Aguardando por tarefas do Kafka...\n");

        // Loop de serviço: espera por novas tarefas
        while (1) {
            rd_kafka_message_t *rkm = rd_kafka_consumer_poll(rk, 1000);
            if (!rkm) {
                continue; // Nenhuma mensagem, continua esperando
            }

            if (rkm->err) {
                if (rkm->err != RD_KAFKA_RESP_ERR__PARTITION_EOF)
                    fprintf(stderr, "%% Erro de consumo: %s\n", rd_kafka_message_errstr(rkm));
                rd_kafka_message_destroy(rkm);
                continue;
            }

            // Mensagem recebida. Parse do JSON.
            json_error_t error;
            json_t *root = json_loads((const char *)rkm->payload, 0, &error);
            
            if (root) {
                json_unpack(root, "{s:i, s:i}", "pow_min", &pow_min, "pow_max", &pow_max);
                json_decref(root);
                printf("\n[Rank 0] Nova tarefa recebida. Processando intervalo de POW: %d a %d\n", pow_min, pow_max);
            } else {
                fprintf(stderr, "[Rank 0] Erro ao parsear JSON. Payload: %s\n", (const char*)rkm->payload);
                rd_kafka_message_destroy(rkm);
                continue;
            }
            rd_kafka_message_destroy(rkm);

            // Transmite o intervalo para os workers
            MPI_Bcast(&pow_min, 1, MPI_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(&pow_max, 1, MPI_INT, 0, MPI_COMM_WORLD);
            
            // =========================================================================
            // INÍCIO DA LÓGICA DE PROCESSAMENTO (MOVIDA PARA DENTRO DO LOOP)
            // =========================================================================
            for (int pow = pow_min; pow <= pow_max; pow++) {
                int tam = 1 << pow;
                double t0 = 0, t1 = 0, t2 = 0, t3 = 0;
                int *tabulIn = (int *)malloc((tam + 2) * (tam + 2) * sizeof(int));
                int *tabulOut = (int *)malloc((tam + 2) * (tam + 2) * sizeof(int));

                // Apenas o rank 0 inicializa o tabuleiro e mede o tempo de init
                t0 = wall_time();
                InitTabul(tabulIn, tabulOut, tam);
                t1 = wall_time();

                // Envia o tabuleiro inicial para todos os processos
                MPI_Bcast(tabulIn, (tam + 2) * (tam + 2), MPI_INT, 0, MPI_COMM_WORLD);

                int effective_size = (size > tam && tam > 0) ? tam : size;

                if (rank >= effective_size) {
                    free(tabulIn);
                    free(tabulOut);
                    continue; // Processos extras não participam se tam < size
                }

                // Divisão de trabalho
                int rows_per_process_base = tam / effective_size;
                int remainder = tam % effective_size;
                int start_row = rank * rows_per_process_base + (rank < remainder ? rank : remainder) + 1;
                int end_row = start_row + rows_per_process_base + (rank < remainder ? 1 : 0) - 1;

                // Loop principal de gerações
                for (int i = 0; i < 4 * (tam - 3); i++) {
                    if (rank > 0) {
                        MPI_Sendrecv(&tabulIn[ind2d(start_row, 0, tam)], tam + 2, MPI_INT, rank - 1, 0,
                                        &tabulIn[ind2d(start_row - 1, 0, tam)], tam + 2, MPI_INT, rank - 1, 0,
                                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    }
                    if (rank < effective_size - 1) {
                        MPI_Sendrecv(&tabulIn[ind2d(end_row, 0, tam)], tam + 2, MPI_INT, rank + 1, 0,
                                        &tabulIn[ind2d(end_row + 1, 0, tam)], tam + 2, MPI_INT, rank + 1, 0,
                                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    }
                    UmaVida(tabulIn, tabulOut, tam, start_row, end_row);
                    int *temp = tabulIn; tabulIn = tabulOut; tabulOut = temp;
                }

                // Coleta de resultados (workers enviam para o mestre)
                for (int p = 1; p < effective_size; p++) {
                    int p_rows_base = tam / effective_size;
                    int p_rem = tam % effective_size;
                    int p_start = p * p_rows_base + (p < p_rem ? p : p_rem) + 1;
                    int p_end = p_start + p_rows_base + (p < p_rem ? 1 : 0) - 1;
                    MPI_Recv(&tabulIn[ind2d(p_start, 0, tam)], (p_end - p_start + 1) * (tam + 2), MPI_INT, p, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                }

                // Rank 0 finaliza, verifica e envia métricas
                t2 = wall_time();
                const char* resultado_str = Correto(tabulIn, tam) ? "CORRETO" : "ERRADO";
                t3 = wall_time();
                
                json_t *metrics_json = json_pack("{s:i, s:i, s:s, s:f, s:f, s:f, s:f, s:s}",
                                                 "tam", tam, "mpi_size", size, "resultado", resultado_str,
                                                 "tempo_init_s", t1 - t0, "tempo_comp_s", t2 - t1,
                                                 "tempo_verif_s", t3 - t2, "tempo_total_s", t3 - t0,
                                                 "engine", "MPI_OpenMP");

                char *json_dump = json_dumps(metrics_json, JSON_INDENT(2));
                send_metrics_to_elasticsearch(json_dump);
                
                printf("tam=%d; resultado=%s; tempos: init=%7.7f, comp=%7.7f, fim=%7.7f, tot=%7.7f (enviado para ES)\n",
                       tam, resultado_str, t1 - t0, t2 - t1, t3 - t2, t3 - t0);
                
                free(json_dump);
                json_decref(metrics_json);

                free(tabulIn);
                free(tabulOut);
            }
            printf("\n[Rank 0] Todas as tarefas do intervalo concluídas. Aguardando próximo comando do Kafka...\n");
            // =========================================================================
            // FIM DA LÓGICA DE PROCESSAMENTO
            // =========================================================================
        }
    }
    // Bloco de código dos processos workers (rank > 0)
    else {
        while (1) {
            // Workers recebem o intervalo do rank 0
            MPI_Bcast(&pow_min, 1, MPI_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(&pow_max, 1, MPI_INT, 0, MPI_COMM_WORLD);
            
            // =========================================================================
            // INÍCIO DA LÓGICA DE PROCESSAMENTO (IDÊNTICA, MOVIDA PARA DENTRO DO LOOP)
            // =========================================================================
            for (int pow = pow_min; pow <= pow_max; pow++) {
                int tam = 1 << pow;
                int *tabulIn = (int *)malloc((tam + 2) * (tam + 2) * sizeof(int));
                int *tabulOut = (int *)malloc((tam + 2) * (tam + 2) * sizeof(int));

                // Recebe o tabuleiro inicial do processo mestre
                MPI_Bcast(tabulIn, (tam + 2) * (tam + 2), MPI_INT, 0, MPI_COMM_WORLD);

                int effective_size = (size > tam && tam > 0) ? tam : size;

                if (rank >= effective_size) {
                    free(tabulIn);
                    free(tabulOut);
                    continue; // Processos extras não participam
                }

                // Divisão de trabalho
                int rows_per_process_base = tam / effective_size;
                int remainder = tam % effective_size;
                int start_row = rank * rows_per_process_base + (rank < remainder ? rank : remainder) + 1;
                int end_row = start_row + rows_per_process_base + (rank < remainder ? 1 : 0) - 1;

                // Loop principal de gerações
                for (int i = 0; i < 4 * (tam - 3); i++) {
                    if (rank > 0) {
                        MPI_Sendrecv(&tabulIn[ind2d(start_row, 0, tam)], tam + 2, MPI_INT, rank - 1, 0,
                                        &tabulIn[ind2d(start_row - 1, 0, tam)], tam + 2, MPI_INT, rank - 1, 0,
                                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    }
                    if (rank < effective_size - 1) {
                        MPI_Sendrecv(&tabulIn[ind2d(end_row, 0, tam)], tam + 2, MPI_INT, rank + 1, 0,
                                        &tabulIn[ind2d(end_row + 1, 0, tam)], tam + 2, MPI_INT, rank + 1, 0,
                                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    }
                    UmaVida(tabulIn, tabulOut, tam, start_row, end_row);
                    int *temp = tabulIn; tabulIn = tabulOut; tabulOut = temp;
                }

                // Envia a parte processada do tabuleiro de volta para o mestre
                MPI_Send(&tabulIn[ind2d(start_row, 0, tam)], (end_row - start_row + 1) * (tam + 2), MPI_INT, 0, 0, MPI_COMM_WORLD);

                free(tabulIn);
                free(tabulOut);
            }
            // =========================================================================
            // FIM DA LÓGICA DE PROCESSAMENTO
            // =========================================================================
        }
    }

    MPI_Finalize();
    return 0;
}
