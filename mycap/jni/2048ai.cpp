/*
 * TARGET_JB:  ver >= 4.1.2
 * TARGET_ICS: ver >= 4.0
 * other: ver < 4.0
 */
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <math.h>
#include <err.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>

#define FRAME_BUFFER_DEV "/dev/graphics/fb0"

#define LOG(fmt, arg...)      _LOG("[get-raw-image]" fmt "\n", ##arg)
#define LOGERR(fmt, arg...)   _LOG("[get-raw-image][Error%d(%s)]" fmt "\n", errno, strerror(errno), ##arg)
#define ABORT(fmt, arg...)  ({_LOG("[get-raw-image][Error%d(%s)]" fmt ". Now exit\n", errno, strerror(errno), ##arg); exit(1);})

#define max(A, B) ((A) > (B) ? (A) : (B))
#define min(A, B) ((A) < (B) ? (A) : (B))
#define MAX_VALUE ~(1 << 31)
#define MIN_VALUE (-1 * MAX_VALUE)

/*
typedef void(*FREE_FUNC)(void*);
FREE_FUNC old_free = &free;

#define free(p) do { \
printf("free %p\n", p); \
old_free(p); \
p = 0; \
printf("~free %p\n", p); \
} while(0)
*/


void* stack_org = 0;
int color_bg = 0;
int grid_ids[20] = {13484212, 15656154 + (1 << 24)};
int special_ids[3] = {13484212, 15656154 + (1 << 24), 0};
int grid_guess[4 * 4] = {};

void _LOG(const char* format, ...) {
    char buf[4096];
    int cnt;
    va_list va;
    struct timeval tv;
    long long mms;
    time_t t;
    struct tm * st;

    gettimeofday(&tv, NULL);
    mms = ((long long) tv.tv_sec) * (1000 * 1000) + tv.tv_usec;
    t = time(NULL);
    st = localtime(&t);

    memset(buf, 0, sizeof(buf));
    sprintf(buf, "%02d/%02d %02d:%02d:%02d.%06d",
        st->tm_mon+1,
        st->tm_mday,
        st->tm_hour,
        st->tm_min,
        st->tm_sec,
        (int)(mms%1000000)
        );
    cnt = strlen(buf);

    va_start(va, format);
    vsnprintf(buf+cnt, sizeof(buf)-cnt-1, format, va);
    va_end(va);

    cnt = strlen(buf); //gcc 3.3 snprintf can not correctly return copied length, so i have to count by myself
    if (cnt==0 || buf[cnt-1]!='\n') {
        buf[cnt++] = '\n';
    }
    write(STDERR_FILENO, buf, cnt);
}

void swipe(int x1, int y1, int x2, int y2) {
    char command[128];
    sprintf(command, "input swipe %d %d %d %d", x1, y1, x2, y2);
    system(command);
}

void swipe_left() {
    swipe(400, 200, 200, 200);
}

void swipe_right() {
    swipe(200, 200, 400, 200);
}

void swipe_up() {
    swipe(200, 400, 200, 200);
}

void swipe_down() {
    swipe(200, 200, 200, 400);
}

void tap(int x, int y) {
    char command[128];
    sprintf(command, "input tap %d %d", x, y);
    system(command);
}

int get_r(char *raw_data, int width, int height, int bytes_per_pixel, int x, int y) {
    return *(raw_data + bytes_per_pixel * (width * y + x));
}

int get_g(char *raw_data, int width, int height, int bytes_per_pixel, int x, int y) {
    return *(raw_data + bytes_per_pixel * (width * y + x) + 1);
}

int get_b(char *raw_data, int width, int height, int bytes_per_pixel, int x, int y) {
    return *(raw_data + bytes_per_pixel * (width * y + x) + 2);
}

int get_rgb(char *raw_data, int width, int height, int bytes_per_pixel, int x, int y) {
    return (get_r(raw_data, width, height, bytes_per_pixel, x, y) << 16) + (get_g(raw_data, width, height, bytes_per_pixel, x, y) << 8) + get_b(raw_data, width, height, bytes_per_pixel, x, y); 
}

