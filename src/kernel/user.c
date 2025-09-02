#include "cstdlib.h"
#include "user.h"
#include "fs.h"

struct user user_table[MAX_USERS];
static uid_t next_uid = 1;

static int find_free_slot() {
    for (int i = 0; i < MAX_USERS; i++) {
        if (user_table[i].name[0] == '\0') {
            return i;
        }
    }
    return -1;
}

static void user_save(void) {
    char buf[1024];
    int off = 0;
    for (int i = 0; i < MAX_USERS; i++) {
        if (user_table[i].name[0] == '\0') continue;
        off += snprintf(buf + off, sizeof(buf) - off, "%d %s %s\n",
                        user_table[i].uid,
                        user_table[i].name,
                        user_table[i].password);
    }
    fs_mkdir("/etc");
    fs_create("/etc/passwd", 0644);
    fs_write_all("/etc/passwd", buf, off);
}

static void user_load(void) {
    char buf[1024];
    int n = fs_read_all("/etc/passwd", buf, sizeof(buf) - 1);
    if (n <= 0) return;
    buf[n] = '\0';
    char *p = buf;
    int max_uid = 0;
    while (*p) {
        while (*p == '\n' || *p == '\r') p++;
        if (!*p) break;
        int uid = 0;
        while (is_digit(*p)) { uid = uid*10 + (*p - '0'); p++; }
        if (*p != ' ') break; p++;
        char name[USER_NAME_MAX]; int ni = 0;
        while (*p && *p != ' ' && *p != '\n' && ni < USER_NAME_MAX-1) {
            name[ni++] = *p++;
        }
        name[ni] = '\0';
        if (*p != ' ') break; p++;
        char pass[USER_PASS_MAX]; int pi = 0;
        while (*p && *p != '\n' && pi < USER_PASS_MAX-1) {
            pass[pi++] = *p++;
        }
        pass[pi] = '\0';
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        user_create(name, pass, uid);
        if (uid >= max_uid) max_uid = uid + 1;
    }
    if (max_uid > next_uid) next_uid = max_uid;
}

void user_init(void) {
    memset(user_table, 0, sizeof(user_table));
    fs_mkdir("/home");
    fs_mkdir("/etc");
    user_load();
    if (user_get_name(0) == NULL) {
        user_create("root", "root", 0);
        user_save();
    }
    fs_mkdir("/home/root");
}

int user_create(const char *name, const char *password, uid_t uid) {
    for (int i = 0; i < MAX_USERS; i++) {
        if (user_table[i].name[0] != '\0' &&
            (user_table[i].uid == uid ||
             strncmp(user_table[i].name, name, USER_NAME_MAX) == 0)) {
            return -1;
        }
    }
    int idx = find_free_slot();
    if (idx < 0) {
        return -1;
    }
    user_table[idx].uid = uid;
    strncpy(user_table[idx].name, name, USER_NAME_MAX - 1);
    user_table[idx].name[USER_NAME_MAX - 1] = '\0';
    strncpy(user_table[idx].password, password, USER_PASS_MAX - 1);
    user_table[idx].password[USER_PASS_MAX - 1] = '\0';
    if (uid >= next_uid) {
        next_uid = uid + 1;
    }
    return 0;
}

int user_add(const char *name, const char *password, uid_t *uid) {
    uid_t new_uid = next_uid++;
    if (user_create(name, password, new_uid) != 0) {
        next_uid--; // rollback
        return -1;
    }
    if (uid) {
        *uid = new_uid;
    }
    user_save();
    return 0;
}

const char* user_get_name(uid_t uid) {
    for (int i = 0; i < MAX_USERS; i++) {
        if (user_table[i].name[0] != '\0' && user_table[i].uid == uid) {
            return user_table[i].name;
        }
    }
    return NULL;
}


int user_auth(const char *name, const char *password, uid_t *uid) {
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

long getuid(void)
{
    return syscall(SYS_GETUID, 0, 0, 0, 0, 0, 0);
}

int setuid(uid_t uid)
{
    long ret = syscall(SYS_SETUID, uid, 0, 0, 0, 0, 0);
    if (ret < 0) {
        return -1;
    }
    return 0;
}