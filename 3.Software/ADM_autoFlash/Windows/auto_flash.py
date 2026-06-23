import subprocess, time, os, sys, configparser, glob, re
from datetime import datetime

# 确保工作目录在脚本所在目录（管理员提权后 CWD 会变成 System32）
os.chdir(os.path.dirname(os.path.abspath(__file__)))

# ============================================================
# 加载配置
# ============================================================
cfg = configparser.ConfigParser()
# 如果不存在 config.ini，使用空配置，依赖下方 fallback
if os.path.exists("config.ini"):
    cfg.read("config.ini", encoding="utf-8")

# ============================================================
# 自动定位烧录工具依赖
# ============================================================
# 回退路径 (系统环境)
fallback_dslite = r"C:\ti\uniflash_9.3.0\dslite.bat"
fallback_dbgjtag = r"C:\ti\uniflash_9.3.0\deskdb\content\TICloudAgent\win\ccs_base\common\uscif\dbgjtag.exe"

# 1. 尝试寻找本地绿色包的 DSLite
local_dslite = None
local_base = None
for base_dir in [".", "uniflash_windows", "uniflash_windows_cli"]:
    # 尝试寻找底层的 DSLite.exe
    inner_exe = os.path.abspath(os.path.join(base_dir, "ccs_base", "DebugServer", "bin", "DSLite.exe"))
    if os.path.exists(inner_exe):
        local_base = base_dir
        local_dslite = inner_exe
        break
    
    # 作为备用，找脚本
    dslite_scripts = glob.glob(os.path.join(base_dir, "dslite*.bat"))
    if dslite_scripts:
        local_base = base_dir
        local_dslite = os.path.abspath(dslite_scripts[0])
        break

# 2. 定位 DSLite
if local_dslite:
    DSLITE = local_dslite
    env_mode = "本地绿色包"
else:
    env_mode = "系统"
    dslite_cfg = cfg.get("paths", "dslite", fallback=fallback_dslite)
    if dslite_cfg.endswith(".bat"):
        DSLITE = os.path.join(os.path.dirname(dslite_cfg), "deskdb", "content", "TICloudAgent", "win", "ccs_base", "DebugServer", "bin", "DSLite.exe")
        if not os.path.exists(DSLITE):
            DSLITE = dslite_cfg
    else:
        DSLITE = dslite_cfg

# 3. 定位 DBGJTAG (优先找本地，本地没有就强制用系统，因为TI导出时常漏掉此文件)
DBGJTAG = cfg.get("paths", "dbgjtag", fallback=fallback_dbgjtag)
if local_base:
    local_dbgjtag = os.path.abspath(os.path.join(local_base, "ccs_base", "common", "uscif", "dbgjtag.exe"))
    if os.path.exists(local_dbgjtag):
        DBGJTAG = local_dbgjtag

# XDSDFU 通常位于 dbgjtag 同级目录下的 xds110 文件夹内
XDSDFU = os.path.join(os.path.dirname(DBGJTAG), "xds110", "xdsdfu.exe")

# ============================================================
# XDS110 驱动检测与自动安装
# ============================================================
def is_admin():
    try:
        import ctypes as _ctypes
        return _ctypes.windll.shell32.IsUserAnAdmin() != 0
    except:
        return False

def xds110_driver_installed():
    """检查 Windows 驱动仓库中是否已有 XDS110 驱动"""
    try:
        driver_store = os.path.join(
            os.environ.get("SystemRoot", r"C:\Windows"),
            "System32", "DriverStore", "FileRepository"
        )
        if not os.path.exists(driver_store):
            return False
        for entry in os.listdir(driver_store):
            if "xds110" in entry.lower():
                return True
    except:
        pass
    return False