int get_square_id(char *raw_data, int width, int height, int bytes_per_pixel, int left, int top, int right, int bottom) {
    int threshold = 15;
    int num =  0;
    int c_last = 0;
    int c_last_column = 1; //{0 : black, 1 : other}
    int digits = 0;
    for (int i = top; i < bottom; i++) {
        int c = get_rgb(raw_data, width, height, bytes_per_pixel, (left + right) / 2, i);
        if (c == color_bg) continue;
        if (c_last == 0) {
            c_last = c;
            continue;
        } else {
            if (c == c_last) {
                num++;
            } else {
                num = 0;
                c_last = c;
            }
        }
        if (num >= threshold) break;
    }

    for (int i = left; i < right; i++) {
        int c_current_column = 1;

        for (int j = top; j < bottom; j += 2) {
            if (get_rgb(raw_data, width, height, bytes_per_pixel, i, j) == 0) {
                c_current_column = 0;
                break;
            }
        }

        if (c_current_column != c_last_column) digits++;

        c_last_column = c_current_column;
    }
    digits /= 2;

    return c_last + (digits << 24);
}

int *clone_grid(int *grid) {
    int *new_grid = (int *) malloc(4 * 4 * sizeof(int));
    if(!new_grid || (unsigned)new_grid % 4 != 0) {
        printf("clone grid malloc failed\n");
        return 0;
    }

    memset(new_grid, 0, 4 * 4 * sizeof(int));
    for (int i = 0; i < 4 * 4; i++) new_grid[i] = grid[i];
    return new_grid;
}

void clear_grid(int *grid) {
    for (int i = 0; i < 4 * 4; i++) grid[i] = 0;
}

void _rotate_right(int *grid) {
    int *grid_tmp = clone_grid(grid);
    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 4; y++) {
            grid[4 - 1 - x + 4 * y] = grid_tmp[y + 4 * x];
        }
    }
    free(grid_tmp);
}

void _move_right(int *grid) {
    for (int x = 0; x < 4; x++) {
        if (grid[4 * x] != 0 && grid[3 + 4 * x] != 0 && grid[4 * x] == grid[1 + 4 * x] && grid[2 + 4 * x] == grid[3 + 4 * x] && grid[4 * x] == grid[3 + 4 * x] + 1) {
            grid[3 + 4 * x] += 2;
            grid[2 + 4 * x] += 1;
            grid[1 + 4 * x] = 0;
            grid[4 * x] = 0;
            continue;
        }

        int last_merge_position = 4;
        for (int y = 4 - 1 - 1; y >= 0; y--) {
            if (grid[y + 4 * x] == 0) continue;            
            int previous_position = y + 1;
            while ((previous_position < last_merge_position - 1) && grid[previous_position + 4 * x] == 0) previous_position++;
            if (grid[previous_position + 4 * x] == 0) {
                grid[previous_position + 4 * x] = grid[y + 4 * x];
                grid[y + 4 * x] = 0;
            } else if (grid[previous_position + 4 * x] == grid[y + 4 * x]) {
                grid[previous_position + 4 * x]++;
                grid[y + 4 * x] = 0;
                last_merge_position = previous_position;
            } else if (grid[previous_position + 4 * x] != grid[y + 4 * x] && previous_position - 1 != y) {
                grid[previous_position - 1 + 4 * x] = grid[y + 4 * x];
                grid[y + 4 * x] = 0;
            }

        }
    }
} 

/**
 * Method that get the new grid after movint left.
 *
 * @param grid
 * @return the new grid after moving left
 */
int* move_left(int *grid) {
    int *new_grid = clone_grid(grid);
    _rotate_right(new_grid);
    _rotate_right(new_grid);
    _move_right(new_grid);
    _rotate_right(new_grid);
    _rotate_right(new_grid);

    return new_grid;
}

/**
 * Method that get the new grid after movint up.
 *
 * @param grid
 * @return the new grid after moving up
 */
int* move_up(int *grid) {
    int *new_grid = clone_grid(grid);
    _rotate_right(new_grid);
    _move_right(new_grid);
    _rotate_right(new_grid);
    _rotate_right(new_grid);
    _rotate_right(new_grid);

    return new_grid;
}

/**
 * Method that get the new grid after movint right.
 *
 * @param grid
 * @return the new grid after moving right
 */
