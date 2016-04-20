# Linux user chroot login shell

After running this shell, the user will be chroot into a dir.


## Compile
```
apt-get install gcc git
```
```
git clone https://git.coding.net/yzs/Linux-CHROOT-SHELL.git
```
```
cd Linux-CHROOT-SHELL
```
```
gcc main.c -o /usr/bin/chroot_shell
```

## Add set uid and set gid permission so as to use chroot()
```
chmod u+s,g+s /usr/bin/chroot_shell
```

## Basic chroot file system build
```
mkdir -p /srv/chroot/jessie/
wget http://mirrors.ustc.edu.cn/openvz/template/precreated/debian-8.0-x86_64-minimal.tar.gz -O- | tar zxvf - -C /srv/chroot/jessie/ # Use OpenVZ template so as to build it quickly
```

## Create user
```
useradd -ms /usr/bin/chroot_shell chroot_user
```
```
echo "chroot_user:password" | chpasswd
```

Then you can login as chroot_user and you will find you are in the chroot file system.
