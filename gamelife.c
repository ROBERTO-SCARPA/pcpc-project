#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <unistd.h>



char ** alloca_matrix(int n, int m){
    char ** mat = calloc(n, sizeof(char *));
    char *line = calloc(n*m, sizeof(char));

    for(int i=0; i<n; i++){
        mat[i] = line + i*m;
    }
    return mat;
}

int is_alive(char **buf, int i, int j, int M, int is_border_dead) {
    if (j < 0 || j >= M) return 0;
    if (is_border_dead) return 0;
    if (buf[i][j] == 'L') {
        return 1;
    }
    return 0;
}

void print_matrix(char **matrix, int n, int m){
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m; j++) {
            if (matrix[i][j] == 'L') {
                printf("■ ");
            } else {
                printf("□ ");
            }
        }
        printf("\n");
    }
    printf("\n");
}


int main(int argc, char ** argv){

    MPI_Init(NULL,NULL);

    int numproc, tag = 1, rank;

    MPI_Comm_size(MPI_COMM_WORLD,&numproc);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Status stat;

    int N = 5;
    int M = 5;
    int iterazioni = 3;

    char ** matrix;

    int b = N/numproc;
    int r = N % numproc;
    int Nsize = b + (rank < r ? 1 : 0);


    int global_row_start = rank * b + (rank < r ? rank : r);
    int top_is_border    = (global_row_start == 0);
    int bottom_is_border = (global_row_start + Nsize == N);

    char **buf = alloca_matrix(Nsize +2, M);
    char **newbuf = alloca_matrix(Nsize +2, M);
    

    if(rank == 0){
        matrix = alloca_matrix(N,M);
        srand(42);
        for(int i=0; i<N; i++){
            for(int j=0;j<M;j++){
                matrix[i][j] = (rand() %2 ==0 ? 'L' : 'D');
            }
        }
        int offset = Nsize;

        for(int i=0; i<Nsize; i++){
            for(int j=0;j<M;j++){
                buf[i+1][j] =  matrix[i][j];
            }
        }

        for(int i = 1; i < numproc; i++){
            int chunk = b + (i< r ? 1 : 0);
            MPI_Send(&matrix[offset][0] ,chunk*M , MPI_CHAR, i,tag,MPI_COMM_WORLD);
            offset = offset + chunk;
        }

    }
    else{
        MPI_Recv(buf[1],Nsize*M,MPI_CHAR,0,tag,MPI_COMM_WORLD,&stat);
    }

    int top,bottom;
    if(rank -1 < 0){
        top = MPI_PROC_NULL;
    } 
    else{
        top = rank -1;
    }

    if(rank + 1 >= numproc) {
        bottom = MPI_PROC_NULL;
    }
    else{
        bottom = rank + 1;
    }

    for(int step = 1; step <= iterazioni; step++){
            MPI_Request reqs[4];
            MPI_Isend(buf[1],M,MPI_CHAR,top,tag,MPI_COMM_WORLD,&reqs[0]);
            MPI_Isend(buf[Nsize],M,MPI_CHAR,bottom,tag,MPI_COMM_WORLD,&reqs[1]);
            MPI_Irecv(buf[0],M,MPI_CHAR,top,tag, MPI_COMM_WORLD,&reqs[2]);
            MPI_Irecv(buf[Nsize + 1],M,MPI_CHAR,bottom,tag, MPI_COMM_WORLD,&reqs[3]);
        


        for(int i = 2; i<= Nsize - 1; i++){
            for(int j=0; j<M;j++){
                int live_cells = 0;
                live_cells += is_alive(buf,i-1,j-1,M,0);
                live_cells += is_alive(buf,i-1,j,M,0);
                live_cells += is_alive(buf,i-1,j+1,M,0);

                live_cells += is_alive(buf,i,j-1,M,0);
                live_cells += is_alive(buf,i,j+1,M,0);

                live_cells += is_alive(buf,i+1,j-1,M,0);
                live_cells += is_alive(buf,i+1,j,M,0);
                live_cells += is_alive(buf,i+1,j+1,M,0);

                int alive = is_alive(buf,i,j,M,0);

                if(alive){
                    if(live_cells == 2 || live_cells == 3){
                        newbuf[i][j] = 'L';
                    }
                    if(live_cells < 2 || live_cells > 3){
                        newbuf[i][j] = 'D';
                    }
                }
                else{
                    if(live_cells == 3){
                        newbuf[i][j] = 'L';
                    }
                    else{
                        newbuf[i][j] = 'D';
                    }
                }
             }

         }

        MPI_Waitall(4,reqs,MPI_STATUSES_IGNORE);

        int bordi[] = {1, Nsize};
        int quanti_bordi = (Nsize == 1) ? 1 : 2; 

        for(int b_idx = 0; b_idx < quanti_bordi; b_idx++) {
            int i = bordi[b_idx];
            if (i < 1 || i > Nsize) continue;

            int ghost_top_dead    = (i == 1)     ? top_is_border    : 0;
            int ghost_bottom_dead = (i == Nsize) ? bottom_is_border : 0;

            for(int j = 0; j < M; j++){
                int live_cells = 0;
                live_cells += is_alive(buf, i-1, j-1, M, ghost_top_dead);
                live_cells += is_alive(buf, i-1, j,   M, ghost_top_dead);
                live_cells += is_alive(buf, i-1, j+1, M, ghost_top_dead);

                live_cells += is_alive(buf, i, j-1, M, 0);
                live_cells += is_alive(buf, i, j+1, M, 0);

                live_cells += is_alive(buf, i+1, j-1, M, ghost_bottom_dead);
                live_cells += is_alive(buf, i+1, j,   M, ghost_bottom_dead);
                live_cells += is_alive(buf, i+1, j+1, M, ghost_bottom_dead);

                int alive = is_alive(buf, i, j, M, 0);
                if(alive){
                    if(live_cells == 2 || live_cells == 3){
                        newbuf[i][j] = 'L';
                    }
                    if(live_cells < 2 || live_cells > 3){
                        newbuf[i][j] = 'D';
                    }
                }
                else{
                    if(live_cells == 3){
                        newbuf[i][j] = 'L';
                    }
                    else{
                        newbuf[i][j] = 'D';
                    }
                }
        }
    }

        char **temp = buf;
        buf = newbuf;
        newbuf = temp;

        if(rank !=0){
            MPI_Send(buf[1],Nsize*M,MPI_CHAR,0,tag,MPI_COMM_WORLD);
        }
        if(rank ==0){
            int offset = Nsize;
            for(int i=1; i<numproc; i++){
                int chunk = b + (i<r? 1:0);
                MPI_Recv(&matrix[offset][0],chunk*M,MPI_CHAR,i,tag,MPI_COMM_WORLD,&stat);
                offset += chunk;
            }
            for(int i =0; i<Nsize; i++){
                for(int j=0;j<M;j++){
                    matrix[i][j] = buf[i+1][j];
                }
            }

            printf("\033[H\033[J");
            printf("Generazione: %d / %d\n", step, iterazioni);
            print_matrix(matrix,N,M);
            fflush(stdout);
            usleep(200000);
        }
        

    }

    free(buf[0]); free(buf);
    free(newbuf[0]); free(newbuf);
    if(rank == 0 && matrix != NULL){
        free(matrix[0]); free(matrix);
    }
    
    MPI_Finalize();

    return 0;
}