int* move_right(int *grid) {
    int *new_grid = clone_grid(grid);
    _move_right(new_grid);

    return new_grid;
}

/**
 * Method that get the new grid after movint down.
 *
 * @param grid
 * @return the new grid after moving down
 */
int* move_down(int *grid) {
    int *new_grid = clone_grid(grid);
    _rotate_right(new_grid);
    _rotate_right(new_grid);
    _rotate_right(new_grid);
    _move_right(new_grid);
    _rotate_right(new_grid);

    return new_grid;
}

/**
 * Method that get new grid of next step.
 *
 * @param grid
 * @param direction {0 : left, 1 : up, 2 : right, 3 : down}
 * @return the new grid
 */
int* move(int *grid, int direction) {
    switch (direction) {
    case 0: return move_left(grid);
            break;
    case 1: return move_up(grid);
            break;
    case 2: return move_right(grid);
            break;
    case 3: return move_down(grid);
            break;
    }
    return 0;
}

struct result {
    double score;
    int direction;
};

int same(int *g1, int *g2) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (g1[j + 4 * i] != g2[j + 4 * i]) return 0;
        }
    }
    return 1;
}

int num_empty(int *grid) {
    int num = 0;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            if (grid[j + 4 * i] == 0) num++;

    return num;
}

/**
 * Method that fill the empty positon of the grid by the given value.
 *
 * @param grid
 * @param index the index of the empty position
 * @param value the value that the empty positon will be filled
 * @return new_grid
 */
int *set_empty(int *grid, int index, int value) {
    int *new_grid = (int *) malloc(4 * 4 * sizeof(int));
    memset(new_grid, 0, 4 * 4 * sizeof(int));
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            if (grid[j + 4 * i] != 0) {
                *(new_grid + j + 4 * i) = grid[j + 4 * i];
                continue;
            }

            if (index == 0) *(new_grid + j + 4 * i) = value;
            else index--;
        }
    
    return new_grid;
}

int monotonicity(int *grid) {
    int totals[4];
    
    // up/down direction
    for (int x = 0; x < 4; x++) {
        int current = 0;
        int next = current + 1;

        while (next < 4) {
            while (next < 4 && grid[next + 4 * x] == 0) next++;

            if (next >= 4) next--;
            int current_value = grid[current + 4 * x];
            int next_value = grid[next + 4 * x];
            if (current_value > next_value) {
                totals[0] += next_value - current_value;
            } else if (next_value > current_value) {
                totals[1] += current_value - next_value;
            }
            current = next;
            next++;
        }
    }

    // left/right direction
    for (int y = 0; y < 4; y++) {
        int current = 0;
        int next = current + 1;

        while (next < 4) {
            while (next < 4 && grid[y + 4 * next] == 0) next++;

            if (next >= 4) next--;
            int current_value = grid[y + 4 * current];
            int next_value = grid[y + 4 * next];
            if (current_value > next_value) {
                totals[2] += next_value - current_value;
            } else if (next_value > current_value) {
                totals[3] += current_value - next_value;
            }
            current = next;
            next++;
        }
    }

    return max(totals[0], totals[1]) + max(totals[2], totals[3]);
}

/**
 * Calculates a heuristic variance-like score that measures how clustered the
 * board is.
 * 
 * @param grid
 * @return 
 */
int smoothness(int *grid) {
    int score = 0;
    int neighbors[] = {-1, 0, -1};
   
    for (int i = 0; i < 4; i++) 
        for (int j = 0; j < 4; j++) {
            if (grid[j + 4 * i] == 0) continue;

            for (int h = 0; h < 3; h++) {
                int k = neighbors[h];
                int x = i + k;

                if (x < 0 || x >= 4) continue;
        
                for (int v = 0; v < 3; v++) {
                    int l = neighbors[v];
                    int y = j + l;

                    if (y < 0 || y >= 4) continue;

                    if (grid[y + 4 * x] > 0) {
                        score -= fabs(grid[j + 4 * i] - grid[y + 4 * x]);
                    }                   
                }
            }
        }
        
    return score;
}

