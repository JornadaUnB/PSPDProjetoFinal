#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <mpi.h>
#include <omp.h>

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

int main(int argc, char *argv[])
{
    double t0 = 0, t1 = 0, t2 = 0, t3 = 0;
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    for (int pow = POWMIN; pow <= POWMAX; pow++)
    {
        int tam = 1 << pow;
        int *tabulIn = (int *)malloc((tam + 2) * (tam + 2) * sizeof(int));
        int *tabulOut = (int *)malloc((tam + 2) * (tam + 2) * sizeof(int));

        if (rank == 0)
        {
            t0 = wall_time();
            InitTabul(tabulIn, tabulOut, tam);
            t1 = wall_time();
        }

        MPI_Bcast(tabulIn, (tam + 2) * (tam + 2), MPI_INT, 0, MPI_COMM_WORLD);

        int effective_size = size;
        if (size > tam && tam > 0) {
             effective_size = tam;
        }

        if (rank >= effective_size) {
            free(tabulIn);
            free(tabulOut);
            continue; 
        }

        int rows_per_process_base = tam / effective_size;
        int remainder = tam % effective_size;
        int start_row = rank * rows_per_process_base + (rank < remainder ? rank : remainder) + 1;
        int end_row = start_row + rows_per_process_base + (rank < remainder ? 1 : 0) - 1;
        
        // O número de gerações correto é 4 * (tam - 3)
        for (int i = 0; i < 4 * (tam - 3); i++)
        {
            // CORREÇÃO DO DEADLOCK: Unificar as tags para 0
            // Troca de bordas com o vizinho de cima (rank-1)
            if (rank > 0)
            {
                MPI_Sendrecv(&tabulIn[ind2d(start_row, 0)], tam + 2, MPI_INT, rank - 1, 0,
                             &tabulIn[ind2d(start_row - 1, 0)], tam + 2, MPI_INT, rank - 1, 0,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            // Troca de bordas com o vizinho de baixo (rank+1)
            if (rank < effective_size - 1)
            {
                MPI_Sendrecv(&tabulIn[ind2d(end_row, 0)], tam + 2, MPI_INT, rank + 1, 0,
                             &tabulIn[ind2d(end_row + 1, 0)], tam + 2, MPI_INT, rank + 1, 0,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }

            UmaVida(tabulIn, tabulOut, tam, start_row, end_row);

            int *temp = tabulIn;
            tabulIn = tabulOut;
            tabulOut = temp;
        }

        // Coleta de Resultados (Estrutura Original Mantida)
        if (rank == 0)
        {
            for (int p = 1; p < effective_size; p++)
            {
                // Recalcula a fatia do processo 'p' para saber o que esperar
                int p_rows_base = tam / effective_size;
                int p_rem = tam % effective_size;
                int p_start = p * p_rows_base + (p < p_rem ? p : p_rem) + 1;
                int p_end = p_start + p_rows_base + (p < p_rem ? 1 : 0) - 1;
                int elements_to_receive = (p_end - p_start + 1) * (tam + 2);
                
                MPI_Recv(&tabulIn[ind2d(p_start, 0)], elements_to_receive,
                         MPI_INT, p, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        }
        else // Apenas os processos ativos (rank > 0 e rank < effective_size) enviam
        {
            int elements_to_send = (end_row - start_row + 1) * (tam + 2);
            MPI_Send(&tabulIn[ind2d(start_row, 0)], elements_to_send,
                     MPI_INT, 0, 0, MPI_COMM_WORLD);
        }
        
        if (rank == 0)
        {
            t2 = wall_time();
            if (Correto(tabulIn, tam))
                printf("**RESULTADO CORRETO**\n");
            else
                printf("**RESULTADO ERRADO**\n");
            t3 = wall_time();
            printf("tam=%d; tempos: init=%7.7f, comp=%7.7f, fim=%7.7f, tot=%7.7f \n",
                   tam, t1 - t0, t2 - t1, t3 - t2, t3 - t0);
        }

        free(tabulIn);
        free(tabulOut);
    }

    MPI_Finalize();
    return 0;
}
