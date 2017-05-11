#include <algorithm>
#include <assert.h>
#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <tuple>

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define BLOCK_LOW(id, p, n) ((id) * (n) / (p))
#define BLOCK_HIGH(id, p, n) (BLOCK_LOW((id) + 1, p, n) - 1)
#define BLOCK_SIZE(id, p, n) (BLOCK_HIGH(id, p, n) - BLOCK_LOW(id, p, n) + 1)
#define BLOCK_OWNER(index, p, n) (((p) * ((index) + 1) - 1) / (n))

// Node representing elements of the sparse matrix
struct node {
    short z; // The z value
    short num_neighbours; // The # of neighbours (not always right, only when we need it)
    bool is_dead; // Is it a dead or an alive node?
    bool operator<(const struct node& rhs) const
    {
        return z < rhs.z;
    }
};

void print_node(struct node* n)
{
    printf("{z: %hd, num_nei: %hd, is_dead: %s}\n", n->z, n->num_neighbours, n->is_dead ? "true" : "false");
}

typedef struct da_struct {
    size_t initial_size;

    void* data;
    size_t used;
    size_t size;
    size_t step; // Size of an element
} dynamic_array;

void da_init(dynamic_array* da, size_t initial_size)
{
    da->initial_size = initial_size;
    da->step = sizeof(struct node);
    da->used = 0;

    da->size = da->initial_size;
    da->data = realloc(NULL, da->initial_size * da->step);
}

dynamic_array da_make(size_t initial_size)
{
    dynamic_array da;
    da_init(&da, MAX(4, initial_size));
    return da;
}

dynamic_array* da_make_ptr(size_t initial_size)
{
    dynamic_array* da = (dynamic_array*)malloc(sizeof(dynamic_array));
    da_init(da, MAX(4, initial_size));
    return da;
}

void da_resize(dynamic_array* da, size_t new_size)
{
    assert(da);

    da->size = new_size;
    da->data = realloc(da->data, new_size * da->step);
    // printf("Reallocating to size %d\n", da->size);
}

void da_insert(dynamic_array* da, struct node* to_insert)
{
    assert(da);
    // printf("Inserting in da '%p'\n", da);

    if (da->used == da->size) {
        da_resize(da, da->size * 2);
    }

    struct node* dest = ((struct node*)da->data) + da->used;
    memcpy(dest, to_insert, da->step);
    da->used++;
}

void da_delete_at(dynamic_array* da, size_t i)
{
    assert(da);

    // printf("Deleting in da '%p' at: %lu\n", da, i);
    if (i >= da->used) { // i < 0  is always false, cause unsigned
        printf("Invalid delete: index smaller or larger than the array size!\n");
        return;
    }

    da->used--;

    struct node* dest = ((struct node*)da->data) + i;
    struct node* src = ((struct node*)da->data) + da->used;
    memcpy(dest, src, da->step);

    if (da->size > da->initial_size && da->used <= da->size / 4) {
        da_resize(da, da->size / 2);
    }
}

void da_free(dynamic_array* da)
{
    assert(da);

    if (da->data) {
        free(da->data);
        da->data = NULL;
        da->used = 0;
        da->size = 0;
    }
}

void da_print(dynamic_array* da)
{
    assert(da);

    printf("************************\n");
    printf("size: %lu\n", da->size);
    printf("used: %lu\n", da->used);
    printf("data:\n");

    size_t i;
    for (i = 0; i < da->used; i++) {
        struct node* ptr = ((struct node*)da->data) + i;
        printf("  ");
        print_node(ptr);
    }

    printf("************************\n");
}

int da_empty(dynamic_array* da)
{
    assert(da);

    return da->used;
}

void da_clear(dynamic_array* da)
{
    assert(da);

    da->used = 0;
    da_resize(da, da->initial_size);
}

int da_find_z(dynamic_array* da, short test_z)
{
    assert(da);

    // printf("\n\nStarting loop with da '%p'. Used = %lu; Data: %p\n", da, da->used, da->data);
    fflush(stdout);

    for (size_t i = 0; i < da->used; i++) {
        struct node* ptr = ((struct node*)da->data) + i;

        if (ptr->z == test_z) {
            return i;
        }
    }
    return -1;
}

// ############################################################
// ######################### MATRIX ###########################
// ############################################################
struct matrix_struct {
    short side;
    dynamic_array** data;
};
typedef matrix_struct Matrix;
void matrix_print(Matrix* m);

// Returns a initialized matrix
inline Matrix make_matrix(short side)
{
    Matrix m;
    m.side = side;
    m.data = (dynamic_array**)calloc(side * side, sizeof(dynamic_array*));
    return m;
}