int max_value(int *grid) {
    int m = 0;
    for (int i = 0; i < 4; i++) 
        for (int j = 0; j < 4; j++)
            if (grid[j + 4 * i] > m) m = grid[j + 4 * i];
    
    return m;
}

double heuristic_score(int *grid) {
    int max_score = max_value(grid);
    int empty_num = num_empty(grid);
    int smooth_score = smoothness(grid);
    int mono_score = monotonicity(grid);

    double smooth_weight = 0.1;
    double mono_weight = 1.0;
    double empty_weight = 2.7;
    double max_weight = 1.0;

    double score = max_score * max_weight + empty_num * empty_weight + smooth_score * smooth_weight + mono_score * mono_weight;

    return score;
}

/**
 * The minimax method.
 *
 * @param grid
 * @param depth
 * @param player {0 : user, 1 : computer}
 * @return the result
 */
struct result minimax(int *grid, int depth, int player) {
    struct result r;
    double best_score;
    int best_direction = -1;

    if (depth == 0)
        best_score = heuristic_score(grid);
    else {
        if(player == 0) {
            best_score = MIN_VALUE;

            for (int direction = 0; direction < 4; direction++) {
                int *new_grid = move(grid, direction);
                if (same(new_grid, grid)) {
                    free(new_grid);
                    continue;
                }

                struct result current_r = minimax(new_grid, depth - 1, 1);
                if (current_r.score > best_score) {
                    best_score = current_r.score;
                    best_direction = direction;
                }
                free(new_grid);
            }
        } 
        else {
            best_score = MAX_VALUE;
            int empty_num= num_empty(grid);
            if (!empty_num) best_score = 0;

            int possible_values[] = {1, 2};
            
            for (int i = 0; i < empty_num; i++) {
                for (int j = 0; j < 2; j++) {
                    int *new_grid = set_empty(grid, i, possible_values[j]);
                    struct result current_r = minimax(new_grid, depth - 1, 0);
                    if (current_r.score < best_score) best_score = current_r.score;
                    free(new_grid);
                }
            }
        }
    }

    r.score = best_score;
    r.direction = best_direction;

    return r;
}


/**
 * The alphabeta method.
 *
 * @param grid
 * @param depth
 * @param alpha
 * @param beta
 * @param player {0 : user, 1 : computer}
 * @return the result
 */
struct result alphabeta(int *grid, int depth, double alpha, double beta, int player) {
    struct result r;
    //printf("statck = %d\n", ((char*)stack_org - (char*)&r));

    double best_score;
    int best_direction = -1;

    if (depth == 0)
        best_score = heuristic_score(grid);
    else {
        if(player == 0) {
            for (int direction = 0; direction < 4; direction++) {
                int *new_grid = move(grid, direction);
                if (same(new_grid, grid)) {
                    free(new_grid);
                    continue;
                }

                struct result current_r = alphabeta(new_grid, depth - 1, alpha, beta, 1);
                if (current_r.score > alpha) {
                    alpha = current_r.score;
                    best_direction = direction;
                }
                free(new_grid);
                
                if (beta <= alpha) break;
            }
           
            best_score = alpha;
        } 
        else {
            int empty_num= num_empty(grid);
            if (!empty_num) best_score = 0;

            int possible_values[] = {1, 2};
            
            for (int i = 0; i < empty_num; i++) {
                for (int j = 0; j < 2; j++) {
                    int *new_grid = set_empty(grid, i, possible_values[j]);
                    struct result current_r = alphabeta(new_grid, depth - 1, alpha, beta, 0);
                    if (current_r.score < beta) beta = current_r.score;
                    free(new_grid);
                    if (beta <= alpha) {
                        i = empty_num; // break;
                        break;
                    }
                }
            }

            best_score = beta;
        }
    }

    r.score = best_score;
    r.direction = best_direction;

    return r;
}

/**
 * Method that finds the best next move.
 *
 * @param grid
 * @return direction {0 : left, 1 : up, 2 : right, 3 : down}
 */
