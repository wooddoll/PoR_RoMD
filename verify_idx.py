import os

orig_path = r'D:\workspace\Pool of Radiance\data\Dialogue\all.idx'
rest_path = 'restore_output/all.idx'  # use forward slash

with open(orig_path, 'rb') as f1, open(rest_path, 'rb') as f2:
    orig = f1.read()
    rest = f2.read()

print(f"Original idx size: {len(orig)} bytes")
print(f"Restore idx size: {len(rest)} bytes")
print(f"Match: {orig == rest}")

if orig != rest:
    for i in range(min(len(orig), len(rest))):
        if orig[i] != rest[i]:
            print(f"First difference at byte {i}: orig={orig[i]}, rest={rest[i]}")
            break