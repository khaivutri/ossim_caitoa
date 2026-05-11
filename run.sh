./os os_0_mlq_paging  > input/os_0_mlq_paging.out
./os os_1_mlq_paging > input/os_1_mlq_paging.out
./os os_1_mlq_paging_small_1K > input/os_1_mlq_paging_small_1K.out
./os os_1_mlq_paging_small_4K > input/os_1_mlq_paging_small_4K.out
./os os_1_singleCPU_mlq > input/os_1_singleCPU_mlq.out
./os os_1_singleCPU_mlq_paging > input/os_1_singleCPU_mlq_paging.out
./os os_2_singleCPU_mlq_paging > input/os_2_singleCPU_mlq_paging.out
./os os_2_mlq_paging > input/os_2_mlq_paging.out
./os sched > input/sched.out
./os sched_0 > input/sched_0.out
./os sched_1 > input/sched_1.out
./os os_sc > input/os_sc.out
./os os_syscall > input/os_syscall.out
./os os_syscall_list > input/os_syscall_list.out
mv input/*.output output
