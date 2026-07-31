import hashlib
import json
import os
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path


if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")

# The AMD Windows loader silently exits before sdtgen/lopper when this normal
# cmd.exe variable is absent from a restricted parent environment.
os.environ.setdefault("PROCESSOR_ARCHITECTURE", "AMD64")


WORKSPACE = Path(__file__).resolve().parent
XSA = WORKSPACE.parent / "adc_easy_test" / "design_1_wrapper.xsa"
VITIS_ROOT = Path(
    os.environ.get("XILINX_VITIS", r"F:\AMDDesignTools\2025.2\Vitis")
)
VIVADO_ROOT = Path(
    os.environ.get("XILINX_VIVADO", str(VITIS_ROOT.parent / "Vivado"))
)

if not XSA.is_file():
    raise FileNotFoundError("Hardware platform XSA was not found: " + str(XSA))
if not VITIS_ROOT.is_dir():
    raise FileNotFoundError("Vitis installation was not found: " + str(VITIS_ROOT))
if not VIVADO_ROOT.is_dir():
    raise FileNotFoundError("Vivado installation was not found: " + str(VIVADO_ROOT))

# Vitis 2025.2's Windows server does not always inherit the directory that
# contains its bundled dtc/lopper helpers.  Put it on PATH before any domain
# regeneration so the script also works outside the Unified IDE.
os.environ["XILINX_VITIS"] = str(VITIS_ROOT)
os.environ["XILINX_VIVADO"] = str(VIVADO_ROOT)
dtc = VIVADO_ROOT / "bin" / "dtc.exe"
if dtc.is_file():
    os.environ["LOPPER_DTC"] = str(dtc)
else:
    os.environ.pop("LOPPER_DTC", None)

cpp = VITIS_ROOT / "tps" / "mingw" / "10.0.0" / "win64.o" / "nt" / "bin" / "cpp.exe"
if cpp.is_file():
    os.environ["LOPPER_CPP"] = str(cpp)
else:
    os.environ.pop("LOPPER_CPP", None)

path_prefix = [
    VIVADO_ROOT / "bin",
    VITIS_ROOT / "bin",
    VITIS_ROOT / "tps" / "mingw" / "10.0.0" / "win64.o" / "nt" / "bin",
]
os.environ["PATH"] = os.pathsep.join(
    [str(path) for path in path_prefix if path.is_dir()]
    + [os.environ.get("PATH", "")]
)

# Make the Unified IDE Python API available from a normal PowerShell Python
# process.  The vendor launcher adds these paths implicitly, but invoking this
# repository script directly does not.
vitis_python_paths = [
    VITIS_ROOT / "cli",
    VITIS_ROOT / "cli" / "proto",
    VITIS_ROOT / "cli" / "python-packages" / "win64",
    VITIS_ROOT / "cli" / "python-packages" / "site-packages",
]
for python_path in reversed(vitis_python_paths):
    if python_path.is_dir() and str(python_path) not in sys.path:
        sys.path.insert(0, str(python_path))
os.environ["PYTHONPATH"] = os.pathsep.join(
    [str(path) for path in vitis_python_paths if path.is_dir()]
    + [os.environ.get("PYTHONPATH", "")]
)


platform_dir = WORKSPACE / "adctestp"
platform_hw_dir = platform_dir / "hw"
platform_xsa = platform_hw_dir / "design_1_wrapper.xsa"
sdt_dir = platform_hw_dir / "sdt"
export_hw_dir = platform_dir / "export" / "adctestp" / "hw"
sdtgen = VIVADO_ROOT / "bin" / "sdtgen.bat"
platformutil = VITIS_ROOT / "vitis-server" / "scripts" / "platformutil.tcl"
for required_tool in (sdtgen, platformutil):
    if not required_tool.is_file():
        raise FileNotFoundError("Required platform tool was not found: " + str(required_tool))

sdt_dir.mkdir(parents=True, exist_ok=True)
print("Generating platform SDT from: " + str(XSA))
subprocess.run(
    [
        "cmd.exe",
        "/d",
        "/c",
        str(sdtgen),
        str(platformutil),
        str(XSA),
        str(sdt_dir),
    ],
    check=True,
)
shutil.copy2(XSA, platform_xsa)