def ensure_xds110_driver():
    """确保 XDS110 驱动已安装；如未安装且当前无管理员权限则自动提权"""
    if xds110_driver_installed():
        return  # 驱动已安装，无需操作

    if not is_admin():
        log("检测到 XDS110 驱动未安装，正在请求管理员权限...", "INFO")
        import ctypes as _ctypes
        _ctypes.windll.shell32.ShellExecuteW(
            None, "runas", sys.executable,
            '"' + os.path.abspath(__file__) + '"', None, 1
        )
        sys.exit(0)

    # 已获得管理员权限，执行驱动安装
    log("正在安装 XDS110 驱动 (静默模式)...", "INFO")
    dpinst = os.path.abspath("uniflash_windows/dpinst_64_eng.exe")
    xds110_drv = os.path.abspath("uniflash_windows/ccs_base/emulation/windows/xds110_drivers")
    icdi_drv   = os.path.abspath("uniflash_windows/ccs_base/emulation/windows/icdi_drivers")

    if not os.path.exists(dpinst):
        log(f"错误: 找不到 dpinst_64_eng.exe", "ERROR")
        return

    try:
        subprocess.run([dpinst, "/SE", "/SW", "/SA", "/PATH", xds110_drv],
                       capture_output=True, timeout=60, creationflags=0x08000000)
        subprocess.run([dpinst, "/SE", "/SW", "/SA", "/PATH", icdi_drv],
                       capture_output=True, timeout=60, creationflags=0x08000000)

        if xds110_driver_installed():
            log("XDS110 驱动安装成功", "SUCCESS")
        else:
            log("驱动安装可能未生效，请手工以管理员身份运行 uniflash_windows\\one_time_setup.bat", "WARN")
    except Exception as e:
        log(f"驱动安装失败: {e}", "ERROR")

# ============================================================
# 自动查找目标配置文件 (.ccxml)
# ============================================================
CCXML = cfg.get("paths", "ccxml", fallback="target.ccxml")
if not os.path.exists(CCXML):
    # 尝试在 user_files/configs 目录找
    ccxml_files = glob.glob("user_files/configs/*.ccxml")
    if ccxml_files:
        CCXML = ccxml_files[0]
    else:
        # 在当前目录找
        local_ccxml = glob.glob("*.ccxml")
        if local_ccxml:
            CCXML = local_ccxml[0]

# 固件与地址配置
BOOT_PATTERN  = cfg.get("firmware", "boot", fallback="boot*.bin")
BOOT_ADDR = cfg.get("firmware", "boot_addr", fallback="0x3F6000")
APP_PATTERN   = cfg.get("firmware", "app", fallback="app*.bin")
APP_ADDR = cfg.get("firmware", "app_addr", fallback="0x3E8000")

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
    if lvl in ["INFO", "WAIT", "READY"]:
        tag = "INFO "
    elif lvl in ["SUCCESS", "RESULT"]:
        tag = "DONE "
    elif lvl in ["ERROR", "FAILED"]:
        tag = "ERR  "
    elif lvl == "RAW":
        tag = "RAW  "
    else:
        tag = lvl.ljust(5)
    print(f"[{ts()}][{tag}] {msg}")

def result_block(ok, msg1, msg2=""):
    level = "SUCCESS" if ok else "FAILED"
    print() # 保持间距
    log(msg1, level)
    if msg2: log(msg2, level)
    print()

# ============================================================
# 硬件检测
# ============================================================
def check_hw(fast_mode=False):
    try:
        # 1. 检查仿真器 USB 连接（超时 1s，失败重试 1 次，避免 USB 繁忙时误判离线）
        if os.path.exists(XDSDFU):
            hw_ok = False
            for attempt in range(2):
                try:
                    hw = subprocess.run([XDSDFU, "-e"], capture_output=True, text=True, timeout=1.0, creationflags=0x08000000)
                    if "Found 1" in hw.stdout or "Found 2" in hw.stdout:
                        hw_ok = True
                        break
                except Exception:
                    pass
            if not hw_ok: return False, False, False
        else:
            # 如果找不到 xdsdfu，默认仿真器连接正常，交由 dbgjtag 检测
            hw_ok = True
        
        # 2. 执行 JTAG 检测
        cmd_type = "pathlength" if fast_mode else "integrity"
        timeout_val = 0.5 if fast_mode else 1.2
        
        jt = subprocess.run([DBGJTAG, "-f", "@xds110", "-S", cmd_type], capture_output=True, text=True, timeout=timeout_val, creationflags=0x08000000)
        
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
    except Exception: return False, False, False

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
        
    # 构建 DSLite 烧录命令：-e 表示全片擦除，-v 表示校验，-u 表示烧录完复位并运行
    cmd = [DSLITE, "flash", f"--config={ccxml}", "-e", "-v", "-f", boot_arg, app_arg, "-u"]
    
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         text=True, encoding="gbk", errors="ignore", creationflags=0x08000000)
    
    # DSLite 初始化时会输出大量 XML 解析和寄存器映射信息，全部过滤掉
    NOISE_PREFIXES = (
        "Parsing ",
        "Mapping registers:",
        "Initializing Register Database",
        "Creating tables and indexes",
        "Configuring Debugger",
        "Executing Startup Scripts",
        "Initializing: ",
        "DSLite version",
    )

    success = False
    for line in p.stdout:
        line = line.strip()
        if not line:
            continue
        # 跳过所有初始化噪音行
        if any(line.startswith(prefix) for prefix in NOISE_PREFIXES):
            continue
        print(f"[{ts()}][RAW  ] {line}")
        if "Program verification successful" in line:
            success = True
    p.wait()
    return success and (p.returncode == 0)

