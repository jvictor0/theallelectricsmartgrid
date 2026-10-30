"""Exercise Make's command boundaries without compiling apps or contacting devices."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


REPO_ROOT = Path(__file__).resolve().parents[2]


class BuildWorkflowTests(unittest.TestCase):
    def setUp(self):
        scratch = REPO_ROOT / "build/results/build-workflow-tests"
        scratch.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "checkout"
        self.caller = Path(self.temp.name) / "caller"
        self.caller.mkdir()
        for name in ("Makefile", "JUCE/SmartGridOne/Makefile", "scripts/clean_builds.py"):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(REPO_ROOT / name, target)
        self.log = self.caller / "commands.jsonl"
        self.juce_header = self.root / "third_party/JUCE/modules/juce_core/juce_core.h"
        self.tool = self.caller / "record-command"
        self.tool.write_text(
            f"#!{sys.executable}\n"
            "import json, os, pathlib, sys\n"
            "with open(os.environ['COMMAND_LOG'], 'a') as log:\n"
            "    log.write(json.dumps(sys.argv[1:]) + '\\n')\n"
            "header = pathlib.Path(os.environ['JUCE_HEADER'])\n"
            "if 'submodule' in sys.argv:\n"
            "    if os.environ.get('FAIL_JUCE_SETUP'):\n"
            "        sys.exit(8)\n"
            "    checkout = pathlib.Path(sys.argv[sys.argv.index('-C') + 1])\n"
            "    assert header.is_relative_to(checkout)\n"
            "    header.parent.mkdir(parents=True, exist_ok=True)\n"
            "    header.write_text('initialized')\n"
            "needs_juce = '-project' in sys.argv or any(\n"
            "    part in arg for arg in sys.argv\n"
            "    for part in ('juce-midi', 'juce-visualizers', 'build/midi/', 'build/visualizers/'))\n"
            "if needs_juce and not header.is_file():\n"
            "    sys.exit('JUCE must be initialized before building')\n"
            "if '-project' in sys.argv:\n"
            "    if os.environ.get('FAIL_BUILD'):\n"
            "        sys.exit(7)\n"
            "    product = next(a.split('=', 1)[1] for a in sys.argv\n"
            "                   if a.startswith('CONFIGURATION_BUILD_DIR='))\n"
            "    exe = pathlib.Path(product) / 'SmartGridOne.app/Contents/MacOS/SmartGridOne'\n"
            "    exe.parent.mkdir(parents=True, exist_ok=True)\n"
            "    exe.write_text('#!/bin/sh\\nprintf ran > \"$RUN_MARKER\"\\n')\n"
            "    exe.chmod(0o755)\n"
        )
        self.tool.chmod(0o755)
        for name in ("git", "cmake", "ctest"):
            (self.caller / name).symlink_to(self.tool)
        self.env = dict(os.environ, COMMAND_LOG=str(self.log),
                        JUCE_HEADER=str(self.juce_header),
                        PATH=str(self.caller) + os.pathsep + os.environ["PATH"],
                        RUN_MARKER=str(self.caller / "ran"))

    def make(self, *args, app=False, success=True):
        makefile = self.root / ("JUCE/SmartGridOne/Makefile" if app else "Makefile")
        result = subprocess.run(
            ["make", "-f", str(makefile), f"XCODEBUILD={self.tool}",
             f"IOS_PYTHON={self.tool}", *args],
            cwd=self.caller, env=self.env, text=True, capture_output=True,
        )
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout + result.stderr

    def commands(self):
        return [json.loads(line) for line in self.log.read_text().splitlines()]

    def test_missing_juce_is_initialized_once_before_parallel_builds(self):
        self.make("-j4", "build", "visualizers-test", "midi-test")
        commands = self.commands()
        self.assertEqual(commands[0], ["-C", str(self.root), "submodule", "update",
                                       "--init", "--recursive", "--", "third_party/JUCE"])
        self.assertEqual(sum("submodule" in args for args in commands), 1)
        self.assertEqual(len(commands), 8)
        self.assertTrue(self.juce_header.is_file())
        before = len(commands)
        self.make("build", app=True)
        self.assertEqual(len(self.commands()), before + 1)
        self.assertIn("-project", self.commands()[-1])

    def test_failed_juce_setup_prevents_build_and_deploy(self):
        self.env["FAIL_JUCE_SETUP"] = "1"
        for app, target in ((False, "build"), (True, "build-sanitized"),
                            (False, "ios-deploy"), (True, "deploy-ios"),
                            (False, "midi-test"), (False, "visualizers-test"),
                            (True, "clang-tidy")):
            with self.subTest(app=app, target=target):
                self.make("-j4", target, app=app, success=False)
                commands = self.commands()
                self.assertEqual(len(commands), 1)
                self.assertIn("submodule", commands[0])
                self.assertFalse(self.juce_header.exists())
                self.log.unlink()

    def test_core_tests_do_not_initialize_juce(self):
        self.make("core-test")
        self.assertEqual(len(self.commands()), 3)
        self.assertFalse(any("submodule" in args for args in self.commands()))
        self.assertFalse(self.juce_header.exists())

    def test_native_run_and_sanitizer_use_the_built_product_from_any_cwd(self):
        for app, target, config in ((False, "run", "Release"),
                                    (True, "run", "Debug"),
                                    (False, "run-sanitized", "Debug")):
            with self.subTest(app=app, target=target):
                self.make(target, f"CONFIG={config}", app=app)
                args = self.commands()[-1]
                base = self.root / "build/xcode/macos" / config
                self.assertEqual(args[args.index("-configuration") + 1], config)
                self.assertEqual(args[args.index("-derivedDataPath") + 1], str(base / "DerivedData"))
                for key, child in (("OBJROOT", "Intermediates"), ("SYMROOT", "Products"),
                                   ("CONFIGURATION_BUILD_DIR", "Products")):
                    self.assertIn(f"{key}={base / child}", args)
                self.assertEqual((self.caller / "ran").read_text(), "ran")
                (self.caller / "ran").unlink()

    def test_ios_deploy_install_and_alias_agree_with_build(self):
        for app, target in ((False, "ios-deploy"), (True, "deploy-ios")):
            self.make("-j4", target, "CONFIG=Debug", app=app)
            build, deploy = self.commands()[-2:]
            product = self.root / "build/xcode/ios/Debug/Products"
            self.assertIn(f"CONFIGURATION_BUILD_DIR={product}", build)
            self.assertIn("generic/platform=iOS", build)
            self.assertEqual(deploy[deploy.index("--app") + 1], str(product / "SmartGridOne.app"))
            self.assertNotIn("--install-only", deploy)
        self.make("ios-install", "CONFIG=Debug")
        self.assertIn("--install-only", self.commands()[-1])

    def test_missing_install_and_failed_build_do_not_deploy(self):
        output = self.make("ios-install", success=False)
        self.assertIn("run make ios-build first", output)
        self.assertFalse(self.log.exists())
        self.make("ios-build")
        self.env["FAIL_BUILD"] = "1"
        before = len(self.commands())
        self.make("ios-deploy", success=False)
        self.assertEqual(len(self.commands()), before + 1)
        self.assertIn("-project", self.commands()[-1])

    def test_cmake_targets_share_directories_and_preserve_defaults(self):
        for family, config in (("core", "RelWithDebInfo"),
                               ("visualizers", "Release"), ("midi", "Release")):
            output = self.make("-n", f"{family}-test")
            directory = str(self.root / "build" / family / config)
            self.assertEqual(output.count(directory), 3)
            self.assertIn(f'-DCMAKE_BUILD_TYPE="{config}"', output)
            output = self.make("-n", f"{family}-test", f"{family.upper()}_CONFIG=Debug")
            self.assertEqual(output.count(str(self.root / "build" / family / "Debug")), 3)

    def test_clean_is_scoped_and_idempotent(self):
        disposable = ("core", "visualizers", "midi", "xcode/macos", "xcode/ios", "clang-tidy", "tools")
        for name in disposable:
            directory = self.root / "build" / name / "nested"
            directory.mkdir(parents=True)
            (directory / "output").write_text("disposable")
        keep = ("build/results/experiment", "build/custom/output", "private/src/source",
                "third_party/JUCE/dependency", "scripts/.venv-ios/python",
                "JUCE/SmartGridOne/Builds/iOS/build/legacy")
        for name in keep:
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text("keep")
        external = self.caller / "external"
        external.mkdir()
        (external / "keep").write_text("keep")
        (self.root / "build/core/link").symlink_to(external, target_is_directory=True)
        self.make("clean", "BUILD_DIR=/", "IOS_BUILD_DIR=/")
        self.make("clean", app=True)
        for name in disposable:
            self.assertFalse((self.root / "build" / name).exists())
        for name in keep:
            self.assertEqual((self.root / name).read_text(), "keep")
        self.assertEqual((external / "keep").read_text(), "keep")

    def test_clean_refuses_symlinked_ancestors_before_deleting_anything(self):
        (self.root / "build/core").mkdir(parents=True)
        marker = self.root / "build/core/keep"
        marker.write_text("keep")
        (self.root / "build/xcode").symlink_to(self.caller, target_is_directory=True)
        output = self.make("clean", success=False)
        self.assertIn("Refusing to clean through symlink", output)
        self.assertEqual(marker.read_text(), "keep")


if __name__ == "__main__":
    unittest.main()