component_json_path = platform_dir / "vitis-comp.json"
component_data = json.loads(component_json_path.read_text(encoding="utf-8"))
component_data["configuration"]["xsa"] = XSA.as_posix()
component_json_path.write_text(
    json.dumps(component_data, indent=2) + "\n", encoding="utf-8"
)

with zipfile.ZipFile(XSA, "r") as archive:
    expected_ps7_init_hash = hashlib.sha256(archive.read("ps7_init.c")).hexdigest()

generated_ps7_init = WORKSPACE / "adctestp" / "hw" / "sdt" / "ps7_init.c"
if not generated_ps7_init.is_file():
    raise FileNotFoundError(
        "Updated SDT ps7_init.c was not generated: " + str(generated_ps7_init)
    )
if hashlib.sha256(generated_ps7_init.read_bytes()).hexdigest() != expected_ps7_init_hash:
    raise RuntimeError("Updated SDT ps7_init.c does not match the source XSA")

fsbl_source_dir = platform_dir / "zynq_fsbl"


def prepare_fsbl_sources():
    # The FSBL component compiles copies in its own source directory.  Domain
    # regeneration can leave them stale, making JTAG and SD configure the PS
    # differently, so synchronize them after every regeneration.
    if not fsbl_source_dir.is_dir():
        raise FileNotFoundError(
            "Generated FSBL source directory was not found: "
            + str(fsbl_source_dir)
        )
    for filename in ("ps7_init.c", "ps7_init.h"):
        generated_file = sdt_dir / filename
        if not generated_file.is_file():
            raise FileNotFoundError(
                "Generated PS7 init file was not found: " + str(generated_file)
            )
        shutil.copy2(generated_file, fsbl_source_dir / filename)

    # Vitis 2025.2 sometimes emits CMAKE_SIZE as a bare command while the ARM
    # tool directory is absent from cmd.exe's PATH.  Patch the generated FSBL
    # project deterministically so a clean platform build remains reproducible.
    cmake_file = fsbl_source_dir / "CMakeLists.txt"
    cmake_text = cmake_file.read_text(encoding="utf-8")
    marker = "get_filename_component(_arm_tool_dir"
    if marker not in cmake_text:
        size_call = "print_elf_size(CMAKE_SIZE ${APP_NAME})"
        if size_call not in cmake_text:
            raise RuntimeError(
                "Cannot locate the FSBL size-tool hook in " + str(cmake_file)
            )
        size_fix = """# Resolve the ARM size tool even when Vitis does not export its directory.
get_filename_component(_arm_tool_dir "${CMAKE_C_COMPILER}" DIRECTORY)
find_program(_arm_size_tool
    NAMES arm-none-eabi-size
    HINTS "${_arm_tool_dir}"
    NO_DEFAULT_PATH)
if(_arm_size_tool)
    set(CMAKE_SIZE "${_arm_size_tool}")
endif()

print_elf_size(CMAKE_SIZE ${APP_NAME})"""
        cmake_file.write_text(
            cmake_text.replace(size_call, size_fix),
            encoding="utf-8",
        )


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


dtc_path = os.environ.get("LOPPER_DTC") or shutil.which("dtc.exe") or shutil.which("dtc")
if not dtc_path:
    raise RuntimeError(
        "Vitis platform regeneration requires dtc; refusing to reuse stale BSP/FSBL outputs"
    )

import vitis

client = vitis.create_client()
try:
    client.set_workspace(str(WORKSPACE))
    platform = client.get_component(name="adctestp")
    if platform is None:
        raise RuntimeError("Vitis platform component 'adctestp' was not found")

    for domain_name in ("zynq_fsbl", "standalone_ps7_cortexa9_0"):
        print("Regenerating domain: " + domain_name)
        domain = platform.get_domain(name=domain_name)
        if domain is None:
            raise RuntimeError("Vitis domain was not found: " + domain_name)
        domain.regenerate()

    prepare_fsbl_sources()
    build_status = platform.build()
    if build_status != 0:
        raise RuntimeError("Platform build failed: " + str(build_status))
