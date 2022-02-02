# sha256stat

List file information including sha256 checksum. Basically combines `ls -l` and `sha256sum` into one command.

**Usage:**

'''
sha256stat /boot/vmlinuz

536638847c584b5f740120bc6e53520aa80f2d5b38b3036a3087d590a4257852 -rw------- 1 root root 6,867,184 2022-01-24 11:06 /boot/vmlinuz
```

```
find /boot/ -type f | sha256stat

6fe0fa2c336cb45b3b713073059e97fbf4cd3c692bce79e7d2d6bd46863c285d -rw------- 1 root root   118,618 2022-01-24 11:06 /boot/config
536638847c584b5f740120bc6e53520aa80f2d5b38b3036a3087d590a4257852 -rw------- 1 root root 6,867,184 2022-01-24 11:06 /boot/vmlinuz
b7a0260762d47d79a86da238f84f5bacc92975591bccde3a2abeb6e5b0ab6d37 -rw------- 1 root root 9,455,675 2022-01-24 11:06 /boot/System.map
```

use `-c` for concise output

```
shastat -c /boot/vmlinuz
536638847c584b5f740120bc6e53520aa80f2d5b38b3036a3087d590a4257852 -rw------- 1 root root 6,867,184 /boot/vmlinuz
```

sha256stat can handle filenames with whitespaces (option `-0`):

```
find ~/.config/chromium/Default/ -mmin -5 -name '* *' -print0 | shastat -c -0
-                                                                drwx------ 3 karel karel  4,096 /home/karel/.config/chromium/Default/Sync Data
e728d53aae2b9c8287decae559c5986bb5c8d4fb5b725b380fe7749cfc15cd8b -rw------- 1 karel karel 15,138 /home/karel/.config/chromium/Default/Current Session
d4f2ce601dcc2fb10a93ea0a6bb330f2936f21f571db33f5a4039aa6c402b926 -rw------- 1 karel karel  5,516 /home/karel/.config/chromium/Default/Current Tabs




```