int find_best_move(int *grid, int depth) {
    //char pos = 0;
    //printf("stack %d\n", ((char*)stack_org - &pos));
    struct result r = alphabeta(grid, depth, MIN_VALUE, MAX_VALUE, 0);
    //printf("stack2 %d\n", ((char*)stack_org - &pos));
    int direction = r.direction == -1 ? (rand() % 4) : r.direction;
    int *grid_guess_tmp = move(grid, direction);
    if(!grid_guess_tmp) {
         printf("grid_guesss tmp null\n");
         return -1;
    }
    for (int i = 0; i < 16; i++) {
        grid_guess[i] = grid_guess_tmp[i];
    }
    free(grid_guess_tmp);

    return direction;
}

void print_grid(int *grid) {
    printf("\n——————————————————————————\n");
    for (int x = 0; x < 4; x++) {
        printf("|");
        for (int y = 0; y < 4; y++) {
            printf("%6d", grid[y + 4 * x] == 0 ? 0 : 1 << grid[y + 4 * x]);
        }
        printf("|\n");
    }
    printf("——————————————————————————\n");
}

void check(int *grid) {
    int num = 0;
    for (int i = 0; i < 4 * 4;  i++) {
        if (grid[i] != grid_guess[i]) {
            num++;
        }
    }
    if (num > 1) {
        printf("################################################");
        printf("grid\n");
        print_grid(grid);
        printf("grid_guess\n");
        print_grid(grid_guess);
        if (num_empty(grid_guess) != 16) exit(1);
    }
}

/**
 * Handle the image raw data.
 *
 * @param raw_data
 * @param width 
 * @param height
 * @param bytes_per_pixel
 * @return
 */
int handle_raw_data(char *raw_data, int width, int height, int bytes_per_pixel) {
    color_bg = get_rgb(raw_data, width, height, bytes_per_pixel, 0, height - 1);

    int top = 0;
    int left = 0;
    int right = width;
    int bottom = 0;

    for (int h = height - 1; h > 0; h--) {
        if (color_bg == get_rgb(raw_data, width, height, bytes_per_pixel, 0, h)) continue;
        top = ++h;
        break;
    }
    top += 5;
    bottom = top + width;    

    int square_len = width / 4;
    int grid_current[4 * 4] = {0};
    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 4; y++) {
            int id = get_square_id(raw_data, width, height, bytes_per_pixel, left + y * square_len, top + square_len * x, left + (y + 1) * square_len, top + (x + 1) * square_len);
           // old solution 

/*
           for (int i = 0; i < 20; i++) {
               if (grid_ids[i] != 0) {
                   if (id == grid_ids[i]) {
                       grid_current[y + 4 * x] = i;
                       break;
                   } 
               } else {
                   grid_ids[i] = id;
                   grid_current[y + 4 * x] = i;
                   break;
               }
           }
*/

           

           // new solution

           if (grid_guess[y + 4 * x] != 0) grid_current[y + 4 * x] = grid_guess[y + 4 * x]; 
           else {
               if (id == special_ids[0]) {
                   grid_current[y + 4 * x] = 0;
               } else if (id == special_ids[1]){
                   grid_current[y + 4 * x] = 1;
               } else {
                   grid_current[y + 4 * x] = 2;
               }
           }


        }
    }

/*
    int test[] = {4, 7, 9, 10, 3, 5, 7, 8, 1, 4, 6, 3, 1, 3, 2, 0};
    //int test[16] = {4, 3, 4, 1, 3, 3, 7, 2, 1, 4, 6, 3, 1, 3, 1, 0};
    printf("test len = %d, test=%p, grid=%p\n", sizeof(test)/sizeof(test[0]), test, grid_current);
    for (int i = 0; i < 16; i++) {
        grid_current[i] = test[i];
    }
    printf("finished copying\n");
*/
    print_grid(grid_current);
    
    
    //check(grid_current);

    int direction = find_best_move(grid_current, 6);

    switch (direction) {
    case 0: printf("\nswipe left\n");
            swipe_left();
            break;
    case 1: printf("\nswipe up\n");
            swipe_up();
            break;
    case 2: printf("\nswipe right\n");
            swipe_right();
            break;
    case 3: printf("\nswipe down\n");
            swipe_down();
            break;
    default: printf("\n?\n");
    }

    return 0;
}

static int64_t microSecondOfNow() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return ((int64_t) t.tv_sec) * (1000 * 1000) + t.tv_usec;
}