finally:
    vitis.dispose()
print("PLATFORM_DOMAIN_REGEN=FULL")

# Vitis also emits convenience copies outside the SDT and BSP directories.
# Keep every exported PS7 initializer aligned with the XSA used for this build.
for destination_dir in (platform_hw_dir, export_hw_dir):
    destination_dir.mkdir(parents=True, exist_ok=True)
    for generated_file in sdt_dir.glob("ps7_init.*"):
        shutil.copy2(generated_file, destination_dir / generated_file.name)

platform_xsa = WORKSPACE / "adctestp" / "hw" / "design_1_wrapper.xsa"
if not platform_xsa.is_file():
    raise FileNotFoundError("Refreshed platform XSA was not found: " + str(platform_xsa))

source_hash = sha256(XSA)
platform_hash = sha256(platform_xsa)
if source_hash != platform_hash:
    raise RuntimeError(
        "Platform XSA hash mismatch: source="
        + source_hash
        + " platform="
        + platform_hash
    )

generated_ps7_init_files = [
    generated_ps7_init,
    platform_hw_dir / "ps7_init.c",
    export_hw_dir / "ps7_init.c",
    export_hw_dir / "sdt" / "ps7_init.c",
    fsbl_source_dir / "ps7_init.c",
    platform_dir / "zynq_fsbl" / "zynq_fsbl_bsp" / "hw_artifacts" / "ps7_init.c",
    platform_dir
    / "ps7_cortexa9_0"
    / "standalone_ps7_cortexa9_0"
    / "bsp"
    / "hw_artifacts"
    / "ps7_init.c",
    platform_dir
    / "export"
    / "adctestp"
    / "sw"
    / "standalone_ps7_cortexa9_0"
    / "hw_artifacts"
    / "ps7_init.c",
]
for generated_file in generated_ps7_init_files:
    if not generated_file.is_file():
        raise FileNotFoundError("Generated ps7_init.c was not found: " + str(generated_file))
    if hashlib.sha256(generated_file.read_bytes()).hexdigest() != expected_ps7_init_hash:
        raise RuntimeError("Generated ps7_init.c hash mismatch: " + str(generated_file))

impl_dir = WORKSPACE.parent / "adc_easy_test" / "adc_easy_test.runs" / "impl_1"
export_hw_dir.mkdir(parents=True, exist_ok=True)
export_sources = {
    "design_1_wrapper.xsa": XSA,
    "design_1_wrapper.bit": impl_dir / "design_1_wrapper.bit",
    "design_1_wrapper.bin": impl_dir / "design_1_wrapper.bin",
}
for name, source in export_sources.items():
    if not source.is_file():
        raise FileNotFoundError("Platform export source was not found: " + str(source))
    shutil.copy2(source, export_hw_dir / name)

software_domain = (
    platform_dir
    / "export"
    / "adctestp"
    / "sw"
    / "standalone_ps7_cortexa9_0"
)
required_bsp_files = [
    software_domain / "Xilinx.spec",
    software_domain / "cortexa9_toolchain.cmake",
    software_domain / "include" / "xparameters.h",
    software_domain / "lib" / "libxil.a",
]
for required_file in required_bsp_files:
    if not required_file.is_file():
        raise FileNotFoundError("Required standalone BSP file was not found: " + str(required_file))

xparameters = (software_domain / "include" / "xparameters.h").read_text(
    encoding="utf-8", errors="replace"
)
for required_address in ("0x40400000", "0x41200000"):
    if required_address not in xparameters:
        raise RuntimeError("Standalone BSP address map is missing " + required_address)

print("PLATFORM_XSA_SHA256=" + platform_hash)
print("PLATFORM_PS7_INIT_SHA256=" + expected_ps7_init_hash.upper())
print("LTC2208_PLATFORM_REFRESH_COMPLETE")
print("DUAL_UART_PLATFORM_REFRESH_COMPLETE")
