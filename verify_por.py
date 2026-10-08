import os

orig_dir = r'D:\workspace\Pool of Radiance\data\Dialogue'
restore_dir = 'restore_output'

files = [f for f in os.listdir(orig_dir) if f.endswith('.por')]
print(f'Total .por files in original: {len(files)}')

restore_files = [f for f in os.listdir(restore_dir) if f.endswith('.por')]
print(f'Total .por files in restore: {len(restore_files)}')

mismatches = 0
missing = 0
for f in files:
    orig_path = os.path.join(orig_dir, f)
    rest_path = os.path.join(restore_dir, f)
    if not os.path.exists(rest_path):
        print(f'MISSING: {f}')
        missing += 1
        continue
    with open(orig_path, 'rb') as fh1, open(rest_path, 'rb') as fh2:
        if fh1.read() != fh2.read():
            print(f'MISMATCH: {f}')
            mismatches += 1

print()
print(f"Missing files: {missing}")
print(f"Mismatched files: {mismatches}")
print(f"Matched files: {len(files) - missing - mismatches}/{len(files)}")