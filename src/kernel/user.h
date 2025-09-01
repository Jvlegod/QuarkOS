#ifndef __USER_H__
#define __USER_H__

#define MAX_USERS 16
#define USER_NAME_MAX 16
#define USER_PASS_MAX 16

struct user {
    int uid;
    char name[USER_NAME_MAX];
    char password[USER_PASS_MAX];
};

extern struct user user_table[MAX_USERS];

int user_create(const char *name, const char *password, int uid);
int user_auth(const char *name, const char *password, int *uid);

#endif /* __USER_H__ */