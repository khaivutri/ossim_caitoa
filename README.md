# ossim_caitoa
Simulation of a Multi-tasking Operating System focusing on Multi-level Queue Scheduling, 64-bit Paging-based Virtual Memory Management, and System Call Interfaces. Built as part of the Operating Systems (CO2018) course at HCMUT.


<div align="center">

<img src="https://img.shields.io/badge/HCMUT-CO2018-003087?style=for-the-badge&logo=academia&logoColor=white"/>
<img src="https://img.shields.io/badge/Language-C-A8B9CC?style=for-the-badge&logo=c&logoColor=white"/>
<img src="https://img.shields.io/badge/Platform-Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black"/>
<img src="https://img.shields.io/badge/Status-Complete-2ea44f?style=for-the-badge"/>

# 🖥️ Simple Operating System Simulation

**A modular operating system simulator featuring process scheduling, virtual memory management, and kernel-mode system calls.**

*CO2018 — Operating Systems | Ho Chi Minh City University of Technology (HCMUT)*
*Linus_Newbies — Faculty of Computer Science & Engineering*

---

</div>

## 📖 Table of Contents

- [Overview](#-overview)
- [Architecture](#-architecture)
- [Key Features](#-key-features)
  - [Process Scheduler](#1-process-scheduler--multi-level-queue)
  - [Virtual Memory Management](#2-virtual-memory-management)
  - [Kernel Interface & System Calls](#3-kernel-interface--system-calls)
- [Project Structure](#-project-structure)
- [Getting Started](#-getting-started)
- [Team](#-team)
- [License](#-license)

---

## 📌 Overview

This project simulates the core components of a multitasking operating system, built as part of the **CO2018 - Operating Systems** course at HCMUT. The simulation models three major subsystems:

| Module | Description |
|---|---|
| **Process Scheduling** | Multi-Level Queue (MLQ) inspired by the Linux kernel |
| **Memory Management** | 5-level paging with swap support and segmentation |
| **Kernel Interface** | Dual-mode system calls with safe user-kernel data transfer |

The system is designed to run on **virtual hardware** supporting multiple CPUs and **dual-mode operation** (User Mode / Kernel Mode).

---

## 🏛️ Architecture

```
┌─────────────────────────────────────────────────────┐
│                   User Applications                  │
├─────────────────────────────────────────────────────┤
│              System Call Interface (syscall.c)        │
│         [ ALLOC │ FREE │ READ │ WRITE ]              │
├───────────────────────┬─────────────────────────────┤
│   Process Scheduler   │   Memory Management Unit     │
│      (sched.c)        │    (mm.c / paging.c)         │
│                       │                              │
│  ┌─── Priority 0 ──┐  │  ┌── Virtual Address Space ┐ │
│  ├─── Priority 1 ──┤  │  │  PGD→P4D→PUD→PMD→PT    │ │
│  ├─── Priority 2 ──┤  │  ├────────────────────────┤ │
│  └─── Priority N ──┘  │  │   RAM  ⇄  SWAP Space   │ │
│                       │  └────────────────────────┘ │
├───────────────────────┴─────────────────────────────┤
│                Virtual Hardware Layer                 │
│            [ Multi-CPU │ Mode Bit Control ]           │
└─────────────────────────────────────────────────────┘
```

---

## 🚀 Key Features

### 1. Process Scheduler — Multi-Level Queue

Implements a **Multi-Level Queue (MLQ)** scheduling policy modelled after the Linux kernel scheduler.

- **Priority Levels:** Configurable up to `MAX_PRIO` priority queues.
- **Time Slicing:** Round-robin execution within each priority level using a fixed time quantum.
- **Slot Allocation:** CPU slots per queue are computed as:

`slots = MAX_PRIO - priority`


Higher-priority processes receive more CPU time, ensuring responsiveness-critical tasks are handled first.

---

### 2. Virtual Memory Management

A comprehensive **paging-based** memory system providing full isolation of process address spaces.

#### Address Space

| Scheme | Details |
|---|---|
| **Address Width** | 64-bit |
| **Paging Levels** | 5-level: `PGD → P4D → PUD → PMD → PT` |
| **Max Virtual Memory** | Up to **128 PiB** per process |

#### Components

- **Segmentation + Paging:** Memory areas (`vm_area_struct`) map contiguous virtual regions to discrete physical frames.
- **Swapping Mechanism:** A page-replacement policy moves pages between **RAM** and **SWAP** storage when physical memory is under pressure.

```
Virtual Address (64-bit)
┌──────┬──────┬──────┬──────┬──────┬────────────┐
│ PGD  │ P4D  │ PUD  │ PMD  │  PT  │   Offset   │
└──────┴──────┴──────┴──────┴──────┴────────────┘
   │      │      │      │      │
   └──────┴──────┴──────┴──────┴──► Physical Frame
```

---

### 3. Kernel Interface & System Calls

Provides a **unified, hardware-assisted** interface for user-space applications to request kernel services.

| Feature | Description |
|---|---|
| **Dual Mode** | Hardware mode bit enforces separation between user and kernel code |
| **System Calls** | `ALLOC`, `FREE`, `READ`, `WRITE` |
| **Data Transfer** | `copy_from_user` / `copy_to_user` for safe cross-boundary transfers |

> ⚠️ All transitions from User Mode → Kernel Mode are gated through the system call interface, preventing unauthorized access to privileged resources.

---

## 📂 Project Structure

```
.
├── src/
│   ├── sched.c          # Multi-Level Queue process scheduler
│   ├── mm.c             # Memory management (allocation, freeing, vm_area)
│   ├── paging.c         # 5-level page table & swap logic
│   └── syscall.c        # System call dispatcher & user-kernel data transfer
│   └── ....
├── include/
│   ├── pcb_t.h          # Process Control Block definition
│   ├── mm_struct.h      # Memory management structures
│   └── krnl_t.h         # Kernel state and configuration types
│   └──....
├── input/
│   ├── *.conf           # Hardware configuration files
│   └── *.proc           # Simulated program descriptions
│
├── Makefile
└── README.md
```

---

## 🛠️ Getting Started

### Prerequisites

- GCC (≥ 9.0) or any C11-compatible compiler
- GNU Make
- Linux/macOS environment (or WSL on Windows)

### Build

```bash
# Clone the repository
git clone https://github.com/<your-repo>/ossim.git
cd ossim

# Compile all modules
make all
```

### Run

```bash
# Execute the simulation with a hardware configuration file
./os [configure_file]
```

**Example:**

```bash
./os input/os_1_mlq_paging.conf
```

### Clean Build Artifacts

```bash
make clean
```

---
## 👥 Team

> **Group: Linux_Newbies** — CO2018 Operating Systems, Semester 2 — 2025–2026

| Role | Responsibility |
|---|---|
| [Vu Tri Khai](https://github.com/khaivutri) |Scheduling |
| [Nguyen Tien Nam](https://github.com/tiennam-nguyen) |Memory Layout+ VM Mapping+ Physical Memory |
| [Nguyen Huu Khanh](https://github.com/Khanhhuu766) |Paging Address Translation+ Memory Ops |
| [Nguyen Nguyen Hung](https://github.com/NguyenHung2006) |Synchronization+ System Call+ Put It All Together |

*Faculty of Computer Science & Engineering, HCMUT*

---

## 📄 License

This source code is developed and used **for educational purposes only**, under the license granted by the **Faculty of Computer Science & Engineering, Ho Chi Minh City University of Technology (HCMUT)**.

> Unauthorized redistribution or commercial use is strictly prohibited.

---