// Returns the head of the matrix in the pos\ition x and y. Can be NULL.
inline dynamic_array* matrix_get(Matrix* m, short x, short y)
{
    return m->data[x + (y * m->side)];
}

// Returns the element with the x, y, z passed. NULL if it doesn't exist.
inline struct node* matrix_get_ele(Matrix* m, short x, short y, short z)
{
    dynamic_array* da = matrix_get(m, x, y);
    if (!da) {
        return NULL;
    }

    int pos = da_find_z(da, z);
    if (pos == -1) {
        return NULL;
    } else {
        return ((struct node*)da->data) + pos;
    }
}

// Insert a new element in the matrix (ordered)
inline void matrix_insert(Matrix* m, short x, short y, short z, bool is_dead, short num_nei)
{
    struct node new_el = { z, num_nei, is_dead };

    dynamic_array* da = matrix_get(m, x, y);
    if (da == NULL) {
        da = da_make_ptr(4);
        m->data[x + (y * m->side)] = da;
    }
    da_insert(da, &new_el);
}

// Remove from the matrix. We need to search for the z in the linked list
inline void matrix_remove(Matrix* m, short x, short y, short z)
{
    dynamic_array* da = matrix_get(m, x, y);
    if (!da) {
        return;
    }
    int pos = da_find_z(da, z);
    //printf("Trying to delete %hd %hd %hd\n", x, y, z);
    if (pos != -1) {
        da_delete_at(da, pos);
    }
}

// Print the live nodes in the matrix (the matrix only contains alive nodes at this point, so print all nodes)
void matrix_print_live(Matrix* m)
{
    short SIZE = m->side;
    for (short i = 0; i < SIZE; i++) {
        for (short j = 0; j < SIZE; j++) {
            dynamic_array* da = matrix_get(m, i, j);
            if (da != NULL) {
                struct node* ptr = ((struct node*)da->data);
                // Kinda sucks to sort here, but oh well
                std::sort(ptr, (ptr + da->used));
                for (size_t k = 0; k < da->used; k++) {
                    printf("%hd %hd %hd\n", i, j, ptr->z);
                    ptr++;
                }
            }
        }
    }
}

void matrix_print(Matrix* m)
{
    short SIZE = m->side;
    for (short i = 0; i < SIZE; i++) {
        for (short j = 0; j < SIZE; j++) {
            dynamic_array* da = matrix_get(m, i, j);
            if (da != NULL) {
                struct node* ptr = ((struct node*)da->data);
                printf("(%hd, %hd): [", i, j);
                for (size_t k = 0; k < da->used; k++) {
                    printf("%hd%c", ptr->z, k == da->used - 1 ? '\x07' : ',');
                    ptr++;
                }
                printf("] (size: %lu; used: %lu)\n", da->size, da->used);
            } else {
                printf("(%hd, %hd): []\n", i, j);
            }
        }
    }
}

inline short pos_mod(short val, short mod)
{
    if (val >= mod)
        return val - mod;
    else if (val < 0)
        return val + mod;
    else
        return val;

    // The method above is faster!
    // return ((val % mod) + mod) % mod;
    // return (val % mod) + (mod * (val < 0));
}

