#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mpi.h"

/* CONTANTES */
#define GRAU         400
#define TAM_INI  1000000
#define TAM_INC  1000000
#define TAM_MAX 10000000
#define BAG_SIZE   100

/* VARIAVEIS GLOBAIS */
double x[TAM_MAX], y[TAM_MAX], gabarito[TAM_MAX];

/* PROTOTIPOS */
double polinomio(double v[], double x);
void erro(char *msg_erro);

double polinomio(double a[], double x)
{
  int i;
  double res = a[0], pot = x;
  for (i = 1; i <= GRAU; ++i)
  {
    res += a[i] * pot;
    pot = pot * x;
  }
  return res;
}

int lastMessageReceiverId = 0;
int getProcessIdToSendMessage(int n)
{
  if (lastMessageReceiverId < n - 1)
  {
    lastMessageReceiverId++;
  }
  else
  {
    lastMessageReceiverId = 1;
  }
  return lastMessageReceiverId;
}

void erro(char *msg_erro)
{
  fprintf(stderr, "ERRO: %s\n", msg_erro);
  MPI_Finalize();
  exit(1);
}

int main(int argc, char **argv)
{
  int id; /* Identificador do processo */
  int n;  /* Numero de processos */
  int i, size;
  double *vet, valor, *vresp, resposta, tempo, a[GRAU + 1];
  int hostsize; /* Tamanho do nome do nodo */
  char hostname[MPI_MAX_PROCESSOR_NAME];
  MPI_Status status; /* Status de retorno */

  MPI_Init(&argc, &argv);
  MPI_Get_processor_name(hostname, &hostsize);
  MPI_Comm_rank(MPI_COMM_WORLD, &id);
  MPI_Comm_size(MPI_COMM_WORLD, &n);

  /* Gera os coeficientes do polinomio */
  #pragma omp parallel for
  for (i = 0; i <= GRAU; ++i)
    a[i] = (i % 3 == 0) ? -1.0 : 1.0;

  int processBagCount = BAG_SIZE;

  if (id == 0)
  {
    /* Preenche vetores */
    #pragma omp parallel for
    for (i = 0; i < TAM_MAX; ++i)
    {
      x[i] = 0.1 + 0.1 * (double)i / TAM_MAX;
      gabarito[i] = polinomio(a, x[i]);
    }
    /* Gera tabela com tamanhos e tempos */
    for (size = TAM_INI; size <= TAM_MAX; size += TAM_INC)
    {
      lastMessageReceiverId = 0;
      /* Calcula */
      tempo = -MPI_Wtime();

      for (int j = 0; j <= size; j += processBagCount)
      {
        double processBag[processBagCount];
        int p = getProcessIdToSendMessage(n);
        // printf("root id=%d event=mounting_message to=%d size=%d\n", id, p, processBagCount);
        for (int k = 0; k < processBagCount; k++) {
          int index = j + k;
          processBag[k] = x[index];
        }
        // printf("root id=%d event=sent_message to=%d\n", id, p);
        MPI_Send(&processBag, processBagCount, MPI_DOUBLE, p, 1, MPI_COMM_WORLD);
      }

// printf("root id=%d event=all_messages_sent\n", id);
      lastMessageReceiverId = 0;
      for (int j = 0; j <= size; j += processBagCount)
      {
        double resultBag[processBagCount];
        int p = getProcessIdToSendMessage(n);
        MPI_Recv(&resultBag, processBagCount, MPI_DOUBLE, p, 1, MPI_COMM_WORLD, &status);
        for (int k = 0; k < processBagCount; k++)
        {
          int index = j + k;
          y[index] = resultBag[k];
        }
      }

      // printf("root id=%d event=all_messages_received\n", id);

      tempo += MPI_Wtime();
      /* Verificacao */
      for (i = 0; i < size; ++i)
      {
        if (y[i] != gabarito[i])
        {
          //  printf("%f != %f i=%d\n", y[i], gabarito[i], i);
          erro("verificacao falhou!\n");
        }
      }
      /* Mostra tempo */
      printf("%d %lf\n", size, tempo);
    }
  }
  else
  {
    // printf("slave id=%d event=started\n", id);
    int receivedMsgs = 0;
    while (1)
    {
      double processBag[processBagCount];
      MPI_Recv(&processBag, processBagCount, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD, &status);
      receivedMsgs++;
      double result[processBagCount];
      for (int i = 0; i < processBagCount; i++)
      {
        result[i] = polinomio(a, processBag[i]);
      }
      // printf("slave id=%d event=send_msg\n", id);
      MPI_Send(&result, processBagCount, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD);
    }
  }
  MPI_Finalize();
  return 0;
}