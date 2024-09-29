#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#include "main_write_header_cb.c"
#include "catpng.c"

void* requestStrip(void* arg);

/* Keeps track of PNGs left to be received */
int PNGSLeft = 50;
bool PNGSLeftArray[50] = {false};

int main(int argc, char *argv[]) {

    int c;
    int t = 1; /* Default is 1 thread */
    int n = 1; /* Default is PNG 1 */

    /* Loops for each shortopts. For each case, the param is then copied. */
    while ((c = getopt (argc, argv, "t:n:")) != -1) {
        switch (c) {
            case 't':
                t = strtoul(optarg, NULL, 10);
                if (t <= 0) {
                    return -1;
                }
                break;
            case 'n':
                n = strtoul(optarg, NULL, 10);
                if (n <= 0 || n > 3) {
                    return -1;
                }
                break;
            default:
                return -1;
        }
    }

    /* Set URLs according to selected image */
    char URL[3][46] = {"http://ece252-1.uwaterloo.ca:2520/image?img=N", "http://ece252-2.uwaterloo.ca:2520/image?img=N", "http://ece252-3.uwaterloo.ca:2520/image?img=N"};
    sprintf(&URL[0][44], "%i", n);
    sprintf(&URL[1][44], "%i", n);
    sprintf(&URL[2][44], "%i", n);

    /* Multithreading ------------------------------------------------------ */
    int serverSelector = 0;
    pthread_t tid[t];

    /* Thread Spawning */
    for (int i = 0; i < t; ++i) {
        pthread_create(&tid[i], NULL, requestStrip, URL[serverSelector]);

        /* Iterates through server list */
        ++serverSelector;
        serverSelector = serverSelector % 3;
    }

    /* Wait for all threads to finish */
    for (int j = 0; j < t; ++j) {
        pthread_join(tid[j], NULL);
    }
    /* --------------------------------------------------------------------- */

    /* Setup all strip names for catpng */
    char pngNames[50][24];
    for (int i = 0; i < 50; ++i) {
        snprintf(pngNames[i], 24, "./strips/output_%i.png", i);
    }
    catpng(pngNames);

    return 0;
}

void* requestStrip(void* arg) {
    char* URL = (char*) arg;

    /* loop until we have all 50 PNG strips */
    while (PNGSLeft > 0) {

        /* Get random PNG. Blocking. */
        int PNGreceived = getStrip(URL, PNGSLeftArray);

        /* Duplicate Received */
        if (PNGreceived == -1) {
            continue;
        }

        /* Mark down new PNG that was received */
        --PNGSLeft;
        PNGSLeftArray[PNGreceived] = true;
    }

    return NULL;
}