int main(int argc, char* argv[])
{
    int id, p, u, a;
    double elapsed_time, init_time;

    MPI_Init(&argc, &argv);
    MPI_Barrier(MPI_COMM_WORLD);
    elapsed_time = -MPI_Wtime();

    MPI_Comm_rank(MPI_COMM_WORLD, &id); // get current process id
    MPI_Comm_size(MPI_COMM_WORLD, &p);

    Matrix m;
    int generations;
    short SIZE;

    if (id == 0) {
        if (argc != 3) {
            printf("[ERROR] Incorrect usage!\n");
            printf("[Usage] ./life3d <input_file> <nr_generations>\n");
            return -1;
        }
        char* input_file = argv[1];
        generations = atoi(argv[2]);

        if (generations <= 0) {
            printf("[ERROR] Number of generations must be bigger that 0. Got: '%d'\n", generations);
            return -1;
        }

        FILE* fp = fopen(input_file, "r");
        if (fp == NULL) {
            printf("[ERROR] Unable to read the input file.\n");
            perror("[ERROR]");
            return -1;
        }

        if (fscanf(fp, "%hd", &SIZE) == EOF) {
            printf("[ERROR] Unable to read the size.\n");
            return -1;
        }

        // Finished parsing metadata. Now only need to parse the actual positions
        m = make_matrix(SIZE);
        short x, y, z;
        while (fscanf(fp, "%hd %hd %hd", &x, &y, &z) != EOF) {
            matrix_insert(&m, x, y, z, false, -1);
        }
        // Finished parsing!
        fclose(fp);

        // matrix_print(&m);

        // ==================================
        // === WE NOW START TO SEND STUFF ===
        // ==================================
        // Send generations and size
        int buf[2];
        buf[0] = (int)SIZE;
        buf[1] = generations;
        MPI_Bcast(buf, 2, MPI_INT, 0, MPI_COMM_WORLD);

        printf("==============================================================\n");
        // Send block size
        for (int x = 0; x < SIZE; x++) {
            int z_lengths[SIZE];
            for (int y = 0; y < SIZE; y++) {
                dynamic_array* da = matrix_get(&m, x, y);
                z_lengths[y] = (da == NULL) ? 0 : da->used;
                printf("[TO: %d] (%d, %d) -> %d\n", BLOCK_OWNER(x, p, SIZE), x, y, z_lengths[y]);
                fflush(stdout);
                MPI_Send(z_lengths, SIZE, MPI_INT, BLOCK_OWNER(x, p, SIZE), 0, MPI_COMM_WORLD);
            }

            // @TODO: Send rows to each row owner
            /*for (int x = 0; x < SIZE; x++) {
                for (int y = 0; y < SIZE; y++) {
                    dynamic_array* da = matrix_get(&m, x, y);
                    // Send the row to the corresponding process
                    MPI_Send(da->data, da->step * da->used, MPI_BYTE, BLOCK_OWNER(x, p, SIZE), 0, MPI_COMM_WORLD);
                }
            }*/
        }

    } else {
        // All processors that don't read the file.

        // Receive SIZE and generations
        int buf[2];
        MPI_Bcast(buf, 2, MPI_INT, 0, MPI_COMM_WORLD);
        SIZE = (short)buf[0];
        generations = buf[1];
        // printf("[%d] Received (SIZE, generations) = (%hd, %d)\n", id, SIZE, generations);

        // Recv my rows
        m = make_matrix(SIZE);
        for (int x = BLOCK_LOW(id, p, SIZE); x <= BLOCK_HIGH(id, p, SIZE); x++) {
            int z_lengths[SIZE];
            MPI_Recv(z_lengths, SIZE, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (int y = 0; y < SIZE; y++) {
                if (id == 1) {
                    printf("  [RV: %d] (%d, %d) -> %d\n", id, x, y, z_lengths[y]);
                    fflush(stdout);
                }
                if (z_lengths[y] == 0) {
                    continue;
                }
                dynamic_array* da = matrix_get(&m, x, y);
                if (da == NULL) {
                    da = da_make_ptr(4);
                    m.data[x + (y * m.side)] = da;
                }

                // MPI_Recv(da->data, (da->step * z_lengths[y]), MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        }

        //printf("==============================================================\n");
        //printf("[%d]\n", id);
        //matrix_print(&m);
    }

    init_time = elapsed_time + MPI_Wtime();

    /*dynamic_array* sen;
    dynamic_array* rec;

    sen = matrix_get(&m, 1, 1);

    a = sen->step * sen->used;
    printf("USED:A %d\n", sen->used);
    MPI_Send(sen->data, a, MPI_BYTE, 2, 0, MPI_COMM_WORLD);

    if (id == 2) {
        MPI_Recv(rec->data, a, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    struct node* ptr;

    if (id == 2) {
        for (u = 0; u < sen->used; u++) {
            ptr = ((struct node*)rec->data) + u;
            printf("DADOS RECEBIDOS: UZ %d %d\n", u, ptr->z);
        }
        printf("HEY\n");
    }*/

    //-----------------
    //--- MAIN LOOP ---
    //-----------------
    /*
    for (int gen = 0; gen < generations; gen++) {
        dynamic_array* da;
        struct node *ptr, *to_test;
        Vector3 t;
        int32_t i, j;
        short z, _z, _y, _x, y, x;

        // printf("==============================================================\n");
        // printf("==================== BEFORE INSERTING ========================\n");
        // printf("==============================================================\n");
        // matrix_print(&m);
        for (i = 0; i < SIZE; i++) {
            for (j = 0; j < SIZE; j++) {
                da = matrix_get(&m, i, j);
                if (!da) {
                    continue;
                }

                size_t limit = da->used;
                // Iterate over every existing z for x and y
                for (size_t k = 0; k < limit; k++) {
                    ptr = ((struct node*)da->data) + k;

                    // If its dead skip
                    if (ptr->is_dead) {
                        continue;
                    }

                    // PROCESS AN ALIVE NODE
                    x = i;
                    y = j;
                    z = ptr->z;
                    ptr->num_neighbours = 0;

                    _z = pos_mod(z + 1, SIZE);
                    to_test = matrix_get_ele(&m, x, y, _z);
                    if (to_test) {
                        if (to_test->is_dead == true) {
                            to_test->num_neighbours++;
                        } else {
                            ptr->num_neighbours++;
                        }
                    } else {
                        matrix_insert(&m, x, y, _z, true, 1);
                        ptr = ((struct node*)da->data) + k;
                    }

                    _z = pos_mod(z - 1, SIZE);
                    to_test = matrix_get_ele(&m, x, y, _z);
                    if (to_test) {
                        if (to_test->is_dead == true) {
                            to_test->num_neighbours++;
                        } else {
                            ptr->num_neighbours++;
                        }
                    } else {
                        matrix_insert(&m, x, y, _z, true, 1);
                        ptr = ((struct node*)da->data) + k;
                    }

                    _x = pos_mod(x + 1, SIZE);
                    to_test = matrix_get_ele(&m, _x, y, z);
                    if (to_test) {
                        if (to_test->is_dead == true) {
                            to_test->num_neighbours++;
                        } else {
                            ptr->num_neighbours++;
                        }
                    } else {
                        matrix_insert(&m, _x, y, z, true, 1);
                        ptr = ((struct node*)da->data) + k;
                    }

                    _x = pos_mod(x - 1, SIZE);
                    to_test = matrix_get_ele(&m, _x, y, z);
                    if (to_test) {
                        if (to_test->is_dead == true) {
                            to_test->num_neighbours++;
                        } else {
                            ptr->num_neighbours++;
                        }
                    } else {
                        matrix_insert(&m, _x, y, z, true, 1);
                        ptr = ((struct node*)da->data) + k;
                    }

                    _y = pos_mod(y + 1, SIZE);
                    to_test = matrix_get_ele(&m, x, _y, z);
                    if (to_test) {
                        if (to_test->is_dead == true) {
                            to_test->num_neighbours++;
                        } else {
                            ptr->num_neighbours++;
                        }
                    } else {
                        matrix_insert(&m, x, _y, z, true, 1);
                        ptr = ((struct node*)da->data) + k;
                    }

                    _y = pos_mod(y - 1, SIZE);
                    to_test = matrix_get_ele(&m, x, _y, z);
                    if (to_test) {
                        if (to_test->is_dead == true) {
                            to_test->num_neighbours++;
                        } else {
                            ptr->num_neighbours++;
                        }
                    } else {
                        matrix_insert(&m, x, _y, z, true, 1);
                        ptr = ((struct node*)da->data) + k;
                    }
                }
            }
        }

        // printf("=============================================================\n");
        // printf("==================== BEFORE DELETING ========================\n");
        // printf("=============================================================\n");
        // matrix_print(&m);
        for (i = 0; i < SIZE; i++) {
            for (j = 0; j < SIZE; j++) {
                da = matrix_get(&m, i, j);
                if (!da) {
                    continue;
                }
                // Iterate over every existing z for x and y
                for (int k = (int)da->used - 1; k >= 0; k--) {
                    ptr = ((struct node*)da->data) + k;
                    if (ptr->is_dead) {
                        if (ptr->num_neighbours == 2 || ptr->num_neighbours == 3) {
                            ptr->is_dead = false;
                        } else {
                            matrix_remove(&m, i, j, ptr->z);
                            ptr = ((struct node*)da->data) + k;
                        }
                    } else {
                        if (ptr->num_neighbours < 2 || ptr->num_neighbours > 4) {
                            matrix_remove(&m, i, j, ptr->z);
                            ptr = ((struct node*)da->data) + k;
                        }
                    }
                }
            }
        }
    }

    // end = omp_get_wtime();
    // process_time = end - start;

    //-----------
    //--- END ---
    //-----------
    // Output the result
    // matrix_print(&m);
    matrix_print_live(&m);

    // Free all (uneccessary)
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            dynamic_array* da = matrix_get(&m, i, j);
            if (!da) {
                continue;
            }
            da_free(da);
            free(da);
            da = NULL;
        }
    }
    free(m.data);*/

    // Write the time log to a file
    // FILE* out_fp = fopen("time.log", "w");
    // char out_str[80];
    // sprintf(out_str, "OMP %s: \ninit_time: %lf \nproc_time: %lf\n", input_file, init_time, process_time);
    // fwrite(out_str, strlen(out_str), 1, out_fp);

    // printf("[%d] Init time: %lf\n", id, init_time);
    MPI_Finalize();
    return 0;
}
