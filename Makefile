file=walk_page.c
file1=all_walk_page.c
file2=10_walk_page.c
all:
	gcc -w -g -O2 -L/root/pageTableWalk/libvmi-0.8/libvmi/.libs /root/pageTableWalk/libvmi-0.8/libvmi/.libs/libvmi.so -lglib-2.0 -lm -lxenctrl -lxenstore -Wl,-rpath -Wl,/root/pageTableWalk/libvmi-0.8/libvmi/.libs -Wl,-rpath -Wl,/usr/local/lib   ${file1} -o all_walk_page
	gcc -w -g -O2 -L/root/pageTableWalk/libvmi-0.8/libvmi/.libs /root/pageTableWalk/libvmi-0.8/libvmi/.libs/libvmi.so -lglib-2.0 -lm  -lxenctrl -lxenstore -Wl,-rpath -Wl,/root/pageTableWalk/libvmi-0.8/libvmi/.libs -Wl,-rpath -Wl,/usr/local/lib   ${file} -o walk_page
	gcc -w -g -O2 -L/root/pageTableWalk/libvmi-0.8/libvmi/.libs /root/pageTableWalk/libvmi-0.8/libvmi/.libs/libvmi.so -lglib-2.0 -lm  -lxenctrl -lxenstore -Wl,-rpath -Wl,/root/pageTableWalk/libvmi-0.8/libvmi/.libs -Wl,-rpath -Wl,/usr/local/lib   ${file2} -o 10_walk_page
