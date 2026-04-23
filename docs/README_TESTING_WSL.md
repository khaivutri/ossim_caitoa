# Hướng dẫn Chạy Testcase và So sánh Kết quả trên WSL Ubuntu

Tài liệu này hướng dẫn cách kiểm tra tính đúng đắn của Scheduler MLQ trên môi trường WSL (Windows Subsystem for Linux) sử dụng Ubuntu.

---

## 1. Chuẩn bị môi trường
Mở terminal Ubuntu trên WSL và cài đặt các công cụ cần thiết (nếu chưa có):
```bash
sudo apt update
sudo apt install build-essential
```

## 2. Truy cập thư mục Project
Trong WSL, các ổ đĩa Windows thường được mount tại `/mnt/`. Hãy di chuyển đến thư mục project của bạn (thay đổi đường dẫn cho đúng với máy của bạn):
```bash
cd /mnt/c/Users/kelve/Code/ossim_caitoa
```

## 3. Biên dịch chương trình
Chạy lệnh `make` để tạo file thực thi `os`:
```bash
make clean
make
```

## 4. Chạy Testcase và So sánh kết quả

### Cách 1: Chạy thủ công từng testcase (Khuyên dùng)
Để kiểm tra xem Scheduler của bạn chạy có đúng như mẫu (reference) không, hãy làm theo các bước sau:

1. **Chạy testcase và lưu kết quả ra file tạm:**
   Ví dụ với testcase `sched_0`:
   ```bash
   ./os sched_0 > my_result_sched_0.txt
   ```

2. **So sánh với kết quả chuẩn bằng lệnh `diff`:**
   ```bash
   diff -u my_result_sched_0.txt output/sched_0.output
   ```
   *   **Nếu không có gì hiện ra:** Chúc mừng! Kết quả của bạn khớp 100% với mẫu.
   *   **Nếu có các dòng dấu `+` và `-`:** Đó là những điểm khác biệt giữa code của bạn và kết quả mong đợi.

### Cách 2: Chạy kiểm tra hàng loạt bằng Script
Bạn có thể dùng đoạn mã sau để kiểm tra nhanh nhiều testcase cùng lúc mà không làm hỏng file mẫu:

```bash
mkdir -p my_output
for test in sched sched_0 sched_1 os_1_singleCPU_mlq; do
    echo "Testing $test..."
    ./os $test > my_output/$test.txt
    if diff -q my_output/$test.txt output/$test.output > /dev/null; then
        echo "✅ [PASS] $test"
    else
        echo "❌ [FAIL] $test (Xem khác biệt bằng lệnh: diff -u my_output/$test.txt output/$test.output)"
    fi
done
```

## 5. Danh sách các Testcase quan trọng cho Scheduler
| Testcase | Mô tả |
| :--- | :--- |
| `sched` | Kiểm tra lập lịch cơ bản |
| `sched_0` | Kiểm tra MLQ với các tiến trình ưu tiên khác nhau |
| `sched_1` | Kiểm tra MLQ với nhiều tiến trình phức tạp hơn |
| `os_1_singleCPU_mlq` | Kiểm tra MLQ trên hệ thống 1 CPU |

## 6. Lưu ý quan trọng
*   **Không chạy `sh run.sh`**: File này sẽ ghi đè lên các file `.output` chuẩn trong thư mục `output/`, làm mất dữ liệu mẫu để so sánh.
*   **Ký tự xuống dòng**: Nếu bạn gặp lỗi `diff` báo cáo toàn bộ file khác nhau dù nội dung giống hệt, có thể là do sự khác biệt giữa `CRLF` (Windows) và `LF` (Linux). Hãy dùng lệnh `dos2unix` để chuẩn hóa nếu cần:
    ```bash
    sudo apt install dos2unix
    dos2unix output/*.output
    ```
