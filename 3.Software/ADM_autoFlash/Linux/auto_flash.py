import subprocess, time, os, configparser, glob, re, sys, traceback
from datetime import datetime

# ============================================================
# 加载配置
# ============================================================
cfg = configparser.ConfigParser()
if os.path.exists("config.ini"):
    cfg.read("config.ini", encoding="utf-8")

# 固件与地址配置
BOOT_PATTERN  = cfg.get("firmware", "boot", fallback="boot*.bin")
BOOT_ADDR = cfg.get("firmware", "boot_addr", fallback="0x3F6000")
APP_PATTERN   = cfg.get("firmware", "app", fallback="app*.bin")
APP_ADDR = cfg.get("firmware", "app_addr", fallback="0x3E8000")
CCXML = cfg.get("paths", "ccxml", fallback="target.ccxml")

# 寻找配置文件
if not os.path.exists(CCXML):
    ccxml_files = glob.glob("user_files/configs/*.ccxml")
    if ccxml_files:
        CCXML = ccxml_files[0]
    else:
        local_ccxml = glob.glob("*.ccxml")
        if local_ccxml:
            CCXML = local_ccxml[0]

def resolve_file(pattern):
    files = glob.glob(pattern)
    if not files: return None
    files.sort()
    return files[-1]

def extract_version(filename):
    m = re.search(r'(\d+)\.bin$', filename.lower())
    if m:
        num = m.group(1)
        if len(num) == 3:
            return f"v{num[0]}.{num[1]}.{num[2]}"
        return f"v{num}"
    return "未知"

def ts(): return datetime.now().strftime("%H:%M:%S")

def log(msg, level="INFO"):
    lvl = level.upper().strip()
    tag = lvl.ljust(5)
    print(f"[{ts()}][{tag}] {msg}")

def result_block(ok, msg1, msg2=""):
    level = "SUCCESS" if ok else "FAILED"
    print()
    log(msg1, level)
    if msg2: log(msg2, level)
    print()

# ============================================================
# 自动定位烧录工具依赖 (Linux 专属)
# ============================================================
local_base = None
local_dslite = None

for base_dir in [".", "uniflash_linux", "uniflash_linux_cli"]:
    dslite_scripts = glob.glob(os.path.join(base_dir, "dslite*.sh"))
    if dslite_scripts:
        local_base = base_dir
        local_dslite = os.path.abspath(dslite_scripts[0])
        break

# 动态寻找系统的 fallback
ti_opt = glob.glob("/opt/ti/uniflash*")
if ti_opt:
    ti_opt.sort()
    ti_base = ti_opt[-1]
    fallback_dslite = f"{ti_base}/dslite.sh"
    fallback_dbgjtag = f"{ti_base}/deskdb/content/TICloudAgent/linux/ccs_base/common/uscif/dbgjtag"
else:
    fallback_dslite = "/opt/ti/uniflash/dslite.sh"
    fallback_dbgjtag = "/opt/ti/uniflash/deskdb/content/TICloudAgent/linux/ccs_base/common/uscif/dbgjtag"

DSLITE = local_dslite if local_dslite else fallback_dslite
DBGTJAG = fallback_dbgjtag
env_mode = "本地绿色包" if local_dslite else "系统"

if local_base:
    local_dbgjtag = os.path.abspath(os.path.join(local_base, "ccs_base", "common", "uscif", "dbgjtag"))
    if os.path.exists(local_dbgjtag):
        DBGTJAG = local_dbgjtag

XDSDFU = os.path.join(os.path.dirname(DBGTJAG), "xds110", "xdsdfu")

# 为了确保 Linux 下有执行权限，强制赋予 x 权限
try:
    if os.path.exists(DBGTJAG):
        os.chmod(DBGTJAG, 0o755)
    if os.path.exists(XDSDFU):
        os.chmod(XDSDFU, 0o755)
except Exception:
    pass

# ============================================================
# 硬件检测
# ============================================================
def check_hw(fast_mode=False):
    try:
        hw_ok = True
        
        # 1. 检查仿真器 USB 连接 (如果存在 xdsdfu)
        if os.path.exists(XDSDFU):
            hw_res = subprocess.run([XDSDFU, "-e"], capture_output=True, text=True, timeout=1.0)
            hw_ok = "Found 1" in hw_res.stdout or "Found 2" in hw_res.stdout
            if not hw_ok:
                return False, False, False
        else:
            hw_ok = True
        
        # 2. 执行 JTAG 检测
        cmd_type = "pathlength" if fast_mode else "integrity"
        timeout_val = 1.0 if fast_mode else 3.0
        
        if not os.path.exists(DBGTJAG):
            return False, False, False
            
        jt = subprocess.run([DBGTJAG, "-f", "@xds110", "-S", cmd_type], capture_output=True, text=True, timeout=timeout_val)
        
        stdout_msg = (jt.stdout + jt.stderr).lower()
        jt_ok = "succeeded" in stdout_msg
            
        # 3. 判定物理接触状态
        if jt_ok:
            is_physical = True
        else:
            if "voltage" in stdout_msg or "cannot find" in stdout_msg or "error" in stdout_msg:
                if "path length" in stdout_msg and "found" in stdout_msg:
                    is_physical = True
                else:
                    is_physical = False
            else:
                is_physical = False

        return hw_ok, jt_ok, is_physical
        
    except Exception as e:
        return False, False, False

