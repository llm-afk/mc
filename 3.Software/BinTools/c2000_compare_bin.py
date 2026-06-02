import xml.etree.ElementTree as ET
import base64
import sys
import os
import subprocess

def find_ofd2000():
    # Try common locations
    paths = [
        r"D:\ti\ccs1281\ccs\tools\compiler\ti-cgt-c2000_22.6.1.LTS\bin\ofd2000.exe",
        r"C:\ti\ccs1281\ccs\tools\compiler\ti-cgt-c2000_22.6.1.LTS\bin\ofd2000.exe",
    ]
    for p in paths:
        if os.path.exists(p):
            return p
            
    # Try looking in PATH
    try:
        res = subprocess.run(["where", "ofd2000"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if res.returncode == 0:
            return res.stdout.strip().split('\n')[0].strip()
    except Exception:
        pass
        
    # Search in C:\ti or D:\ti
    for drive in ["C:\\ti", "D:\\ti"]:
        if os.path.exists(drive):
            for root, dirs, files in os.walk(drive):
                if "ofd2000.exe" in files:
                    return os.path.join(root, "ofd2000.exe")
                    
    return "ofd2000.exe" # fallback to PATH

def compare_out_with_bin(out_path, bin_path):
    if not os.path.exists(out_path):
        print(f"Error: .out file {out_path} does not exist.")
        sys.exit(1)
    if not os.path.exists(bin_path):
        print(f"Error: .bin file {bin_path} does not exist.")
        sys.exit(1)

    ofd_path = find_ofd2000()
    print(f"Using OFD tool: {ofd_path}")

    # Generate temporary XML using cmd redirection to preserve UTF-8
    temp_xml = out_path + "_compare_ofd.xml"
    if os.path.exists(temp_xml):
        os.remove(temp_xml)
        
    cmd = f'"{ofd_path}" -x "{out_path}" > "{temp_xml}"'
    print(f"Generating temporary XML representation...")
    res = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if not os.path.exists(temp_xml) or os.path.getsize(temp_xml) == 0:
        print("Error: ofd2000 failed to generate XML.")
        print("Stdout:", res.stdout.decode(errors='ignore'))
        print("Stderr:", res.stderr.decode(errors='ignore'))
        sys.exit(1)

    try:
        tree = ET.parse(temp_xml)
        root = tree.getroot()
    except Exception as e:
        print(f"Error parsing generated XML: {e}")
        if os.path.exists(temp_xml):
            os.remove(temp_xml)
        sys.exit(1)

    sections = []
    for sec in root.findall('.//ti_coff/section'):
        name_node = sec.find('name')
        name = name_node.text if name_node is not None else "UNNAMED"
        
        is_copy = False
        copy_node = sec.find('copy')
        if copy_node is not None and copy_node.text.strip().lower() == 'true':
            is_copy = True
            
        vaddr_node = sec.find('virtual_addr')
        vaddr = int(vaddr_node.text, 16) if vaddr_node is not None and vaddr_node.text else None
        
        raw_data_node = sec.find('raw_data')
        has_raw = raw_data_node is not None and raw_data_node.text is not None
        
        if has_raw and not is_copy:
            data = base64.b64decode(raw_data_node.text.strip())
            sections.append({
                'name': name,
                'load_addr': vaddr,
                'data': data
            })

    # Clean up temp XML
    try:
        os.remove(temp_xml)
    except Exception:
        pass

    if not sections:
        print("Error: No loadable sections found in COFF file.")
        sys.exit(1)

    # Sort sections by load address
    sections.sort(key=lambda s: s['load_addr'])

    print("\n=== Loadable Sections from ELF/COFF ===")
    for sec in sections:
        end_addr = sec['load_addr'] + len(sec['data']) // 2
        print(f"  Section {sec['name']:<15} Address Span: {hex(sec['load_addr'])} - {hex(end_addr)} (words), Size: {len(sec['data'])} bytes")

    min_addr = sections[0]['load_addr']
    max_addr = max(sec['load_addr'] + len(sec['data']) // 2 for sec in sections)
    total_words = max_addr - min_addr
    total_bytes = total_words * 2
    
    print(f"\nExpected continuous memory span: {hex(min_addr)} - {hex(max_addr)} ({total_words} words, {total_bytes} bytes)")

    # Reconstruct reference continuous memory representation
    ref_mem = bytearray([0xFF] * total_bytes)
    for sec in sections:
        offset = (sec['load_addr'] - min_addr) * 2
        data = sec['data']
        ref_mem[offset:offset+len(data)] = data

    with open(bin_path, 'rb') as f:
        bin_data = f.read()

    print(f"Actual BIN file size: {len(bin_data)} bytes")

    # Byte-by-byte Comparison
    mismatches = []
    ref_len = len(ref_mem)
    bin_len = len(bin_data)
    max_len = max(ref_len, bin_len)

    for i in range(max_len):
        if i >= ref_len:
            mismatches.append((None, bin_data[i], f"Extra byte at BIN offset {i}"))
        elif i >= bin_len:
            mismatches.append((ref_mem[i], None, f"Missing byte in BIN at offset {i}"))
        else:
            r_byte = ref_mem[i]
            b_byte = bin_data[i]
            if r_byte != b_byte:
                mismatches.append((r_byte, b_byte, f"Value mismatch at offset {i}"))

    if not mismatches:
        print("\nSUCCESS: The generated BIN matches the ELF/COFF reference memory 100%!")
        sys.exit(0)
    else:
        print(f"\nFAILURE: Found {len(mismatches)} mismatches between ELF/COFF and BIN.")
        print(f"{'Offset':<8} {'Address (Word)':<16} {'Byte Type':<12} {'Ref (COFF)':<10} {'BIN':<10} {'Reason'}")
        
        # Display first 50 mismatches
        display_limit = 50
        for idx, (r_b, b_b, reason) in enumerate(mismatches):
            if idx >= display_limit:
                print(f"... and {len(mismatches) - display_limit} more mismatches omitted.")
                break
            
            # Find the offset in ref_mem or bin_data
            offset = idx
            word_addr = min_addr + offset // 2
            byte_type = "Low Byte" if (offset % 2 == 0) else "High Byte"
            
            r_str = f"0x{r_b:02X}" if r_b is not None else "N/A"
            b_str = f"0x{b_b:02X}" if b_b is not None else "N/A"
            
            print(f"{offset:<8} {hex(word_addr):<16} {byte_type:<12} {r_str:<10} {b_str:<10} {reason}")
            
        sys.exit(1)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python c2000_compare_bin.py <input_out_file> <input_bin_file>")
        sys.exit(1)
        
    compare_out_with_bin(sys.argv[1], sys.argv[2])
