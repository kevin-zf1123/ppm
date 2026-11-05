"""Stage, deploy, smoke-test, and archive the CI application (not the build tree)."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile


def run(*args):
    subprocess.run([str(arg) for arg in args], check=True)


def smoke_test(executable, qt_dir, temporary):
    """Check native startup with no project and no Qt installation on PATH."""
    env = os.environ.copy()
    for key in list(env):
        if key.startswith(("QT_", "QML", "DYLD_")):
            del env[key]
    env["PATH"] = os.pathsep.join(
        entry for entry in env.get("PATH", "").split(os.pathsep)
        if not Path(entry).is_relative_to(qt_dir)
    )
    home = temporary / "home"
    empty_stuff = temporary / "empty-stuff"
    home.mkdir()
    empty_stuff.mkdir()
    env["HOME"] = str(home)
    env["CFFIXED_USER_HOME"] = str(home)
    if sys.platform == "darwin":
        config_dir = home / "Library/Application Support/PPM"
        config_dir.mkdir(parents=True)
    else:
        config_dir = executable.parent
    (config_dir / "conf.ini").write_text(
        "[General]\nStuffFolderPath=" + empty_stuff.as_posix() + "\n",
        encoding="utf-8",
    )
    with tempfile.TemporaryFile() as output:
        process = subprocess.Popen(
            [str(executable)], cwd=temporary, env=env,
            stdout=output, stderr=subprocess.STDOUT,
        )
        try:
            try:
                code = process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                print("Native application stayed running for five seconds.", flush=True)
            else:
                raise RuntimeError(f"Application exited during startup: {code}")
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            output.seek(0)
            print(output.read().decode("utf-8", errors="replace"), flush=True)


def main():
    repository = Path(__file__).resolve().parents[2]
    build = repository / "build"
    qt_dir = Path(os.environ["QT_ROOT_DIR"]).resolve()
    if sys.platform not in ("darwin", "win32"):
        raise RuntimeError("CI application packages support Windows and macOS only")
    platform_name = "macos-arm64" if sys.platform == "darwin" else "windows-x64"
    with tempfile.TemporaryDirectory(prefix="ppm-artifact-") as temp:
        temporary = Path(temp)
        package = temporary / "PPM"
        package.mkdir()
        shutil.copytree(repository / "stuff", package / "stuff")
        shutil.copytree(repository / "docs", package / "docs")
        shutil.copy2(repository / "LICENSE", package / "LICENSE")
        shutil.copy2(repository / "package/README-ci.txt", package / "README.txt")
        if (qt_dir / "licenses").is_dir():
            shutil.copytree(qt_dir / "licenses", package / "stuff/LICENSE/Qt")

        if sys.platform == "darwin":
            bundle = package / "ppm.app"
            shutil.copytree(build / "ppm.app", bundle, symlinks=True)
            # The source build points to the runner's Qt plugins. Deployment
            # must instead use the plugins inside this fresh staging bundle.
            config = bundle / "Contents/Resources/qt.conf"
            config.unlink()
            run(qt_dir / "bin/macdeployqt", bundle, "-always-overwrite", "-no-codesign")
            config.write_text("[Paths]\nPlugins=PlugIns\n", encoding="utf-8")
            run("cmake", f"-DBUNDLE={bundle}", "-P",
                repository / ".github/scripts/verify-macos-bundle.cmake")
            run("codesign", "--force", "--deep", "--sign", "-", bundle)
            run("codesign", "--verify", "--deep", "--strict", bundle)
            executable = bundle / "Contents/MacOS/ppm"
        else:
            executable = package / "ppm.exe"
            shutil.copy2(build / "Release/ppm.exe", executable)
            run(qt_dir / "bin/windeployqt.exe", "--release", "--compiler-runtime",
                executable)

        smoke_test(executable, qt_dir, temporary)
        if sys.platform == "win32":
            (package / "conf.ini").write_text(
                "[General]\nStuffFolderPath=stuff\n", encoding="utf-8")
        (package / "BUILD.txt").write_text(
            f"Commit: {os.environ['GITHUB_SHA']}\nPlatform: {platform_name}\nQt: 6.12.0\n",
            encoding="utf-8",
        )
        output = build / "artifacts"
        output.mkdir(exist_ok=True)
        if sys.platform == "darwin":
            # Preserve executable permissions and framework symlinks through
            # GitHub's outer artifact ZIP, which does not retain permissions.
            archive = output / f"ppm-{platform_name}.tar.gz"
            with tarfile.open(archive, "w:gz") as handle:
                handle.add(package, arcname="PPM")
        else:
            archive = output / f"ppm-{platform_name}.zip"
            with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as handle:
                for path in sorted(package.rglob("*")):
                    if path.is_file():
                        handle.write(path, path.relative_to(temporary))
        print(f"Packaged {archive}", flush=True)


if __name__ == "__main__":
    main()
