#include "database.h"

ms mstime() {
    struct timeval curr;
    gettimeofday(&curr, NULL);
    return curr.tv_sec*1000 + curr.tv_usec/1000;
}

size_t hash(char* str, int str_len, size_t max) {
    size_t ret = 0;
    for (int i = 0; i < str_len; i++)
        ret = (ret << 5) ^ (ret + str[i]);
    return ret % max;
}

int db_init(struct database *this, int size) {
    this->size = size;
    this->used = 0;
    this->entries = calloc(size, sizeof(struct entry));
    if (!this->entries) {
        printf("calloc failed\n");
        return -1;
    }
    return 0;
}

int db_update(struct database *this, char *key, int key_len, char *val, int val_len, ms expiry) {
    if (this->used == this->size) {
        printf("db_update failed: database is full\n");
        return -1;
    }
    size_t idx = hash(key, key_len, this->size);
    while (
        this->entries[idx].occupied &&
        (
            key_len != this->entries[idx].key_len ||
            strncmp(this->entries[idx].key, key, key_len)
        )
    )
        idx = (idx+1) % this->size;
    this->entries[idx].occupied = 1;
    this->entries[idx].expiry = expiry;
    this->entries[idx].key_len = key_len;
    this->entries[idx].val_len = val_len;
    this->entries[idx].key = malloc(key_len);
    memcpy(this->entries[idx].key, key, key_len);
    this->entries[idx].val = malloc(val_len);
    memcpy(this->entries[idx].val, val, val_len);
    ++this->used;
    return idx;
}

int db_query(struct database *this, char *key, int key_len, char *val, int val_len) {
    size_t idx = hash(key, key_len, this->size);
    size_t stop = idx;
    bool ok = 1;
    while (
        !this->entries[idx].occupied ||
        this->entries[idx].key_len != key_len || 
        strncmp(this->entries[idx].key, key, key_len)
    ) {
        idx = (idx+1) % this->size;
        if (idx == stop) {
            ok = 0;
            break;
        }
    }
    if (ok && mstime() > this->entries[idx].expiry) {
        this->entries[idx].occupied = 0;
        free(this->entries[idx].key);
        free(this->entries[idx].val);
        ok = 0;
    }
    if (!ok) {
        printf("Key %.*s not found or has been expired\n", key_len, key);
        return -1;
    }
    if (val_len > this->entries[idx].val_len)
        val_len = this->entries[idx].val_len;
    memcpy(val, this->entries[idx].val, val_len);
    return val_len;
}
