# PlatformIOのextra_script: git describe をファームに埋め込む
import subprocess

Import("env")  # noqa: F821

try:
    ver = subprocess.check_output(
        ["git", "describe", "--tags", "--always", "--dirty"], text=True, stderr=subprocess.DEVNULL
    ).strip()
except Exception:
    ver = "dev"

env.Append(CPPDEFINES=[("LABTERM_VERSION", env.StringifyMacro(ver))])  # noqa: F821
