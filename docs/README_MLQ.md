# Tài liệu Kỹ thuật: Hiện thực Scheduler Multi-level Queue (MLQ)

Tài liệu này cung cấp hướng dẫn chi tiết về cách hiện thực thuật toán lập lịch MLQ và giải thích cơ chế phối hợp giữa Scheduler, CPU Routine và Timer.

---

## 1. Quản lý Hàng đợi (FIFO Queue)
Hàng đợi trong project này được hiện thực dựa trên mảng tĩnh `proc[MAX_QUEUE_SIZE]` tại file `src/queue.c`.

### Cơ chế Enqueue & Dequeue
1.  **`enqueue`**: Thêm tiến trình vào vị trí `q->proc[q->size]`. Đây là vị trí cuối cùng của hàng đợi. Đừng quên tăng `q->size` sau khi thêm.
2.  **`dequeue`**: 
    *   Luôn lấy phần tử ở đầu mảng (`q->proc[0]`).
    *   **Dịch chuyển (Shift)**: Sau khi lấy ra, bạn phải dịch chuyển tất cả các phần tử còn lại từ vị trí `i` sang `i-1` để lấp chỗ trống.
    *   Giảm `q->size`.
    *   Trả về tiến trình.

### Cơ chế Purgequeue (Gỡ tiến trình chỉ định)
Khác với `dequeue` (lấy phần tử đầu tiên), `purgequeue` cần tìm và xóa một tiến trình cụ thể bất kể nó nằm ở đâu trong mảng:
1.  **Duyệt mảng** để tìm vị trí `i` sao cho `q->proc[i] == proc`.
2.  **Dịch chuyển**: Nếu tìm thấy, dịch chuyển các phần tử từ `j = i` đến `q->size - 1` lên phía trước 1 bước (`q->proc[j] = q->proc[j+1]`).
3.  **Cập nhật kích thước**: Giảm `q->size` đi 1 và trả về tiến trình vừa xóa. Trả về `NULL` nếu không tìm thấy.
*Lưu ý: Hàm này cực kỳ quan trọng để gỡ tiến trình khỏi `running_list` khi nó kết thúc hoặc hết thời gian (slot).*

---

## 2. Logic Lập lịch MLQ (`src/sched.c`)

### A. Cơ chế Slot (Time Quantum)
Mỗi mức ưu tiên (`prio`) có một số lượng slot nhất định trong một chu kỳ: `slot[prio] = MAX_PRIO - prio`.
*   Tiến trình có `prio` thấp (độ ưu tiên cao như 0, 1, 2...) sẽ có nhiều slot hơn.
*   Mỗi lần một CPU lấy một tiến trình từ hàng đợi `prio`, giá trị `slot[prio]` phải giảm đi 1.

### B. Các hàm xử lý luồng MLQ

#### `get_mlq_proc` (CPU yêu cầu tiến trình)
1.  Duyệt vòng lặp `prio` từ `0` đến `MAX_PRIO - 1`.
2.  Nếu `mlq_ready_queue[prio]` không trống VÀ `slot[prio] > 0`:
    *   `proc = dequeue(&mlq_ready_queue[prio])`
    *   Giảm `slot[prio]` đi 1.
    *   Thoát vòng lặp tìm kiếm.
3.  **Cập nhật `running_list`**: Nếu tìm thấy `proc`, phải **`enqueue(&running_list, proc)`**. Việc này giúp hệ thống biết tiến trình nào đang chiếm CPU.
4.  **Reset Slot**: Nếu không tìm thấy `proc` (do tất cả đã hết slot hoặc trống), kiểm tra toàn bộ hệ thống bằng `queue_empty()`. Nếu vẫn còn tiến trình ở các hàng đợi nhưng đã hết slot thì thực hiện khởi tạo lại mảng `slot` (`slot[i] = MAX_PRIO - i`) và trả về `NULL`. Việc trả về `NULL` sẽ kích hoạt cơ chế chờ của CPU.

#### `put_mlq_proc` (Tiến trình chạy hết slot)
Khi CPU chạy hết `time_slot`, nó gọi hàm này để trả tiến trình về hệ thống.
1.  **Gỡ khỏi `running_list`**: Gọi `purgequeue(&running_list, proc)` vì tiến trình không còn chạy trên CPU nữa.
2.  **Đưa về hàng đợi**: Gọi `enqueue(&mlq_ready_queue[proc->prio], proc)` để chờ tới lượt tiếp theo.

