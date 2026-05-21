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

def main():
    if len(sys.argv) < 3:
        print("Usage: python c2000_fill_gaps.py <input_out_file> <output_bin_file>")
        sys.exit(1)
        
    out_file = sys.argv[1]
    bin_file = sys.argv[2]
    
    if not os.path.exists(out_file):
        print(f"Error: input file {out_file} does not exist.")
        sys.exit(1)
        
    ofd_path = find_ofd2000()
    print(f"Using OFD tool: {ofd_path}")
    
    # Generate temporary XML using cmd redirection to preserve UTF-8
    temp_xml = out_file + "_temp_ofd.xml"
    if os.path.exists(temp_xml):
        os.remove(temp_xml)
        
    cmd = f'"{ofd_path}" -x "{out_file}" > "{temp_xml}"'
    print(f"Running command: {cmd}")
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
        # Try to clean up and exit
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
        
    # Sort by load address
    sections.sort(key=lambda s: s['load_addr'])
    
    min_addr = sections[0]['load_addr']
    max_addr = max(sec['load_addr'] + len(sec['data']) // 2 for sec in sections)
    total_words = max_addr - min_addr
    total_bytes = total_words * 2
    
    print(f"Reconstructing binary from {hex(min_addr)} to {hex(max_addr)} ({total_words} words, {total_bytes} bytes)")
    
    # Build the binary buffer
    bin_buf = bytearray([0xFF] * total_bytes)
    for sec in sections:
        offset = (sec['load_addr'] - min_addr) * 2
        data = sec['data']
        bin_buf[offset:offset+len(data)] = data
        print(f"  Placed section {sec['name']:<15} at offset {offset:<6} (Addr: {hex(sec['load_addr'])}) size: {len(data)} bytes")
        
    # Write the output file
    with open(bin_file, 'wb') as f:
        f.write(bin_buf)
    print(f"Successfully generated gap-filled binary: {bin_file} (Size: {len(bin_buf)} bytes)")

if __name__ == '__main__':
    main()
