import struct

trace_file = "/scratch/nikhitha/ReuseCache/ONNX-Runtime-Inference/caffenet-12.champsim.normal.trace"

# 64 bytes per instruction (little-endian assumed)
record_size = 64
num_records_to_read = 50

with open(trace_file, "rb") as f:
    for _ in range(num_records_to_read):
        data = f.read(record_size)
        if not data:
            break

        # Unpack in little-endian format
        ip = struct.unpack_from('<Q', data, 0)[0]
        is_branch = struct.unpack_from('B', data, 8)[0]
        branch_taken = struct.unpack_from('B', data, 9)[0]
        dest_regs = struct.unpack_from('2B', data, 10)
        src_regs = struct.unpack_from('4B', data, 12)
        dest_mem = struct.unpack_from('<QQ', data, 16)
        src_mem = struct.unpack_from('<QQQQ', data, 32)

        print(f"IP: {hex(ip)}, branch: {is_branch}, taken: {branch_taken}")
        print(f"dest_regs: {dest_regs}, src_regs: {src_regs}")
        print(f"dest_mem: {dest_mem}, src_mem: {src_mem}")
        print('-'*50)
