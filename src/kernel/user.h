#ifndef __USER_H__
#define __USER_H__
#include "syscall.h"

#define MAX_USERS 16
#define USER_NAME_MAX 16
#define USER_PASS_MAX 16

#define uid_t int

struct user {
    uid_t uid;
    char name[USER_NAME_MAX];
    char password[USER_PASS_MAX];
};

extern struct user user_table[MAX_USERS];

int user_create(const char *name, const char *password, uid_t uid);
int user_auth(const char *name, const char *password, uid_t *uid);
void user_init(void);
int user_add(const char *name, const char *password, uid_t *uid);
const char* user_get_name(uid_t uid);
long getuid(void);
int setuid(uid_t uid);

#endif /* __USER_H__ */