#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <mpi.h>
#include <omp.h>

#include <librdkafka/rdkafka.h>
#include <string.h>


#define ind2d(i, j, tam) (i) * (tam + 2) + j

double wall_time(void)
{
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);
    return (tv.tv_sec + tv.tv_usec / 1000000.0);
}

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

int main(int argc, char *argv[]) {
    int rank, size;
    int pow_min = 0, pow_max = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // ================================================================
    // A LÓGICA DO KAFKA FICA APENAS NO RANK 0
    // ================================================================
    if (rank == 0) {
        rd_kafka_t *rk;
        rd_kafka_conf_t *conf;
        char errstr[512];

        conf = rd_kafka_conf_new();
        if (rd_kafka_conf_set(conf, "bootstrap.servers", "kafka:9092", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
            fprintf(stderr, "%% Erro de configuração Kafka: %s\n", errstr);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        if (rd_kafka_conf_set(conf, "group.id", "engine-mpi-omp-group", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
            fprintf(stderr, "%% Erro de configuração Kafka: %s\n", errstr);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
        if (!rk) {
            fprintf(stderr, "%% Falha ao criar consumidor: %s\n", errstr);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        
        rd_kafka_poll_set_consumer(rk);

        rd_kafka_topic_partition_list_t *topics = rd_kafka_topic_partition_list_new(1);
        rd_kafka_topic_partition_list_add(topics, "omp-mpi-topic", -1);
        rd_kafka_subscribe(rk, topics);
        rd_kafka_topic_partition_list_destroy(topics);

        printf("[Rank 0] Serviço iniciado. Aguardando por tarefas do Kafka...\n");

        // LOOP DE SERVIÇO: Rank 0 espera por tarefas indefinidamente
        while (1) {
            rd_kafka_message_t *rkm = rd_kafka_consumer_poll(rk, 1000);
            if (!rkm) continue;

            if (rkm->err) {
                if (rkm->err != RD_KAFKA_RESP_ERR__PARTITION_EOF) {
                    fprintf(stderr, "%% Erro de consumo: %s\n", rd_kafka_message_errstr(rkm));
                }
                rd_kafka_message_destroy(rkm);
                continue;
            }

            // Mensagem recebida! Parse manual simples do JSON.
            const char* payload = (const char*)rkm->payload;
            char* p_min_str = strstr(payload, "\"pow_min\"");
            char* p_max_str = strstr(payload, "\"pow_max\"");

            if (p_min_str && p_max_str) {
                sscanf(p_min_str, "\"pow_min\":%d", &pow_min);
                sscanf(p_max_str, "\"pow_max\":%d", &pow_max);
                printf("\n[Rank 0] Nova tarefa recebida. Processando intervalo de POW: %d a %d\n", pow_min, pow_max);
            } else {
                fprintf(stderr, "[Rank 0] JSON inválido. Esperando por 'pow_min' e 'pow_max'. Payload: %s\n", payload);
                rd_kafka_message_destroy(rkm);
                continue; // Volta a esperar por uma mensagem válida
            }
            rd_kafka_message_destroy(rkm);

            // Transmite o intervalo para os outros ranks e sai do loop de escuta para processar
            MPI_Bcast(&pow_min, 1, MPI_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(&pow_max, 1, MPI_INT, 0, MPI_COMM_WORLD);

            for (int pow = pow_min; pow <= pow_max; pow++) {
                int tam = 1 << pow;
                double t0 = 0, t1 = 0, t2 = 0, t3 = 0;
                int *tabulIn = (int *)malloc((tam + 2) * (tam + 2) * sizeof(int));
                int *tabulOut = (int *)malloc((tam + 2) * (tam + 2) * sizeof(int));

                if (rank == 0) {
                    t0 = wall_time();
                    InitTabul(tabulIn, tabulOut, tam);
                    t1 = wall_time();
                }

                MPI_Bcast(tabulIn, (tam + 2) * (tam + 2), MPI_INT, 0, MPI_COMM_WORLD);

                int effective_size = (size > tam && tam > 0) ? tam : size;

                if (rank >= effective_size) {
                    free(tabulIn);
                    free(tabulOut);
                    continue;
                }

                int rows_per_process_base = tam / effective_size;
                int remainder = tam % effective_size;
                int start_row = rank * rows_per_process_base + (rank < remainder ? rank : remainder) + 1;
                int end_row = start_row + rows_per_process_base + (rank < remainder ? 1 : 0) - 1;

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

                if (rank == 0) {
                    for (int p = 1; p < effective_size; p++) {
                        int p_rows_base = tam / effective_size;
                        int p_rem = tam % effective_size;
                        int p_start = p * p_rows_base + (p < p_rem ? p : p_rem) + 1;
                        int p_end = p_start + p_rows_base + (p < p_rem ? 1 : 0) - 1;
                        MPI_Recv(&tabulIn[ind2d(p_start, 0, tam)], (p_end - p_start + 1) * (tam + 2), MPI_INT, p, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    }
                } else {
                    MPI_Send(&tabulIn[ind2d(start_row, 0, tam)], (end_row - start_row + 1) * (tam + 2), MPI_INT, 0, 0, MPI_COMM_WORLD);
                }

                if (rank == 0) {
                    t2 = wall_time();
                    if (Correto(tabulIn, tam))
                        printf("**RESULTADO CORRETO**\n");
                    else
                        printf("**RESULTADO ERRADO**\n");
                    t3 = wall_time();
                    printf("tam=%d; tempos: init=%7.7f, comp=%7.7f, fim=%7.7f, tot=%7.7f\n",
                           tam, t1 - t0, t2 - t1, t3 - t2, t3 - t0);
                }

                free(tabulIn);
                free(tabulOut);
            } 

            if (rank == 0) {
                 printf("\n[Rank 0] Todas as tarefas do intervalo concluídas. Aguardando próximo comando do Kafka...\n");
            }

        } // Fim do loop while(1) do Kafka
    } 
    // ================================================================
    // LÓGICA DOS WORKERS (RANK > 0) - ESPERA E EXECUTA
    // ================================================================
    else {
        // Workers ficam em um loop, aguardando tarefas do rank 0
        while (1) {
            MPI_Bcast(&pow_min, 1, MPI_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(&pow_max, 1, MPI_INT, 0, MPI_COMM_WORLD);
            
            for (int pow = pow_min; pow <= pow_max; pow++) {
                int tam = 1 << pow;
                int *tabulIn = (int *)malloc((tam + 2) * (tam + 2) * sizeof(int));
                int *tabulOut = (int *)malloc((tam + 2) * (tam + 2) * sizeof(int));

                MPI_Bcast(tabulIn, (tam + 2) * (tam + 2), MPI_INT, 0, MPI_COMM_WORLD);

                int effective_size = (size > tam && tam > 0) ? tam : size;

                if (rank >= effective_size) {
                    free(tabulIn);
                    free(tabulOut);
                    continue;
                }
                
                int rows_per_process_base = tam / effective_size;
                int remainder = tam % effective_size;
                int start_row = rank * rows_per_process_base + (rank < remainder ? rank : remainder) + 1;
                int end_row = start_row + rows_per_process_base + (rank < remainder ? 1 : 0) - 1;

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

                MPI_Send(&tabulIn[ind2d(start_row, 0, tam)], (end_row - start_row + 1) * (tam + 2), MPI_INT, 0, 0, MPI_COMM_WORLD);

                free(tabulIn);
                free(tabulOut);
            }
        }
    }

    MPI_Finalize();
    return 0;
}