#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <stdbool.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <dirent.h>
#define ROOT_FILE_SYSTEM "/srv/chroot/jessie/"
#define USER_CHROOT_DIR "/srv/chroot/user"
#define USER_TMP_DIR "/srv/chroot/user_tmp"
#define SHELL "/bin/bash"
#define DEFAULT_CHDIR "/"
#define VALID_FILE_NAME "/lib/.chroot"

bool dir_create(const char * path);

int main(void) {
    DIR *dirp;
    int fd;
    bool chdir_to_userdir = true;
    struct passwd *user_info;
    struct dirent *dir_item;
    uid_t uid = getuid(), euid = geteuid(); // GET UID
    gid_t gid = getgid(), egid = getegid(); // GET GID
    char *shell_argv[] = {"--login", NULL}, chroot_dir[FILENAME_MAX + 1], uid_dir[FILENAME_MAX + 1], chroot_valid_filename[FILENAME_MAX + 1], valid_file_full_path[FILENAME_MAX + 1], user_dir_full_path[FILENAME_MAX + 1], user_dir_parent_dir[FILENAME_MAX + 1], tmp_file_name1[FILENAME_MAX + 1], tmp_file_name2[FILENAME_MAX + 1];
    printf("UID: %ld, GID: %ld\nEUID: %ld, EGID: %ld\n", (long) uid, (long) gid, (long) euid, (long) egid);
    sprintf(uid_dir, "/uid_%ld", (long) uid);
    strcpy(chroot_dir, USER_CHROOT_DIR);
    strcat(chroot_dir, uid_dir);
    dir_create(chroot_dir);

    // GET THE FULL PATH TO THE CHROOT FILE SYSTEM VALID FILE AND CREATE IT
    strcpy(chroot_valid_filename, ROOT_FILE_SYSTEM);
    strcat(chroot_valid_filename, VALID_FILE_NAME);
    if ((fd = open(chroot_valid_filename, O_CREAT | O_RDONLY, S_IRUSR)) == -1) {
        perror("Chroot valid file open failed");
        exit(EXIT_FAILURE);
    }
    close(fd);

    user_info = getpwuid(uid); // GET USER INFO

    // GET THE FULL PATH TO THE CHROOT DIR FOR USER
    strcpy(valid_file_full_path, chroot_dir);
    strcat(valid_file_full_path, VALID_FILE_NAME);

    // GET THE FULL PATH TO THE USER DIR
    strcpy(user_dir_full_path, chroot_dir);
    strcat(user_dir_full_path, user_info->pw_dir);

    // CHECK WHETHER THE CHROOT VALID EXISTS IN THE CHROOT DIR FOR USER, IF NOT, BIND MOUNT CHROOT FILE SYSTEM FOR USER
    if (access(valid_file_full_path, F_OK) != 0) {

        // OPEN THE CHROOT FILE SYSTEM DIR FOR GET THE LIST OF FILE IN THE CHROOT ROOT DIR
        if ((dirp = opendir(ROOT_FILE_SYSTEM)) == NULL) {
            perror("opendir failed");
            exit(EXIT_FAILURE);
        }

        // GET THE FILE NAME, CREATE THE SAME NAME ONE IN USER CHROOT DIR THEN BIND MOUNT IT, "home", "tmp", "." AND ".." ARE IGNORE
        while ((dir_item = readdir(dirp)) != NULL) {
            printf("%s\n", dir_item->d_name);
            if ((dir_item->d_type == DT_REG || dir_item->d_type == DT_DIR) && strcmp(dir_item->d_name, "home") != 0 && strcmp(dir_item->d_name, "tmp") && strcmp(dir_item->d_name, ".") != 0 && strcmp(dir_item->d_name, "..") != 0) {
                strcpy(tmp_file_name1, chroot_dir);
                strcat(tmp_file_name1, "/");
                strcat(tmp_file_name1, dir_item->d_name);
                strcpy(tmp_file_name2, ROOT_FILE_SYSTEM);
                strcat(tmp_file_name2, "/");
                strcat(tmp_file_name2, dir_item->d_name);
                switch (dir_item->d_type) {
                    case DT_REG:
                        if (access(tmp_file_name1, F_OK) == 0) {
                            if ((fd = open(tmp_file_name1, O_RDONLY | O_CREAT)) == -1) {
                                perror("File open failed");
                                exit(EXIT_FAILURE);
                            }
                            close(fd);
                        }
                        break;
                    case DT_DIR:
                        dir_create(tmp_file_name1);
                        break;
                }
                printf("mount %s to %s\n", tmp_file_name2, tmp_file_name1);
                if (mount(tmp_file_name2, tmp_file_name1, NULL, MS_BIND | MS_NODEV | MS_RDONLY | MS_NOSUID, NULL) != 0) {
                    perror("Bind mount falied");
                    exit(EXIT_FAILURE);
                }
                // NEED TO USE REMOUNT TO MOUNT AS READ ONLY
                if (mount(tmp_file_name2, tmp_file_name1, NULL, MS_REMOUNT | MS_BIND | MS_NODEV | MS_RDONLY | MS_NOSUID, NULL) != 0) {
                    perror("Bind mount falied");
                    exit(EXIT_FAILURE);
                }
            }
        }
        closedir(dirp);

        // CREATE TMP DIR
        strcpy(tmp_file_name1, chroot_dir);
        strcat(tmp_file_name1, "/");
        strcat(tmp_file_name1, "tmp");
        strcpy(tmp_file_name2, USER_TMP_DIR);
        strcat(tmp_file_name2, "/");
        strcat(tmp_file_name2, "tmp");
        if (access(tmp_file_name2, F_OK) != 0) {
            dir_create(tmp_file_name2);
            if (chmod(tmp_file_name2, S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
                perror("chmod for tmp dir failed");
                exit(EXIT_FAILURE);
            }
        }
        if (access(tmp_file_name1, F_OK) != 0)
            dir_create(tmp_file_name1);
        if (mount(tmp_file_name2, tmp_file_name1, NULL, MS_BIND | MS_NODEV | MS_RDONLY | MS_NOSUID, NULL) != 0) {
            perror("Bind mount falied");
            exit(EXIT_FAILURE);
        }
    }

    // CHECK WHETHER THE USER HAS USER DIR
    if (access(user_info->pw_dir, F_OK) != 0)
        chdir_to_userdir = false;
    else if (access(user_dir_full_path, F_OK) != 0) { // IF YES, BIND MOUNT THE USER DIR TO THE USER CHROOT DIR
        dir_create(user_dir_full_path); // Create parent dirs, so as to mount tmpfs
        rmdir(user_dir_full_path); // Remove user home dir, because we need to mount a tmpfs to the parent dir

        // GET THE PARENT DIR OF USER DIR
        strcpy(user_dir_parent_dir, user_dir_full_path);
        dirname(user_dir_parent_dir);

        // MOUNT THE TMPFS
        printf("mount %s to %s\n", "none", user_dir_parent_dir);
        if (mount("none", user_dir_parent_dir, "tmpfs", 0, "mode=0711,uid=0,size=4k") != 0) {
            perror("tmpfs mount failed");
            exit(EXIT_FAILURE);
        }

        // CREATE USER DIR
        mkdir(user_dir_full_path, S_IRWXU);

        // BIND MOUNT USER DIR
        printf("mount %s to %s\n", user_info->pw_dir, user_dir_full_path);
        if (mount(user_info->pw_dir, user_dir_full_path, NULL, MS_BIND | MS_NODEV | MS_NOEXEC | MS_NOSUID, NULL) != 0) {
            perror("Bind mount failed");
            exit(EXIT_FAILURE);
        }
    }

    // TRY CHDIR
    if (chroot(chroot_dir) == -1) {
        perror("Chroot failed");
        exit(EXIT_FAILURE);
    }

    // CHANGE THE UID, GID, EUID AND EGID BACK TO THE REAL ID
    setegid(gid); // SET EFFECTIVE GID
    seteuid(uid); // SET EFFECTIVE UID
    setgid(gid); // SET REAL UID
    setuid(uid); // SET REAL GID
    uid = getuid(), euid = geteuid(); // GET UID
    gid = getgid(), egid = getegid(); // GET GID
    printf("UID: %ld, GID: %ld\nEUID: %ld, EGID: %ld\n", (long) uid, (long) gid, (long) euid, (long) egid);

    // TRY TO CHDIR, BECAUSE CHROOT DO NOT CHDIR AUTOMATICALLY
    // IF THE USER HAS USER DIR, CHDIR INTO IT
    if (chdir_to_userdir && access(user_info->pw_dir, F_OK | X_OK) == 0) {
        printf("Chdir to user dir: %s\n", user_info->pw_dir);
        if (chdir(user_info->pw_dir) == -1) {
            perror("Chdir failed");
            exit(EXIT_FAILURE);
        }
    } else {
        printf("Chdir to default dir: %s\n", DEFAULT_CHDIR);
        if (chdir(DEFAULT_CHDIR) == -1) {
            perror("Chdir failed");
            exit(EXIT_FAILURE);
        }
    }

    // EXECUTE THE SHELL, SO AS TO REPLACE OF CURRENT PROGRAM
    execve(SHELL, shell_argv, NULL);
    // PRINT THE ERROR INFO IF SHELL EXECUTE FAILED
    perror("Exec");
    return 0;
}

bool dir_create(const char * path) {
    printf("Dir: %s\n", path);
    char the_path[FILENAME_MAX + 1]; // STORE THE PATH
    char the_dirname[FILENAME_MAX + 1]; // STORE THE PARENT DIRNAME
    if (strcmp(path, "/") == 0 || access(path, F_OK) == 0) { // CHECK WHETHER THE PATH IS / OR EXISTS
        printf("Path %s exists\n", path);
        return true;
    }
    strcpy(the_path, path);
    strcpy(the_dirname, dirname(the_path)); // GET PARENT DIR
    if (access(the_dirname, F_OK) != 0) { // IF PARENT DIR NOT EXISTS
        dir_create(the_dirname); // CREATE IT
    }
    if (mkdir(path, S_IRWXU | S_IXGRP | S_IRGRP | S_IROTH | S_IXOTH) != 0) {
        printf("%s creating.\n", path);
        perror("DIR Create Filed");
        exit(EXIT_FAILURE);
    }
    return true;
}