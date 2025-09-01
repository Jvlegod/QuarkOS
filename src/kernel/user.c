#include "cstdlib.h"
#include "user.h"

struct user user_table[MAX_USERS];

static int find_free_slot() {
    for (int i = 0; i < MAX_USERS; i++) {
        if (user_table[i].uid == 0 && user_table[i].name[0] == '\0') {
            return i;
        }
    }
    return -1;
}

int user_create(const char *name, const char *password, int uid) {
    int idx = find_free_slot();
    if (idx < 0) {
        return -1;
    }
    user_table[idx].uid = uid;
    strncpy(user_table[idx].name, name, USER_NAME_MAX - 1);
    user_table[idx].name[USER_NAME_MAX - 1] = '\0';
    strncpy(user_table[idx].password, password, USER_PASS_MAX - 1);
    user_table[idx].password[USER_PASS_MAX - 1] = '\0';
    return 0;
}

int user_auth(const char *name, const char *password, int *uid) {
    for (int i = 0; i < MAX_USERS; i++) {
        if (strncmp(user_table[i].name, name, USER_NAME_MAX) == 0 &&
            strncmp(user_table[i].password, password, USER_PASS_MAX) == 0) {
            if (uid) {
                *uid = user_table[i].uid;
            }
            return 0;
        }
    }
    return -1;
}