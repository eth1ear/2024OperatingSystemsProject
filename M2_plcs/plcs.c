#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#define MAXN 10000

char A[MAXN], B[MAXN];
int dp[MAXN + 1][MAXN + 1];
int n, m, num_threads;
pthread_barrier_t barrier;

void *calculate_lcs(void *arg) {
    int id = *(int *)arg;
    int i, j;

    for (int round = 1; round <= n + m - 1; round++) {
        int start = (round <= m) ? 1 : (round - m + 1);
        int end = (round <= n) ? round : n;

        for (i = start + id; i <= end; i += num_threads) {
            j = round - i + 1;
            if (j >= 1 && j <= m) {
                if (A[i - 1] == B[j - 1]) {
                    dp[i][j] = dp[i - 1][j - 1] + 1;
                } else {
                    dp[i][j] = (dp[i - 1][j] > dp[i][j - 1]) ? dp[i - 1][j] : dp[i][j - 1];
                }
            }
        }

        pthread_barrier_wait(&barrier);  // Ensure all threads finish this round
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        num_threads = atoi(argv[1]);
    } else {
        num_threads = 1;
    }

    // Initialize strings A and B
    scanf("%s %s", A, B);
    n = strlen(A);
    m = strlen(B);

    // Initialize dp array
    memset(dp, 0, sizeof(dp));

    pthread_t threads[num_threads];
    int thread_ids[num_threads];

    // Initialize barrier
    pthread_barrier_init(&barrier, NULL, num_threads);

    // Create threads
    for (int i = 0; i < num_threads; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, calculate_lcs, (void *)&thread_ids[i]);
    }

    // Join threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Destroy barrier
    pthread_barrier_destroy(&barrier);

    // Output the length of the LCS
    printf("%d\n", dp[n][m]);

    return 0;
}

