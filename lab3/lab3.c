#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char path[4096];
    unsigned char hash[32];
    ino_t inode;
    dev_t dev;
} FileData;

FileData allFiles[100000];
size_t nFiles = 0;

void scanDir(const char* path) {
    DIR* d = opendir(path);
    if (!d) return;

    struct dirent* ent;
    while ((ent = readdir(d))) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);

        struct stat st;
        lstat(full, &st);
        
        if (!S_ISREG(st.st_mode)) 
            continue;
        
        if (nFiles < 100000) {
            FileData* f = &allFiles[nFiles++];
            strncpy(f->path, full, sizeof(f->path)-1);
            f->inode = st.st_ino;
            f->dev = st.st_dev;
        }
    }
    closedir(d);
}

void calcHash(const char* fname, unsigned char* out) {
    FILE* fp = fopen(fname, "rb");
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        SHA256_Update(&ctx, buf, n);
    SHA256_Final(out, &ctx);
    fclose(fp);
}

int cmpHash(const void* a, const void* b) {
    return memcmp(((FileData*)a)->hash, ((FileData*)b)->hash, 32);
}

int main(int argc, char** argv) {
    scanDir(argv[1]);

    for (size_t i = 0; i < nFiles; i++)
        calcHash(allFiles[i].path, allFiles[i].hash);

    qsort(allFiles, nFiles, sizeof(FileData), cmpHash);

    for (size_t i = 0; i < nFiles; ) {
        size_t j = i + 1;
        while (j < nFiles && 
               memcmp(allFiles[i].hash, allFiles[j].hash, 32) == 0)
            j++;
        
        for (size_t k = i + 1; k < j; k++) {
            if (allFiles[i].dev == allFiles[k].dev && 
                allFiles[i].inode != allFiles[k].inode) {
                unlink(allFiles[k].path);
                link(allFiles[i].path, allFiles[k].path);
            }
        }
        i = j;
    }

    return 0;
}