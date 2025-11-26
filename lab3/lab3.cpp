#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <vector>
#include <algorithm>

typedef struct {
    char path[1024];
    unsigned char hash[SHA_DIGEST_LENGTH];
    ino_t inode;
    dev_t device;
} FileEntry;

void scan_directory(const char* dir_path, std::vector<FileEntry>& files) {
    DIR* dir = opendir(dir_path);
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        char full_path[1024];
        snprintf(full_path, 1024, "%s/%s", dir_path, entry->d_name);
        
        struct stat file_stat;
        if (lstat(full_path, &file_stat) != 0) continue;
        
        if (S_ISLNK(file_stat.st_mode)) continue;
        
        if (S_ISDIR(file_stat.st_mode)) {
            scan_directory(full_path, files);
        } 
        else if (S_ISREG(file_stat.st_mode)) {
            FileEntry fe;
            strncpy(fe.path, full_path, 1023);
            fe.path[1023] = '\0';
            fe.inode = file_stat.st_ino;
            fe.device = file_stat.st_dev;
            files.push_back(fe);
        }
    }
    closedir(dir);
}

void calculate_sha1(const char* filename, unsigned char* hash) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        SHA1((const unsigned char*)"", 0, hash);
        return;
    }

    SHA_CTX sha_context;
    SHA1_Init(&sha_context);
    
    unsigned char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        SHA1_Update(&sha_context, buffer, bytes_read);
    }
    
    fclose(file);
    SHA1_Final(hash, &sha_context);
}

bool compare_hashes(const FileEntry& a, const FileEntry& b) {
    return memcmp(a.hash, b.hash, SHA_DIGEST_LENGTH) < 0;
}

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    
    std::vector<FileEntry> files;
    scan_directory(argv[1], files);
    int file_count = files.size();
    
    for (int i = 0; i < file_count; i++) {
        calculate_sha1(files[i].path, files[i].hash);
    }
    
    std::sort(files.begin(), files.end(), compare_hashes);
    
    for (int i = 0; i < file_count; i++) {
        int j = i + 1;
        while (j < file_count && !memcmp(files[i].hash, files[j].hash, SHA_DIGEST_LENGTH)) {
            if (files[i].device == files[j].device) {
                if (link(files[i].path, files[j].path) == 0) {
                    unlink(files[j].path);
                }
            }
            j++;
        }
        i = j - 1;
    }
    
    return 0;
}