#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <openssl/evp.h>
//#include <openssl/err.h>

#define MAX_THREADS 1024
#define BUF_SIZE 1024
static long n_cores;
static unsigned char flag = 0;
pthread_mutex_t output_mutex;

//structure to hold task details (task pool)
struct thd_task_pool {
    void* (*routine_cb)(void*);
    char **filename;
    int total_done;
};

//structure to hold thread details (thread pool)
struct thd_thread_pool {
    pthread_t t[MAX_THREADS];
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

// variable to task pool access
static struct thd_task_pool *gtask_pool;

// variable to thread pool access
static struct thd_thread_pool *gthd_pool;


static char *lsperms(int mode)
{
    static const char *rwx[] = {"---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx"};
    static char bits[11];

    if (S_ISDIR(mode))
        bits[0] = 'd';
    else if (S_ISCHR(mode))
        bits[0] = 'c';
    else if (S_ISBLK(mode))
        bits[0] = 'b';
    else if (S_ISFIFO(mode))
        bits[0] = 'p';
    else if (S_ISLNK(mode))
        bits[0] = 'l';
    else if (S_ISSOCK(mode))
        bits[0] = 's';
    else
        bits[0] = '-';

    strcpy(&bits[1], rwx[(mode >> 6)& 7]);
    strcpy(&bits[4], rwx[(mode >> 3)& 7]);
    strcpy(&bits[7], rwx[(mode & 7)]);

    if (mode & S_ISUID)
        bits[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID)
        bits[6] = (mode & S_IXGRP) ? 's' : 'l';
    if (mode & S_ISVTX)
        bits[9] = (mode & S_IXOTH) ? 't' : 'T';
    bits[10] = '\0';
    return(bits);
}

void *sha256stat(void *filename) {

    FILE *f;
    size_t len;
    struct stat fileStat1;
    struct stat fileStat2;
    unsigned char buffer[BUFSIZ];  // BUFSIZ=8192

    if (lstat(filename,&fileStat1) < 0) {
        fprintf(stderr, "no such file or directory: %s\n", filename);
        return NULL;
    }

    fileStat2=fileStat1;

    if S_ISLNK(fileStat1.st_mode)
        stat(filename,&fileStat1);

    if S_ISREG(fileStat1.st_mode) {
        f = fopen(filename, "r");

        if (!f) {
                fprintf(stderr, "couldn't open file: %s\n", filename);
                return NULL;
        }

        EVP_MD_CTX *hashctx;             
		hashctx = EVP_MD_CTX_new();
		if (hashctx == NULL)
		{
			printf("\nError - EVP_MD_CTX \n");
			return NULL;
		}
        const EVP_MD *hashptr = EVP_get_digestbyname("SHA256");
        EVP_MD_CTX_init(hashctx);
        EVP_DigestInit_ex(hashctx, hashptr, NULL);

        do {
                len = fread(buffer, 1, BUFSIZ, f);
                EVP_DigestUpdate(hashctx, buffer, len);
        } while (len == BUFSIZ);

        unsigned int outlen;
        EVP_DigestFinal_ex(hashctx, buffer, &outlen);
	EVP_MD_CTX_reset(hashctx);       
		EVP_MD_CTX_free(hashctx); 
		hashctx = NULL;           

        pthread_mutex_lock(&output_mutex);
        int i;

        for (i = 0; i < outlen; i++)
                printf("%02x", buffer[i]);

        fclose(f);
    }
    else if S_ISLNK(fileStat1.st_mode) {
        pthread_mutex_lock(&output_mutex);
        printf("!!!_______________________BROKEN__LINK_______________________!!!");
    }
    else {
        pthread_mutex_lock(&output_mutex);
        printf("-                                                               ");
    }

    printf(" %s",lsperms(fileStat2.st_mode));

    printf(" %d",fileStat2.st_nlink);

    struct group *grp;
    struct passwd *pwd;

    pwd = getpwuid(fileStat2.st_uid);
    if (pwd != NULL)
        printf(" %s", pwd->pw_name);
    else
        printf(" %d", fileStat2.st_uid);

    grp = getgrgid(fileStat2.st_gid);
    if (grp != NULL)
        printf(" %s", grp->gr_name);
    else
        printf(" %d", fileStat2.st_gid);

    setlocale(LC_NUMERIC, "");
    printf(" %'jd", (intmax_t)fileStat2.st_size);

    if (flag==0) {
        struct tm *tm;
        tm = localtime(&fileStat2.st_mtime);
        strftime(buffer, 17, "%Y-%m-%d %H:%M", tm);
        printf(" %s", buffer);
    }

    ssize_t l;
    if ((l = readlink(filename, buffer, sizeof(buffer)-1)) != -1) {
        buffer[l] = '\0';
        printf(" %s -> %s\n", filename, buffer);
    }
    else
        printf(" %s\n", filename);

    pthread_mutex_unlock(&output_mutex);
}


//function to free memory for thread pool
void pthread_destroy() {
    //release memory of thread pool
    pthread_mutex_destroy(&output_mutex);
    pthread_mutex_destroy(&gthd_pool->mutex);
    free(gthd_pool);
    gthd_pool = NULL;
}

//function to free memory for task pool
void task_destroy() {
    //de-assign filename pointer (assign from argv)
    gtask_pool->filename = NULL;
    //release memory of task pool
    free(gtask_pool);
    gtask_pool = NULL;
}

//funtion to initialize task details
void pthread_task_init(int num_task,char **fname) {
    //allocate memory to store task details
    if ((gtask_pool = (struct thd_task_pool *)malloc(sizeof(struct thd_task_pool))) == NULL) 
    {
    // why is this block empty ???????
    }

    //store number of task that is decremented when task completes
    gtask_pool->total_done = num_task;
    //filename to be executed
    gtask_pool->filename = fname;
    //routine/task to be executed
    gtask_pool->routine_cb = sha256stat;
}

//function executed by threads
void *pthread_thread_start(void *arg) {

    char *l_filename = NULL;
    pthread_t id;

    //check if all task is done
    for (;;) {
        //lock the execution of getting task (same task should not be taken by other thread)
        pthread_mutex_lock(&gthd_pool->mutex);

        if (gtask_pool->total_done > 0) {
            //get filename to be processed
            l_filename = gtask_pool->filename[gtask_pool->total_done-1];
            //decrement task
            gtask_pool->total_done--;
        } // task complete. Go for next task in this thread
        else {
            pthread_mutex_unlock(&gthd_pool->mutex);
            break;
        }

        //unlock the process of getting task      
        pthread_mutex_unlock(&gthd_pool->mutex);

        //execute routine/task
        gtask_pool->routine_cb((void *)l_filename);
    }

    //get thread id. All task is done
    id = pthread_self();

    //exit thread
    pthread_exit(&id);
    return NULL;
}

//function to initalize thread pool and execute threads
void thread_init() {

    int i = 0;
    void *p = NULL;

    //allocate memory to store thread details
    if ((gthd_pool = (struct thd_thread_pool *)malloc(sizeof(struct thd_thread_pool) * n_cores)) == NULL) 
    {
    // why is this block empty ???????
    }

    //initalize thread mutex
    pthread_mutex_init(&(gthd_pool->mutex), NULL);
    pthread_mutex_init(&(output_mutex), NULL);

    //create number of threads
    for (i=0; i<n_cores; i++) {
        if (pthread_create(&gthd_pool->t[i], NULL, &pthread_thread_start, (void *) p)) {            
            return;
        }     
    }

    //wait for all threads to be finished
    for (i=0; i<n_cores; i++) {
        pthread_join(gthd_pool->t[i], NULL);
    }
}

int main(int argc, char **argv) {

    struct stat sb;
    char buffer[BUF_SIZE] = {'\0'};
    char *content = NULL;
    char **doublebuff = NULL;
    size_t contentSize = 1; // includes NULL
    int i = 0;
    int j = 0;
    int k = 0;
    int l_nlen = 0;
    char l_cSeperator = '\n';
    char ch;

    // get number of cores
    n_cores = sysconf(_SC_NPROCESSORS_ONLN);

    if (n_cores>MAX_THREADS)
        n_cores=MAX_THREADS;

    int c;
    int l_nActualArgCount = 0;

    OpenSSL_add_all_algorithms();

    if (isatty(STDIN_FILENO)) {
        while ((c = getopt(argc, argv, "c")) != EOF) switch(c) {
                case 'c':
                    flag |= 1;
                    break;
                case '?':
                    fprintf(stderr, "\n usage: %s [-c] <file>\n", argv[0]);
                    fprintf(stderr, "\n        find <DIR> | %s [-c]\n\n", argv[0]);
                    return 1;
            }

            l_nActualArgCount = argc - optind;
            //initialize task pool
            pthread_task_init(l_nActualArgCount,argv + optind);

            if (l_nActualArgCount < 1) {
                fprintf(stderr, "\n usage: %s [-c] <file>\n", argv[0]);
                fprintf(stderr, "\n        find <DIR> | %s [-c]\n\n", argv[0]);
                return 1;
            }
    }
    else {
        if (fstat(STDIN_FILENO, &sb) == 0) {
            if (S_ISFIFO(sb.st_mode)) {
                while ((c = getopt(argc, argv, "c0")) != EOF) switch(c) {
                    case '0':
                        l_cSeperator = '\0';
                        break;
                    case 'c':
                        flag |= 1;
                        break;
                    case '?':
                        fprintf(stderr, "\n usage: <stream> | %s [-c] [-0]\n\n", argv[0]);
                        return 1;
                }
                content = (char *)malloc(sizeof(char) * BUF_SIZE);
                if (content == NULL) {
                    printf("Failed to allocate content");
                    return 0;
                }
                content[0] = '\0'; // make null-terminated

                if (l_cSeperator != '\n') {
                    while ((ch = fgetc(stdin)) != EOF) {
                        content = realloc(content, contentSize);
                        if (content == NULL) {
                            perror("Failed to reallocate content");
                            return 0;
                        }
                        content[contentSize - 1] = ch;
                        contentSize++;
                    }
                    char *l_contentdata = content;
                    for (j = 0; j < contentSize; j++) {
                        if (l_contentdata[j] != l_cSeperator) {
                            buffer[k] = l_contentdata[j];
                            k++;
                        } else {
                            buffer[k] = '\0';
                            l_nlen = strlen(buffer); 
                            if ( l_nlen > 0) {
                                doublebuff = (char**)realloc(doublebuff,(sizeof(char*) * (i+1)));
                                doublebuff[i] = (char*)malloc( (l_nlen * sizeof(char)) + 1);
                                memset(doublebuff[i],'\0',(l_nlen  * sizeof(char)) + 1);
                                strcpy(doublebuff[i],buffer);
                                i++;
                            }
                            memset(&buffer,'\0',sizeof(buffer));
                            k = 0;
                        }
                    }
                } else {
                    for (;;) {
                        size_t bytes = fread(buffer,1023,1,stdin);
                        char *old = content;

                        contentSize += strlen(buffer);
                        content = realloc(content, contentSize);
                        if (content == NULL) {
                            perror("Failed to reallocate content");
                            free(old);
                            return 0;
                        }
                        strcat(content, buffer);
                        memset(&buffer,'\0',sizeof(buffer));
                        if (bytes < BUF_SIZE)
                            if (feof(stdin)) {
                                break;
                            }
                    }

                    char *l_contentdata = content;
                    for (j = 0; j < contentSize; j++) {
                        if (l_contentdata[j] != l_cSeperator) {
                            if (l_contentdata[j] != ' ') {
                                buffer[k] = l_contentdata[j];
                                k++;
                            }
                        } else {
                            buffer[k] = '\0';
                            l_nlen = strlen(buffer); 
                            if ( l_nlen > 0) {
                                doublebuff = (char**)realloc(doublebuff,(sizeof(char*) * (i+1)));
                                doublebuff[i] = (char*)malloc( (l_nlen * sizeof(char)) + 1);
                                memset(doublebuff[i],'\0',(l_nlen  * sizeof(char)) + 1);
                                strcpy(doublebuff[i],buffer);
                                i++;
                            }
                            memset(&buffer,'\0',sizeof(buffer));
                            k = 0;
                        }
                    }
                }
                l_nActualArgCount = i;
                //initialize task pool
                pthread_task_init(l_nActualArgCount,doublebuff);
            }
        }    
    }
    //initialize thread pool and execute threads
    thread_init();

    //clear task pool
    task_destroy();

    //clear thread pool
    pthread_destroy();

    for (j=0;j<i;j++) {
        free(doublebuff[j]);
        doublebuff[j] = NULL;
    }

    if (doublebuff != NULL) {
        free(doublebuff);
        doublebuff = NULL;
    }

    return 0;
}