static void on_SIGPIPE(int signum) {
    LOG("pipe peer ended first, no problem");
    exit(1);
}

// hack android OS head file
#if defined(TARGET_ICS) || defined(TARGET_JB) //{
namespace android {

template <typename T> class sp {
public:
    union{
        T* m_ptr;
        char data[128];
    };
};

class IBinder;

class ScreenshotClient {
    /*
    sp<IMemoryHeap> mHeap;
    uint32_t mWidth;
    uint32_t mHeight;
    PixelFormat mFormat;
    */
    char data[1024]; //android 4.2 embed CpuConsumer::LockedBuffer here which cause more space
public:
    ScreenshotClient();

#if defined(TARGET_ICS)
    // frees the previous screenshot and capture a new one
    int32_t update();
#endif
#if defined(TARGET_JB)
    // frees the previous screenshot and capture a new one
    int32_t update(const sp<IBinder>& display);
#endif
    // pixels are valid until this object is freed or
    // release() or update() is called
    void const* getPixels() const;

    uint32_t getWidth() const;
    uint32_t getHeight() const;
    uint32_t getStride() const; //base + getStride()*bytesPerPixel will get start address of next row
    int32_t getFormat() const;
    // size of allocated memory in bytes
    size_t getSize() const;
};

#if defined(TARGET_JB)
class SurfaceComposerClient {
public:
    //! Get the token for the existing default displays.
    //! Possible values for id are eDisplayIdMain and eDisplayIdHdmi.
    static sp<IBinder> getBuiltInDisplay(int32_t id);
};
#endif

class ProcessState {
    char data[1024]; //please adjust this value when you copy this definition to your real source!!!!!!!!!!!!!!!!!!!!!!!
public:
    static sp<ProcessState> self();
    void startThreadPool();
};

} //end of namespace android

using android::ScreenshotClient;
using android::sp;
using android::IBinder;
#if defined(TARGET_JB)
using android::SurfaceComposerClient;
#endif
using android::ProcessState;

#endif //} end of "if defined(TARGET_ICS) || defined(TARGET_JB)"


