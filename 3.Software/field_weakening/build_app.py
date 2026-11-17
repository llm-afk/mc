"""Build an APP-only OTA image with the original TI toolchain. No hardware access."""
import argparse
import concurrent.futures
from pathlib import Path
import subprocess
import sys
from xml.etree import ElementTree

ROOT=Path(__file__).resolve().parents[2]
TOOLS=ROOT/"3.Software/BinTools"

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler",type=Path,default=Path("D:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS"))
    parser.add_argument("--app-dir",type=Path,default=ROOT/"2.Firmware/ADM32F036_mc_app")
    parser.add_argument("--output",type=Path)
    args=parser.parse_args()
    app=args.app_dir.resolve(strict=True)
    out=(args.output or app/"Debug/245_b").resolve()
    out.mkdir(parents=True,exist_ok=True)
    compiler=args.compiler/"bin/cl2000.exe"
    if not compiler.is_file():parser.error("TI C2000 CGT 22.6.1.LTS is required")
    cfg=ElementTree.parse(app/".cproject").find(".//configuration[@name='Debug']")
    option=next(o for o in cfg.iter("option") if o.get("superClass","").endswith(".compilerID.DEFINE"))
    defines=["--define="+v.get("value") for v in option.findall("listOptionValue")]
    flags=["-v28","-ml","-mt","--cla_support=cla0","-O4","--opt_for_speed=5",
           "--abi=coffabi","--c11","-g","--diag_warning=225","--display_error_number"]
    includes=["--include_path="+str(p) for p in
              (app,app/"HeardFiles/SourceHeader",app/"HeardFiles/ADP32F03xHeader",app/"user",args.compiler/"include")]
    def compile_one(source):
        obj=out/source.relative_to(app).with_suffix(".obj")
        obj.parent.mkdir(parents=True,exist_ok=True)
        result=subprocess.run([compiler,*flags,*defines,*includes,"--compile_only","--output_file="+str(obj),source],
                              cwd=out,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        obj.with_suffix(".log").write_bytes(result.stdout)
        if result.returncode:raise RuntimeError(result.stdout.decode(errors="replace"))
        return obj
    sources=sorted(p for directory in ("Function","Main","Motor/src","SubProgra","user")
                   for p in (app/directory).iterdir() if p.suffix.lower() in (".c",".asm"))
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:objects=list(pool.map(compile_one,sources))
    stem=out/"ADM32F036_mc_app"
    image,binary,package=Path(str(stem)+".out"),Path(str(stem)+".bin"),Path(str(stem)+"_released_fw.bin")
    subprocess.run([compiler,*flags,"-z","--stack_size=0x300","--warn_sections",
        "-i"+str(args.compiler/"lib"),"-i"+str(app/"user"),"--reread_libs","--entry_point=code_start",
        "--rom_model","-m"+str(stem)+".map","-o",image,*objects,*sorted((app/"Motor/obj").glob("*.obj")),
        app/"DSP28035Peripheral.cmd",app/"F28036.cmd",app/"user/ADP32F036_027_Flash_API_ZONE.lib","-llibc.a"],cwd=out,check=True)
    subprocess.run([sys.executable,TOOLS/"c2000_fill_gaps.py",image,binary],check=True)
    subprocess.run([sys.executable,TOOLS/"c2000_compare_bin.py",image,binary],check=True)
    packer=TOOLS/"bin/bin_packer.exe"
    pack_args=["-i",binary,"-o",package,"-s","48KB","-vf",app/"HeardFiles/SourceHeader/Define.h"]
    try:subprocess.run([packer,*pack_args],cwd=ROOT,check=True)
    except OSError as error:
        if getattr(error,"winerror",0) not in (193,623):raise
        packer=out/"bin_packer.exe"
        subprocess.run(["gcc","-O2",TOOLS/"src/bin_packer.c","-o",packer],check=True)
        subprocess.run([packer,*pack_args],cwd=ROOT,check=True)
    subprocess.run([packer,"--verify","-i",package],check=True)
    print(package)

if __name__=="__main__":main()
