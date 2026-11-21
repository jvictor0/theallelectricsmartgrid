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
        for name in ("Makefile", "JUCE/SmartGridOne/Makefile", "scripts/clean_builds.py",
                     "scripts/build_cache.py"):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(REPO_ROOT / name, target)
        self.log = self.caller / "commands.jsonl"
        self.juce_header = self.root / "third_party/JUCE/modules/juce_core/juce_core.h"
        self.upstream = Path(self.temp.name) / "JUCE upstream"
        self.git(self.upstream, "init", "--quiet")
        self.source_header = self.upstream / "modules/juce_core/juce_core.h"
        self.source_header.parent.mkdir(parents=True)
        self.revision = self.commit_juce("first revision")
        self.git(self.root, "init", "--quiet")
        self.gitmodules = self.root / ".gitmodules"
        self.gitmodules.write_text(f'[submodule "JUCE8"]\n\tpath = third_party/JUCE\n\turl = {self.upstream}\n')
        self.git(self.root, "add", ".")
        self.pin_juce(self.revision)
        self.git(self.root, "commit", "-qm", "SmartGrid fixture")
        self.cache = Path(self.temp.name) / "shared cache"
        self.tool = self.caller / "record-command"
        self.tool.write_text(
            f"#!{sys.executable}\n"
            "import json, os, pathlib, sys\n"
            "with open(os.environ['COMMAND_LOG'], 'a') as log:\n"
            "    log.write(json.dumps(sys.argv[1:]) + '\\n')\n"
            "if '--build' in sys.argv and 'CCACHE_ENV_LOG' in os.environ:\n"
            "    path = pathlib.Path(os.environ['CCACHE_ENV_LOG'])\n"
            "    path.write_text(json.dumps([os.environ['CCACHE_DIR'], os.environ['CCACHE_BASEDIR']]))\n"
            "for arg in sys.argv:\n"
            "    if arg.startswith(('SMARTGRID_JUCE_MODULES=', '-DSMARTGRID_JUCE_ROOT=')):\n"
            "        path = pathlib.Path(arg.split('=', 1)[1])\n"
            "        modules = path if path.name == 'modules' else path / 'modules'\n"
            "        assert (modules / 'juce_core/juce_core.h').is_file()\n"
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
        for name in ("cmake", "ctest"):
            (self.caller / name).symlink_to(self.tool)
        self.env = dict(os.environ, COMMAND_LOG=str(self.log),
                        SMARTGRID_BUILD_CACHE_DIR=str(self.cache),
                        PATH=str(self.caller) + os.pathsep + os.environ["PATH"],
                        RUN_MARKER=str(self.caller / "ran"))

    def git(self, root, *args):
        root.mkdir(parents=True, exist_ok=True)
        return subprocess.check_output(
            ["git", "-C", str(root), "-c", "user.name=Build Tests", "-c", "user.email=build@example.invalid",
             "-c", "commit.gpgsign=false", *args], text=True,
        ).strip()

    def commit_juce(self, content):
        self.source_header.write_text(content)
        self.git(self.upstream, "add", ".")
        self.git(self.upstream, "commit", "-qm", content)
        return self.git(self.upstream, "rev-parse", "HEAD")

    def pin_juce(self, revision):
        self.git(self.root, "update-index", "--add", "--cacheinfo", f"160000,{revision},third_party/JUCE")

    def make(self, *args, app=False, success=True, root=None):
        makefile = (root or self.root) / ("JUCE/SmartGridOne/Makefile" if app else "Makefile")
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

    def test_parallel_builds_and_fresh_worktree_reuse_pinned_juce(self):
        self.make("-j4", "build", "visualizers-test", "midi-test")
        commands = self.commands()
        self.assertEqual(len(commands), 7)
        source = self.cache / "juce" / self.revision
        self.assertEqual((source / "modules/juce_core/juce_core.h").read_text(), "first revision")
        self.assertFalse(self.juce_header.exists())
        self.assertIn(f"-DSMARTGRID_JUCE_ROOT={source}", [arg for args in commands for arg in args])
        other = self.root.with_name("other-worktree")
        self.git(self.root, "worktree", "add", "--quiet", "--detach", str(other))
        self.upstream.rename(self.upstream.with_name("offline"))
        self.make("build", app=True, root=other)
        self.assertIn(f"SMARTGRID_JUCE_MODULES={source}/modules", self.commands()[-1])
        self.assertFalse((other / "third_party/JUCE/modules").exists())

    def test_pin_changes_and_local_checkout_takes_precedence(self):
        first = Path(self.make("-s", "juce-deps").strip())
        next_revision = self.commit_juce("next revision")
        self.pin_juce(next_revision)
        second = Path(self.make("-s", "juce-deps").strip())
        self.assertEqual((first / "modules/juce_core/juce_core.h").read_text(), "first revision")
        self.assertEqual((second / "modules/juce_core/juce_core.h").read_text(), "next revision")
        self.juce_header.parent.mkdir(parents=True)
        self.juce_header.write_text("local edits")
        self.make("build", app=True)
        self.assertIn(f"SMARTGRID_JUCE_MODULES={self.root}/third_party/JUCE/modules", self.commands()[-1])

    def test_failed_juce_setup_prevents_build_and_deploy(self):
        self.upstream.rename(self.upstream.with_name("offline"))
        for app, target in ((False, "build"), (True, "build-sanitized"),
                            (False, "ios-deploy"), (True, "deploy-ios"),
                            (False, "midi-test"), (False, "visualizers-test"),
                            (True, "clang-tidy")):
            with self.subTest(app=app, target=target):
                output = self.make("-j4", target, app=app, success=False)
                self.assertIn("Build cache:", output)
                self.assertFalse(self.log.exists())
                self.assertFalse(self.juce_header.exists())
                self.assertFalse((self.cache / "juce" / self.revision).exists())

    def test_core_tests_do_not_initialize_juce(self):
        self.make("core-test")
        self.assertEqual(len(self.commands()), 3)
        self.assertFalse(self.cache.exists())
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

    def test_ccache_launcher_uses_shared_store_and_worktree_base(self):
        env_log = self.caller / "ccache-env.json"
        self.env["CCACHE_ENV_LOG"] = str(env_log)
        self.env.pop("CCACHE_DIR", None)
        self.env.pop("CCACHE_BASEDIR", None)
        self.make("core-build", "CCACHE=/example/ccache")
        self.assertIn("-DCMAKE_CXX_COMPILER_LAUNCHER=/example/ccache", self.commands()[0])
        self.assertEqual(json.loads(env_log.read_text()), [str(self.cache / "ccache"), str(self.root)])

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