# ============================================================
# 烧录流程
# ============================================================
def flash_production(ccxml, boot_pattern, boot_addr, app_pattern, app_addr):
    boot = resolve_file(boot_pattern)
    app = resolve_file(app_pattern)
    
    if not boot:
        log(f"错误: 找不到符合条件的 Boot 文件 '{boot_pattern}'", "ERROR")
        return False
    if not app:
        log(f"错误: 找不到符合条件的 App 文件 '{app_pattern}'", "ERROR")
        return False
        
    app_ver = extract_version(app)
    boot_ver = extract_version(boot)
    
    log(f"正在执行合并烧录并启动...", "INFO")
    log(f"-> 烧录版本 App: {app_ver}  | Boot: {boot_ver}", "INFO")
    log(f"-> 烧录文件: {os.path.basename(app)} + {os.path.basename(boot)}", "INFO")
        
    # 针对 bin 文件自动拼接地址
    boot_arg = boot
    if boot.lower().endswith(".bin"):
        boot_arg = f"{boot},{boot_addr}"
        
    app_arg = app
    if app.lower().endswith(".bin"):
        app_arg = f"{app},{app_addr}"
        
    cmd = [DSLITE, f"--config={ccxml}", "-e", "-v", "-f", boot_arg, app_arg, "-u"]
    if not DSLITE.endswith(".sh"):
        cmd.insert(1, "flash")
    
    try:
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        ok = False
        for line in proc.stdout:
            line = line.strip()
            if not line: continue
            
            # 精简输出：过滤掉无意义的底层加载日志
            if "Mapping registers" in line or "Initializing:" in line or "Executing Startup Scripts" in line or "Configuring Debugger" in line or "Parsing" in line:
                continue
                
            log(line, "RAW")
            if "Success" in line or "Program verification successful" in line:
                ok = True
            if "error" in line.lower() or "failed" in line.lower() or "fatal" in line.lower():
                pass
        proc.wait()
        return ok and (proc.returncode == 0)
    except Exception as e:
        log(f"调用烧录工具时抛出异常: {e}", "ERROR")
        return False

# ============================================================
# 主循环
# ============================================================
def main():
    log("TMS320F28035 自动烧录脚本启动 (Linux 版)", "INFO")
    log(f"当前使用的运行环境: [{env_mode}]", "INFO")
    
    if not os.path.exists(CCXML):
        log(f"警告: 找不到配置文件 {CCXML}", "ERROR")
        
    state = None
    while True:
        hw, jt_ok, is_physical = check_hw(fast_mode=False)
        
        if not hw: 
            new_state = "NO_PROBE"
            msg = "未检测到仿真器 (Debug probe not found)"
            level = "ERROR"
        elif not is_physical: 
            new_state = "WAIT_BOARD"
            msg = "请将探针压紧至目标板..."
            level = "INFO"
        else: 
            new_state = "READY"
            msg = "探针已接触，开始烧录..."
            level = "INFO"

        if new_state != state: 
            log(msg, level)
            state = new_state

        if new_state == "READY":
            time.sleep(1.5)
            if flash_production(CCXML, BOOT_PATTERN, BOOT_ADDR, APP_PATTERN, APP_ADDR):
                result_block(True, "Boot + App 烧录成功并已启动", "请松开探针")
            else:
                result_block(False, "烧录失败", "请重新压紧探针")
            
            # 等待拔出探针
            time.sleep(0.5)
            log("等待探针抬起...", "INFO")
            disconnect_cnt = 0
            while True:
                _, _, physical_check = check_hw(fast_mode=False)
                if not physical_check:
                    disconnect_cnt += 1
                else:
                    disconnect_cnt = 0
                    
                if disconnect_cnt >= 3:
                    log("检测到探针已断开", "INFO")
                    time.sleep(0.3)
                    state = None # 触发下一轮检测状态更新
                    break
                time.sleep(0.1)
                
        time.sleep(0.2)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        log("用户终止程序", "INFO")