#### `add_mlq_proc` (Nạp tiến trình mới)
Được gọi từ `loader.c` khi một tiến trình mới được nạp vào RAM. Nó chưa từng chạy nên không nằm trong `running_list`.
1.  Chỉ cần gọi `enqueue(&mlq_ready_queue[proc->prio], proc)`.

---

## 3. Logic Lập lịch Non-MLQ (Khối `#else`)
Nếu `OS_CFG` tắt `MLQ_SCHED`, hệ thống sử dụng thuật toán Active/Expired Queue đơn giản:
*   **`add_proc`**: Thêm thẳng vào `ready_queue`.
*   **`put_proc`**: Tiến trình chạy xong slot sẽ bị tống vào `run_queue` (hàng đợi của các tiến trình "hết lượt"). Nhớ dùng `purgequeue` để gỡ nó khỏi `running_list`.
*   **`get_proc`**: Lấy phần tử đầu tiên của `ready_queue` và đưa vào `running_list`. Nếu `ready_queue` trống nhưng `run_queue` vẫn còn, hệ thống sẽ **bốc toàn bộ `run_queue` đổ sang `ready_queue`** (đây chính là thao tác "đảo hàng đợi" Active <-> Expired) rồi mới tiến hành `dequeue`.

---

## 4. Vai trò của `next_slot` và Đồng bộ hóa

Sự phối hợp giữa `src/sched.c` và `src/os.c` thông qua `next_slot()` là cực kỳ quan trọng:

### A. Phối hợp với `cpu_routine` trong `os.c`
Trong `src/os.c`, mỗi CPU chạy một vòng lặp:
```c
proc = get_proc();
if (proc == NULL) {
    next_slot(timer_id); 
    continue;
}
```
Khi `get_proc` trả về `NULL` (do bạn vừa reset slot), CPU sẽ gọi `next_slot()`. Điều này giúp:
*   **Tránh Busy Waiting**: CPU sẽ "ngủ" thay vì chạy vòng lặp vô tận gây tốn tài nguyên máy tính thật.
*   **Đồng bộ thời gian**: Đảm bảo toàn bộ CPU trong hệ thống cùng bước sang một nhịp thời gian mới (`_time++`).

### B. Cơ chế nhịp tim (Timer)
Hàm `next_slot()` trong `src/timer.c` đảm bảo rằng:
1.  Thời gian chỉ trôi đi khi **tất cả** CPU đã xong việc của mình trong slot hiện tại.
2.  Sau khi thời gian tăng lên, Timer sẽ đánh thức tất cả CPU dậy. Lúc này, vì `slot` đã được reset ở bước trước, CPU gọi lại `get_proc()` và sẽ lấy được tiến trình để chạy.

---

## 5. Lưu ý về An toàn Đa luồng (Thread Safety)
Vì hệ thống mô phỏng đa nhân (nhiều CPU threads), việc truy cập vào các hàng đợi chung và mảng `slot` phải được bảo vệ tuyệt đối:
*   Luôn dùng `pthread_mutex_lock(&queue_lock)` trước khi đọc/ghi hàng đợi hoặc mảng slot.
*   Luôn dùng `pthread_mutex_unlock(&queue_lock)` ngay sau khi kết thúc thao tác.
*   Tất cả các thao tác `enqueue`, `dequeue`, `purgequeue` bên trong các hàm của `sched.c` đều **phải** được bọc giữa cặp lock/unlock này.

---

## 6. Tóm tắt Vòng đời Tiến trình
1.  **NEW**: Tiến trình được `loader.c` nạp, gọi `add_proc` -> Nằm trong `mlq_ready_queue` (hoặc `ready_queue`).
2.  **READY**: Nằm chờ CPU.
3.  **RUNNING**: CPU gọi `get_proc`, tiến trình sang `running_list` và thi hành các lệnh.
4.  **EXPIRED**: CPU chạy hết time slot, gọi `put_proc` -> Tiến trình bị gỡ khỏi `running_list` bằng `purgequeue` và quay lại `mlq_ready_queue` (hoặc `run_queue` nếu non-MLQ).
5.  **TERMINATED**: Nếu `proc->pc == proc->code->size` (chạy xong toàn bộ lệnh), `os.c` gọi `free(proc)`. Vòng đời kết thúc.
