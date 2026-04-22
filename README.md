# Multi-Container Runtime in C

# Operating Systems Project Report

# Introduction
We built a lightweight Linux container runtime in C along with a kernel-space memory monitoring module. The main idea behind this project was to understand how containers work internally at a much lower level rather than simply using existing tools like Docker.

Through this project, we got hands-on experience with process creation, container lifecycle management, root filesystem isolation, and basic kernel module programming. It gave us a much clearer understanding of how operating system concepts such as process isolation, memory handling, and system-level monitoring work in practice.


# Objective
The main objective of our project was to design and implement a lightweight multi-container runtime that can:
-run multiple isolated containers
-manage container lifecycle using commands like run, start, stop, logs, and ps
-support a supervisor process
-monitor memory behavior from kernel space
-run CPU, memory, and I/O intensive workloads for analysis


# Project Structure
The major files used in our project are:
-`engine.c` → user-space container runtime and supervisor
-`monitor.c` → kernel-space memory monitor
-`monitor_ioctl.h` → shared ioctl definitions
-`cpu_hog.c` → CPU intensive workload
-`memory_hog.c` → memory intensive workload
-`io_pulse.c` → I/O intensive workload
-`Makefile` → build commands for the project


# Environment Check
```bash
cd boilerplate
chmod +x environment-check.sh
sudo ./environment-check.sh
```
Screenshot — Environment Preflight Check Passing:
<img width="1600" height="900" alt="image" src="https://github.com/user-attachments/assets/eb6adf47-503f-48eb-bdcb-aaa6cd9330e7" />


# Build Output
<img width="1600" height="900" alt="image" src="https://github.com/user-attachments/assets/77eaa083-23e9-4b02-885d-debc001f2790" />
This step confirmed that our environment and compiler setup were working properly before moving to the actual implementation and testing.


# Root Filesystem Preparation
To simulate container isolation, we used Alpine Linux minirootfs as the base filesystem.
The following steps were used:
```bash
mkdir rootfs-base
wget https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.3-x86_64.tar.gz
tar -xzf alpine-minirootfs-3.20.3-x86_64.tar.gz -C rootfs-base
cp -a rootfs-base rootfs-alpha
```
This allowed us to create a separate writable filesystem for our container.

# Root Filesystem Screenshot
<img width="2560" height="1440" alt="image" src="https://github.com/user-attachments/assets/56479079-532c-4915-b833-a558f0bde41e" />
This part helped us understand how each container can have its own isolated filesystem environment.


# Multi-Container Execution & Supervisor
Unlike basic runtimes, our implementation uses a Supervisor mode. The supervisor stays active in the background, listening for new container requests via Unix Domain Sockets.
To run the system:
```Bash
# Start the supervisor
sudo ./engine supervisor ./rootfs-alpha

# Launch isolated instances
sudo ./engine run alpha ./rootfs-alpha "sleep 100"
sudo ./engine run beta ./rootfs-beta "while true; do sleep 1; done"
```
Verification: Process Isolation
We verified the multi-container capability using ps aux | grep engine. This shows the kernel managing independent process trees for each container ID.
<img width="1600" height="900" alt="image" src="https://github.com/user-attachments/assets/c907404f-dd88-4be2-b335-884d26fd1253" />


# Process Lifecycle Management
We also tested background container execution and lifecycle commands.
The following commands were used:
```bash
sudo ./engine start alpha ../rootfs-alpha /bin/sh
./engine ps
./engine logs alpha
sudo ./engine stop alpha
```

# Lifecycle Screenshot
<img width="2078" height="567" alt="image" src="https://github.com/user-attachments/assets/3b4efeab-096f-444e-adc8-4d151d25c985" />
This helped us verify that container processes could be started, monitored, and terminated correctly.


# Kernel Memory Monitor
The kernel-space monitoring component was compiled as a loadable kernel module.
We loaded it using:
```bash
sudo insmod monitor.ko
```
To verify successful loading, we checked the kernel logs:
```bash
dmesg | tail
```
# Kernel Module Screenshot
<img width="1600" height="900" alt="image" src="https://github.com/user-attachments/assets/63eabc84-3bc5-45de-9fa0-a95503ebab5c" />
<img width="1600" height="900" alt="image" src="https://github.com/user-attachments/assets/df6e7e81-829e-4a61-9a36-b1ef3d179151" />

This section was particularly useful in helping us understand how kernel modules are loaded and how they can be used to monitor system behavior.


# Workload Experiments and Analysis
To study the behavior of the runtime under different workloads, we used the three provided test programs.


# 1. CPU-bound Workload
The CPU-intensive workload was tested using:
```bash
./cpu_hog
```

# CPU Workload Screenshot
<img width="2560" height="1440" alt="image" src="https://github.com/user-attachments/assets/4316540a-e89d-4532-b611-c7afe424091f" />
<img width="2560" height="1440" alt="image" src="https://github.com/user-attachments/assets/17d210af-a2be-456d-8679-fbcecc7fafd1" />


# Observation
We observed that CPU usage remained consistently high while this program was running, indicating continuous processor-intensive operations.
This helped us understand how the scheduler handles computation-heavy processes.


# 2. Memory-bound Workload
The memory-intensive workload was tested using:
```bash
./memory_hog
```

# Memory Workload Screenshot
<img width="1600" height="900" alt="image" src="https://github.com/user-attachments/assets/b7f7cbda-00b1-423a-81da-5136656b1bda" />


# Observation
During execution, memory usage steadily increased, clearly showing the process consuming a larger amount of RAM over time.
This experiment was useful in understanding how memory-intensive applications behave and how the monitoring module can be used to observe them.


# 3. I/O-bound Workload
The I/O-intensive workload was tested using:
```bash
./io_pulse
```

# I/O Workload Screenshot
<img width="2560" height="1440" alt="image" src="https://github.com/user-attachments/assets/69450f74-1bbf-4a5b-9d84-0a6c4c9e886a" />


# Observation
This workload generated periodic bursts of disk activity and helped us analyze how the system behaves under I/O-heavy operations.


# Challenges Faced
Developing this runtime presented several real-world systems engineering challenges:
Unix Domain Socket Conflicts: We encountered "Address already in use" errors where the supervisor wouldn't restart. We resolved this by implementing a cleanup routine for /tmp/container_socket files.
CLI Logic Refactoring: Initially, the boilerplate required "dummy" arguments to run commands. We refactored the argc handling in engine.c to create a more intuitive, human-readable CLI.
Git Divergence: Syncing changes between a local VM and GitHub led to divergent branch histories, which we resolved using rebase and force-synchronization techniques.
Filesystem Persistency: Debugging the /proc and /dev mount points within the minirootfs to ensure commands like ps and uptime worked inside the container.


# Learning Outcomes
This project bridged the gap between OS theory and practice. We learned:
Namespace Isolation: How the OS hides processes and filesystems from a container.
Inter-Process Communication (IPC): Using sockets to coordinate between a user-facing CLI and a background supervisor.
Kernel Interaction: Loading modules to monitor and restrict resource usage.
Systems Debugging: Using tools like dmesg, ps aux, and strace to find why a binary isn't executing as expected.


# Conclusion
Building a lightweight container runtime from scratch helped us understand how modern container systems work internally and gave us hands-on exposure to low-level operating system concepts that are usually only discussed theoretically in class.
It was helped us understand process isolation, kernel-space interaction, and workload behavior in a practical setting.
