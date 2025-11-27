#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <vector>
#include <algorithm>

const size_t HASH_SIZE = 32;      // SHA-256
const size_t MAX_PATH_LEN = 2048;

struct FileEntry {
    char path[MAX_PATH_LEN];
    unsigned char hash[HASH_SIZE];
    ino_t inode;
    dev_t device;
};

void scan_directory(const char* dir_path, std::vector<FileEntry>& files) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        perror(dir_path);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[MAX_PATH_LEN];
        if (snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name)
            >= (int)sizeof(full_path)) {
            fprintf(stderr, "Path too long, skipping.\n");
            continue;
        }

        struct stat st;
        if (lstat(full_path, &st) != 0) {
            perror(full_path);
            continue;
        }

        if (S_ISLNK(st.st_mode)) continue;

        if (S_ISDIR(st.st_mode)) {
            scan_directory(full_path, files);
        }
        else if (S_ISREG(st.st_mode)) {
            FileEntry fe;
            memset(&fe, 0, sizeof(fe));
            strncpy(fe.path, full_path, sizeof(fe.path)-1);
            fe.inode = st.st_ino;
            fe.device = st.st_dev;
            files.push_back(fe);
        }
    }
    closedir(dir);
}

void calculate_hash(const char* filename, unsigned char* hash_out) {
    memset(hash_out, 0, HASH_SIZE);

    FILE* f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        return;
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        fprintf(stderr, "OpenSSL error: can't create ctx\n");
        fclose(f);
        return;
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        fprintf(stderr, "DigestInit failed\n");
        EVP_MD_CTX_free(ctx);
        fclose(f);
        return;
    }

    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (EVP_DigestUpdate(ctx, buf, n) != 1) {
            fprintf(stderr, "DigestUpdate failed\n");
            EVP_MD_CTX_free(ctx);
            fclose(f);
            return;
        }
    }

    unsigned int hl = 0;
    if (EVP_DigestFinal_ex(ctx, hash_out, &hl) != 1) {
        fprintf(stderr, "DigestFinal failed\n");
    }

    EVP_MD_CTX_free(ctx);
    fclose(f);
}

bool hash_compare(const FileEntry& a, const FileEntry& b) {
    int cmp = memcmp(a.hash, b.hash, HASH_SIZE);
    return (cmp == 0) ? strcmp(a.path, b.path) < 0 : cmp < 0;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <directory>\n", argv[0]);
        return 1;
    }

    std::vector<FileEntry> files;
    scan_directory(argv[1], files);

    for (auto& f : files)
        calculate_hash(f.path, f.hash);

    std::sort(files.begin(), files.end(), hash_compare);

    for (size_t i = 0; i < files.size(); ) {
        size_t j = i + 1;

        while (j < files.size() &&
               memcmp(files[i].hash, files[j].hash, HASH_SIZE) == 0) {

            if (files[i].device == files[j].device &&
                files[i].inode  != files[j].inode) {

                if (unlink(files[j].path) != 0) {
                    perror(files[j].path);
                } else {
                    if (link(files[i].path, files[j].path) != 0) {
                        perror("link");
                    } else {
                        struct stat st;
                        if (stat(files[j].path, &st) == 0) {
                            files[j].inode = st.st_ino;
                            files[j].device = st.st_dev;
                        }
                    }
                }
            }

            j++;
        }

        i = j;
    }

    return 0;
}