int main(int argc, char** argv) {
    LOG("start. pid %d", getpid());
    int64_t interval_mms = -1;
    stack_org = &interval_mms;

    bool isGetFormat = false;
    bool forceUseFbFormat = false;
    const char* tmps;
    int width, height, internal_width, bytesPerPixel;

    //for fb0
    int fb = -1;
    char* mapbase = NULL;
    size_t lastMapSize = 0;

    if (argc>1) {
        double fps = atof(argv[1]);
        if (fps==0) {
            //
        }
        else {
            interval_mms = ((double)1000000)/fps;
            LOG("use fps=%.3lf (interval=%.3lfms)", fps, (double)interval_mms/1000);
        }
    } else {
        isGetFormat = true;
    }

    if (isGetFormat && (tmps=getenv("forceUseFbFormat")) && 0==strcmp(tmps, "forceUseFbFormat")) {
        LOG("forceUseFbFormat");
        forceUseFbFormat = true;
    }

#if defined(TARGET_JB) || defined(TARGET_ICS)
    LOG("call ProcessState::self()->startThreadPool()");
    ProcessState::self().m_ptr->startThreadPool();
    LOG("call ScreenshotClient init");
    ScreenshotClient screenshot;
#endif

#if defined(TARGET_JB)
    LOG("call SurfaceComposerClient::getBuiltInDisplay");
    sp<IBinder> display = SurfaceComposerClient::getBuiltInDisplay(0 /*1 is hdmi*/);
    if (display.m_ptr==NULL)
        LOGERR("failed to getBuiltInDisplay. So use fb0");
#endif

    //LOG(isGetFormat ? "capture once" : "start capture");
    int64_t count_start_mms = microSecondOfNow();
    int64_t until_mms = count_start_mms + interval_mms;

    for (int count=1; ;count++, until_mms += interval_mms) {

        char* rawImageData;
        size_t rawImageSize;

#if defined(TARGET_JB) || defined(TARGET_ICS)
        bool surfaceOK = false;
        uint32_t status;
#endif

#if defined(TARGET_JB)
        if (!forceUseFbFormat) {
            if (display.m_ptr != NULL) {
                if (count==1) LOG("call ScreenshotClient.update(mainDisplay)");
                status = screenshot.update(display);
                surfaceOK = (status == 0);
                if (!surfaceOK)
                    LOG("Error: failed to ScreenshotClient.update(mainDisplay). Result:%d. So use fb0 alternatively, maybe not useful", status);
            }
        }
#endif
#if defined(TARGET_ICS)
        if (!forceUseFbFormat) {
            if (count==1) LOG("call ScreenshotClient.update()");
            status = screenshot.update();
            surfaceOK = (status == 0);
            if (!surfaceOK)
                LOG("Error: failed to ScreenshotClient.update(). Result:%d. So use fb0 alternatively, maybe not useful", status);
        }
#endif
#if defined(TARGET_JB) || defined(TARGET_ICS)
        if (surfaceOK) {
            rawImageData = (char*)screenshot.getPixels();
            rawImageSize = screenshot.getSize();
            width = screenshot.getWidth();
            height = screenshot.getHeight();
            internal_width = screenshot.getStride();
            bytesPerPixel = rawImageSize/internal_width/height;

            int fmt = screenshot.getFormat();
            if (count==1) {
                LOG("ScreenshotClient.update result: imageSize:%d w:%d h:%d W:%d bytesPerPixel:%d fmt:%d",
                 rawImageSize, width, height, internal_width, bytesPerPixel, fmt);
            }

            if (isGetFormat) {
                printf("-s %dx%d -pix_fmt %s\n", width, height,
                    (bytesPerPixel==4) ? "rgb0" :
                    (bytesPerPixel==3) ? "rgb24" :
                    (bytesPerPixel==2) ? "rgb565le" :
                    (bytesPerPixel==5) ? "rgb48le" :
                    (bytesPerPixel==6) ? "rgba64le" :
                    (LOG("strange bytesPerPixel:%d", bytesPerPixel),"unknown"));
                LOG("end");
                return 0;
            }
        } else
#endif
        {
            if (fb < 0) {
                fb = open(FRAME_BUFFER_DEV, O_RDONLY);
                if (fb < 0)
                    ABORT("open fb0");
            }

            struct fb_var_screeninfo vinfo;
            if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) < 0)
                ABORT("ioctl fb0");

            width = vinfo.xres;
            height = vinfo.yres;
            internal_width = width;
            bytesPerPixel = vinfo.bits_per_pixel/8;
            rawImageSize = (width*height) * bytesPerPixel;

            if (count==1) {
                LOG("FBIOGET_VSCREENINFO result: imageSize:%d w:%d h:%d bytesPerPixel:%d virtualW:%d virtualH:%d"
                    " bits:%d"
                    " R:(offset:%d length:%d msb_right:%d)"
                    " G:(offset:%d length:%d msb_right:%d)"
                    " B:(offset:%d length:%d msb_right:%d)"
                    " A:(offset:%d length:%d msb_right:%d)"
                    " grayscale:%d nonstd:%d rotate:%d",
                    rawImageSize, width, height, bytesPerPixel, vinfo.xres_virtual, vinfo.yres_virtual
                    ,vinfo.bits_per_pixel
                    ,vinfo.red.offset, vinfo.red.length, vinfo.red.msb_right
                    ,vinfo.green.offset, vinfo.green.length, vinfo.green.msb_right
                    ,vinfo.blue.offset, vinfo.blue.length, vinfo.blue.msb_right
                    ,vinfo.transp.offset, vinfo.transp.length, vinfo.transp.msb_right
                    ,vinfo.grayscale, vinfo.nonstd, vinfo.rotate
                    );
            }

            if (isGetFormat) {
                printf("-s %dx%d -pix_fmt %s\n", width, height,
                    (vinfo.bits_per_pixel==32&&vinfo.red.offset==0) ? "rgb0" :
                    (vinfo.bits_per_pixel==32&&vinfo.red.offset!=0) ? "bgr0" :
                    (vinfo.bits_per_pixel==24&&vinfo.red.offset==0) ? "rgb24" :
                    (vinfo.bits_per_pixel==24&&vinfo.red.offset!=0) ? "bgr24" :
                    (vinfo.bits_per_pixel==16&&vinfo.red.offset==0) ? "rgb565le" :
                    (vinfo.bits_per_pixel==16&&vinfo.red.offset!=0) ? "bgr565le" :
                    (vinfo.bits_per_pixel==48&&vinfo.red.offset==0) ? "rgb48le" :
                    (vinfo.bits_per_pixel==48&&vinfo.red.offset!=0) ? "bgr48le" :
                    (vinfo.bits_per_pixel==64&&vinfo.red.offset==0) ? "rgba64le" :
                    (vinfo.bits_per_pixel==64&&vinfo.red.offset!=0) ? "bgra64le" :
                    (LOG("strange bits_per_pixel:%d", vinfo.bits_per_pixel),"unknown"));
                LOG("end");
                return 0;
            }
            else {
                uint32_t offset =  (vinfo.xoffset + vinfo.yoffset*width) *bytesPerPixel;
                int virtualSize = vinfo.xres_virtual*vinfo.yres_virtual*bytesPerPixel;
                if (offset+rawImageSize > virtualSize) {
                    LOG("Strange! offset:%d+rawImageSize:%d > virtualSize:%d", offset, rawImageSize, virtualSize);
                    virtualSize = offset+rawImageSize;
                }

                if (virtualSize > lastMapSize) {
                    if (mapbase) {
                        LOG("remap due to virtualSize %d is bigger than previous %d", virtualSize, lastMapSize);
                        munmap(mapbase, lastMapSize);
                        mapbase = NULL;
                    }
                    lastMapSize = virtualSize;
                }

                if (mapbase==NULL) {
                    mapbase = (char*)mmap(0, virtualSize, PROT_READ, MAP_PRIVATE, fb, 0);
                    if (mapbase==NULL)
                        ABORT("mmap %d", virtualSize);
                }


                rawImageData = mapbase + offset;
            }
        }

        if (count==1) { //when first time, set SIGPIPE handler to default (terminate)
            signal(SIGPIPE, on_SIGPIPE); //this is very important!!! If not set, write will be very slow if data is too big
            LOG("rawImageSize:%d", rawImageSize);
        }

        char* base = rawImageData;

        handle_raw_data(base, width, height, bytesPerPixel);

        for (int y=0; y < height && 0; y++) {
            size_t sizeOfRow = width*bytesPerPixel;

            #define MAX_WRITE_SIZE (4*1024*1024)
            int rest = sizeOfRow;
            int callCount = 0;
            while (rest > 0) {
                int request = rest <= MAX_WRITE_SIZE ? rest : MAX_WRITE_SIZE;
                if (callCount > 0 ||request < rest) LOG("data is too big so try to write %d of rest %d", request, rest);
                int bytesWritten = write(STDOUT_FILENO, base+(sizeOfRow-rest), request);
                if (bytesWritten < 0) {
                    ABORT("write() requested:%d", request);
                } else if (bytesWritten < request) {
                    LOGERR("write() result:%d < requested:%d. Continue writing rest data", bytesWritten, request);
                } else {
//                  if (callCount > 0) LOG("write %d OK", request);
                }
                rest -= bytesWritten;
                callCount++;
            }
            if (callCount > 1) LOG("write() finished. total:%d", sizeOfRow);

            base += internal_width*bytesPerPixel;
        }

        if (interval_mms==-1) {
            LOG("stop due to fps argument is 0");
            close(STDOUT_FILENO); //let pipe peer known end, maybe unnecessary
            exit(0);
        }
        else {
            if (count==1) LOG("continue capturing......");
        }

        int64_t now_mms = microSecondOfNow();
        int64_t diff_mms = until_mms - now_mms;
        if (diff_mms > 0) {
            usleep(diff_mms);
            now_mms += diff_mms;
        }

        /*
        //show statistics at every about 10 seconds
        diff_mms = now_mms-count_start_mms;
        if (diff_mms >= 10*1000000) {
            //LOG("count: %d now-count_start_ms: %lld", count, diff_mms);
            LOG("raw fps: %.2lf   ", ((double)count) / (((double)diff_mms)/1000000));
            count_start_mms = now_mms;
            count = 0;
        }
        */
    }

    return 0;
}