# ============================================================
# 探针断开检测
# ============================================================
def wait_unplug():
    time.sleep(0.5)
    log("等待探针抬起...", "INFO")
    
    physical_disconnect_cnt = 0
    while True:
        _, jt_ok, is_physical = check_hw(fast_mode=False)
        if not is_physical:
            physical_disconnect_cnt += 1
        else:
            physical_disconnect_cnt = 0
            
        if physical_disconnect_cnt >= 3:
            log("检测到探针已断开", "INFO")
            time.sleep(0.3)
            return
        time.sleep(0.1)

# ============================================================
# 主循环
# ============================================================
def main():
    log("ADM32F03X 自动烧录脚本启动", "INFO")
    log(f"运行环境: {env_mode}", "INFO")

    # --- 驱动检查 ---
    ensure_xds110_driver()

    # --- 工具链信息 ---
    log(f"DSLite : {DSLITE}", "INFO")
    log(f"dbgjtag: {DBGJTAG}", "INFO")
    log(f"xdsdfu : {XDSDFU}", "INFO")

    # --- 目标配置 ---
    if os.path.exists(CCXML):
        log(f"目标配置: {CCXML}", "INFO")
    else:
        log(f"警告: 找不到目标配置文件 {CCXML}", "WARN")

    # --- 固件信息 ---
    boot = resolve_file(BOOT_PATTERN)
    app  = resolve_file(APP_PATTERN)

    if boot:
        log(f"Boot 固件: {os.path.basename(boot)}  版本: {extract_version(boot)}  地址: {BOOT_ADDR}", "INFO")
    else:
        log(f"错误: 未找到 Boot 固件 (匹配: {BOOT_PATTERN})", "ERROR")

    if app:
        log(f"App  固件: {os.path.basename(app)}   版本: {extract_version(app)}   地址: {APP_ADDR}", "INFO")
    else:
        log(f"错误: 未找到 App 固件 (匹配: {APP_PATTERN})", "ERROR")

    # 等待 USB 枚举就绪，避免启动瞬间误报"未检测到仿真器"
    time.sleep(0.5)

    state = None
    seen = set()             # 每个等待状态只输出一次，不刷屏
    no_probe_cnt = 0         # 防抖计数：连续多少次未检测到仿真器
    NO_PROBE_DEBOUNCE = 8    # 连续 8 次 (~0.8s) 才报错，给 USB 足够恢复时间
    while True:
        hw, jt_ok, is_physical = check_hw(fast_mode=False)

        if not hw:
            no_probe_cnt += 1
            if no_probe_cnt < NO_PROBE_DEBOUNCE:
                time.sleep(0.1)
                continue
            new_state = "NO_PROBE"
            msg = "未检测到仿真器 (Debug probe not found)"
            level = "ERROR"
        elif not is_physical:
            no_probe_cnt = 0
            new_state = "WAIT_BOARD"
            msg = "等待探针接触目标板..."
            level = "INFO"
        else:
            no_probe_cnt = 0
            new_state = "READY"
            msg = "探针已接触，开始烧录..."
            level = "INFO"

        if new_state != state:
            if new_state == "READY" or new_state not in seen:
                log(msg, level)
                seen.add(new_state)
            state = new_state

        if new_state == "READY":
            # 延迟 1.5 秒，给底层 USB 驱动和探针固件充分的时间释放句柄和初始化，避免 -260 错误
            time.sleep(1.5)
            if flash_production(CCXML, BOOT_PATTERN, BOOT_ADDR, APP_PATTERN, APP_ADDR):
                result_block(True, "Boot + App 烧录成功并已启动", "请松开探针")
            else:
                result_block(False, "烧录失败", "请重新压紧探针")

            wait_unplug()
            state = None
            seen.clear()       # 新一轮烧录，重置

        time.sleep(0.1)

if __name__ == "__main__":
    try: main()
    except KeyboardInterrupt: log("系统退出", "INFO")
