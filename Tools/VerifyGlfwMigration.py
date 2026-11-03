"""Read-only GLFW packaging/build-graph checks. Never invokes a compiler or CMake.

Display tests are an independent mathematical model; use the game's --verify
after a user build for real C++ / OpenGL integration checks.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import re
import struct
import subprocess
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "SimpleGame"
DEPS = GAME / "Dependencies"
GLFW = DEPS / "GLFW"
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}


def read(path):
    return path.read_text(encoding="utf-8-sig")


def items(tree, kind):
    return [row.attrib["Include"] for row in tree.findall(f".//m:{kind}", NS)
            if "Include" in row.attrib]


def local(base, relative):
    return (base / relative.replace("\\", "/")).resolve()


def canvas_view(width, height):
    if width <= 0 or height <= 0:
        return 0, 0, 1440, 900
    scale = min(width/1440, height/900)
    w, h = width/scale, height/scale
    return (1440-w)/2, (900-h)/2, w, h


def to_canvas(x, y, width, height, fb_width, fb_height):
    if min(width, height, fb_width, fb_height) <= 0:
        return -1, -1
    vx, vy, vw, vh = canvas_view(fb_width, fb_height)
    return x*vw/width+vx, y*vh/height+vy


class MigrationChecks(unittest.TestCase):
    def test_pinned_upstream_bytes(self):
        manifest = json.loads(read(DEPS / "GLFW.UPSTREAM.json"))
        self.assertEqual(manifest["version"], "3.5.1")
        self.assertEqual(manifest["commit"], "d9d6f0f1f967807ffade6598ea9a631ebaf37a56")
        files = manifest["files"]
        actual = {p.relative_to(GLFW).as_posix() for p in GLFW.rglob("*")
                  if p.is_file() and p.relative_to(GLFW).parts[0] != "Build"}
        self.assertEqual(set(files), actual)
        self.assertEqual(len(files), manifest["fileCount"])
        for name, expected in files.items():
            self.assertEqual(hashlib.sha256((GLFW / name).read_bytes()).hexdigest().upper(),
                             expected.upper(), name)

    def test_runtime_architectures_and_hashes(self):
        manifest = json.loads(read(DEPS / "GLEW/runtime-manifest.json"))
        for arch, row in manifest["architectures"].items():
            runtime = local(DEPS / "GLEW", row["runtime"]["path"])
            data = runtime.read_bytes()
            self.assertEqual(data[:2], b"MZ")
            pe = struct.unpack_from("<I", data, 0x3C)[0]
            self.assertEqual(data[pe:pe+4], b"PE\0\0")
            self.assertEqual(struct.unpack_from("<H", data, pe+4)[0],
                             0x14C if arch == "Win32" else 0x8664)
            for kind in ("runtime", "importLibrary"):
                record = row[kind]
                path = local(DEPS / "GLEW", record["path"])
                self.assertEqual(path.stat().st_size, record["size"])
                self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest().upper(),
                                 record["sha256"].upper(), str(path))
            ignored = subprocess.run(["git", "check-ignore", "--no-index", "-q", str(runtime)],
                                     cwd=ROOT, capture_output=True)
            self.assertEqual(ignored.returncode, 1, f"Runtime would not be committed: {runtime}")

    def test_native_source_and_dependency_graph(self):
        game_project = ET.parse(GAME / "SimpleGame.vcxproj")
        glfw_project = ET.parse(GLFW / "Build/GLFW.vcxproj")
        for tree, base in ((game_project, GAME), (glfw_project, GLFW / "Build")):
            for name in items(tree, "ClCompile") + items(tree, "ClInclude"):
                self.assertTrue(local(base, name).is_file(), name)
            self.assertEqual(set(items(tree, "ProjectConfiguration")),
                             {"Debug|Win32", "Release|Win32", "Debug|x64", "Release|x64"})
        sources = items(game_project, "ClCompile")
        self.assertNotIn("SimpleGame.cpp", sources)
        references = items(game_project, "ProjectReference")
        self.assertIn("Dependencies\\GLFW\\Build\\GLFW.vcxproj", references)
        project_text = read(GAME / "SimpleGame.vcxproj")
        self.assertNotIn("freeglut", project_text.lower())
        self.assertNotIn("RuntimeDlls", project_text)
        self.assertNotIn("$(SolutionDir)", project_text)
        self.assertIn("$(DefaultPlatformToolset)", project_text)
        self.assertIn("GetFileHash", project_text)
        self.assertIn("$(GlewRuntime)", project_text)
        glfw_text = read(GLFW / "Build/GLFW.vcxproj")
        self.assertIn("<ConfigurationType>StaticLibrary</ConfigurationType>", glfw_text)
        self.assertIn("<PrecompiledHeader>NotUsing</PrecompiledHeader>", glfw_text)
        self.assertNotIn("GLFW_INCLUDE_NONE", glfw_text)
        self.assertNotIn("GLFW_DLL", glfw_text)
        self.assertIn("MultiThreadedDebugDLL", glfw_text)
        self.assertIn("MultiThreadedDLL", glfw_text)
        expected = {"context.c", "init.c", "input.c", "monitor.c", "platform.c", "vulkan.c",
                    "window.c", "egl_context.c", "osmesa_context.c", "null_init.c",
                    "null_monitor.c", "null_window.c", "null_joystick.c", "win32_module.c",
                    "win32_time.c", "win32_thread.c", "win32_init.c", "win32_joystick.c",
                    "win32_monitor.c", "win32_window.c", "wgl_context.c"}
        self.assertEqual({local(GLFW / "Build", p).name for p in items(glfw_project, "ClCompile")},
                         expected)
        for name in sources:
            self.assertIsNone(re.search(r"\b(?:glut[A-Z]\w*|GLUT_\w*|freeglut)\b",
                                        read(local(GAME, name))), name)
        solution = read(ROOT / "SimpleGame.sln")
        guid = glfw_project.findtext("m:PropertyGroup/m:ProjectGuid", namespaces=NS)
        for config, platform in (("Debug", "x64"), ("Debug", "x86"),
                                 ("Release", "x64"), ("Release", "x86")):
            self.assertIn(f"{guid}.{config}|{platform}.Build.0", solution)

    def test_cmake_is_offline_and_sources_match(self):
        text = read(ROOT / "CMakeLists.txt")
        block = re.search(r"set\(game_sources\s+(.*?)\n\)", text, re.S)[1]
        cmake_sources = set(re.findall(r"\b[\w_]+\.cpp\b", block))
        native_sources = set(items(ET.parse(GAME / "SimpleGame.vcxproj"), "ClCompile"))
        self.assertEqual(cmake_sources, native_sources)
        self.assertIn("add_subdirectory", text)
        self.assertNotIn("FetchContent", text)
        self.assertNotIn("ExternalProject", text)
        for option in ("GLFW_BUILD_EXAMPLES", "GLFW_BUILD_TESTS", "GLFW_BUILD_DOCS", "GLFW_INSTALL"):
            self.assertRegex(text, rf"set\({option} OFF\b")
        self.assertIn("GLFW_LIBRARY_TYPE STATIC", text)
        self.assertIn("MultiThreaded$<$<CONFIG:Debug>:Debug>DLL", text)
        presets = json.loads(read(ROOT / "CMakePresets.json"))
        self.assertEqual({p["name"] for p in presets["buildPresets"]},
                         {"windows-x64-debug", "windows-x64-release",
                          "windows-x86-debug", "windows-x86-release"})
        for name in ("LICENSE.md", "include/GLFW/glfw3.h", "src/win32_window.c"):
            self.assertTrue((GLFW / name).is_file())

    def test_loop_input_and_lifetime_contract(self):
        text = read(GAME / "Main.cpp")
        run = text[text.index("    void Run()") : text.index("} // namespace")]
        self.assertLess(run.index("glfwPollEvents()"), run.index("game->Tick(dt)"))
        self.assertLess(run.index("game->Tick(dt)"), run.index("game->Render()"))
        self.assertLess(run.index("game->Render()"), run.index("glfwSwapBuffers(window)"))
        self.assertEqual(run.count("glfwSwapBuffers(window)"), 1)
        self.assertNotIn("sleep", run.lower())
        self.assertIn("glfwWaitEventsTimeout(.1)", run)
        self.assertIn("monitor->Capture(game->world)", run[run.index("if (!focused"):])
        close = text[text.index("    void Close()") : text.index("    void ClearInput()")]
        self.assertLess(close.index("game.reset()"), close.index("renderer.reset()"))
        self.assertLess(close.index("renderer.reset()"), close.index("glfwDestroyWindow(window)"))
        self.assertLess(close.index("glfwDestroyWindow(window)"), close.index("glfwTerminate()"))
        self.assertIn("action == GLFW_REPEAT", text)
        self.assertIn("key == GLFW_KEY_F11", text)
        self.assertIn("mods & GLFW_MOD_ALT", text)
        self.assertIn("GLFW_OPENGL_COMPAT_PROFILE", text)
        self.assertIn("glfwWindowHint(GLFW_DEPTH_BITS, 0)", text)
        self.assertIn("glfwWindowHint(GLFW_STENCIL_BITS, 0)", text)
        self.assertNotIn("SetProcessDPIAware", text)
        self.assertIn("glfwWindowHint(GLFW_SCALE_TO_MONITOR, verify ? GLFW_FALSE : GLFW_TRUE)", text)
        self.assertLess(text.index("window = glfwCreateWindow"),
                        text.index("glfwSetWindowSize(window, width, height)"))
        self.assertLess(text.index("glfwSetWindowSize(window, width, height)"),
                        text.index("glfwMakeContextCurrent(window);", text.index("window = glfwCreateWindow")))
        self.assertIn("std::chrono::steady_clock", read(GAME / "RenderStats.cpp"))
        self.assertNotIn("GLFW", read(GAME / "Input.h"))
        self.assertIn("GLFW_INCLUDE_NONE", read(GAME / "GlfwInput.h"))
        for name in ("RecordTiming", "RestartWindow", "averageCpuMs", "averageSwapMs"):
            self.assertIn(name, read(GAME / "RenderStats.h"))

    def test_viewport_math(self):
        for width, height in ((1280, 800), (1920, 1080), (2560, 1440), (3840, 2160),
                              (3440, 1440), (5120, 1440), (1001, 741), (800, 1200), (1, 1)):
            for sx, sy in ((1, 1), (2, 2), (1.25, 1.25), (1.5, 1.5), (2, 1.5)):
                fw, fh = round(width*sx), round(height*sy)
                vx, vy, vw, vh = canvas_view(fw, fh)
                self.assertLessEqual(vx, 0)
                self.assertLessEqual(vy, 0)
                self.assertGreaterEqual(vw, 1440)
                self.assertGreaterEqual(vh, 900)
                self.assertAlmostEqual(vw/vh, fw/fh)
                self.assertAlmostEqual(fw/vw, fh/vh)  # circles remain circular
                for x, y in ((0, 0), (720, 450), (1440, 900), (1300, 110)):
                    actual = to_canvas((x-vx)*width/vw, (y-vy)*height/vh,
                                       width, height, fw, fh)
                    self.assertTrue(math.isclose(actual[0], x, abs_tol=1e-8))
                    self.assertTrue(math.isclose(actual[1], y, abs_tol=1e-8))
                self.assertEqual(to_canvas(0, 0, width, height, fw, fh), (vx, vy))
                scale = min(1, 2560/fw, 1440/fh)
                sw, sh = max(1, round(fw*scale)), max(1, round(fh*scale))
                self.assertLessEqual(sw, min(fw, 2560))
                self.assertLessEqual(sh, min(fh, 1440))
                self.assertLessEqual(abs(sw-sh*fw/fh), .5+fw/fh*.5+1e-8)
        self.assertEqual(to_canvas(1, 1, 0, 0, 0, 0), (-1, -1))
        self.assertAlmostEqual(to_canvas(0, 540, 1920, 1080, 3840, 2160)[0], -80)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--require-tracked", action="store_true")
    args = parser.parse_args()
    if args.require_tracked:
        required = list(GLFW.rglob("*")) + list((DEPS / "GLEW").rglob("*"))
        for path in required:
            if path.is_file():
                result = subprocess.run(["git", "ls-files", "--error-unmatch", "--",
                                         path.relative_to(ROOT).as_posix()],
                                        cwd=ROOT, capture_output=True)
                assert result.returncode == 0, f"Not staged/committed: {path}"
    unittest.main(argv=[__file__], verbosity=2)
