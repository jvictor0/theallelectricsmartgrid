"""Exercise build provenance using isolated Git repositories and real builds."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest


x_repoRoot = Path(__file__).resolve().parents[1]
x_generator = x_repoRoot / "scripts/generate_build_info.py"


class BuildInfoTests(unittest.TestCase):
    def setUp(self):
        self.m_temp = tempfile.TemporaryDirectory(prefix="smartgrid-build-info-")
        self.addCleanup(self.m_temp.cleanup)
        self.m_root = Path(self.m_temp.name)
        self.m_repo = self.m_root / "checkout with spaces"
        self.m_repo.mkdir()
        self.m_output = self.m_root / "build with spaces/generated/SmartGridBuildInfo.hpp"
        self.Run("git", "init", "--quiet", str(self.m_repo))
        self.Git("config", "user.name", "Build Info Test")
        self.Git("config", "user.email", "build-info@example.invalid")
        self.Git("-c", "commit.gpgsign=false", "commit", "--allow-empty", "--quiet", "-m", "First")

    def Run(self, *command, **kwargs):
        result = subprocess.run(command, capture_output=True, text=True, **kwargs)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout.strip()

    def Git(self, *arguments):
        return self.Run("git", "-C", str(self.m_repo), *arguments)

    def Generate(self):
        self.Run(sys.executable, str(x_generator), str(self.m_repo), str(self.m_output))

    def test_generates_full_commit_sha_in_build_directory(self):
        self.Generate()
        sha = self.Git("rev-parse", "HEAD")
        self.assertEqual(len(sha), 40)
        self.assertIn(f'inline constexpr char x_gitCommitSha[] = "{sha}";', self.m_output.read_text())
        self.assertEqual(list(self.m_repo.iterdir()), [self.m_repo / ".git"])

    def test_unchanged_head_does_not_rewrite_even_with_dirty_files(self):
        self.Generate()
        os.utime(self.m_output, ns=(1000000000, 1000000000))
        original = self.m_output.read_bytes()
        (self.m_repo / "untracked.txt").write_text("uncommitted")
        self.Generate()
        self.assertEqual(self.m_output.read_bytes(), original)
        self.assertEqual(self.m_output.stat().st_mtime_ns, 1000000000)

    def test_cmake_rebuild_embeds_changed_head_without_cleaning(self):
        test_dir = self.m_repo / "private/test"
        (test_dir / "support").mkdir(parents=True)
        (test_dir / "unit").mkdir()
        (self.m_repo / "private/src").mkdir()
        (self.m_repo / "scripts").mkdir()
        shutil.copyfile(x_repoRoot / "private/test/CMakeLists.txt", test_dir / "CMakeLists.txt")
        shutil.copyfile(x_generator, self.m_repo / "scripts/generate_build_info.py")
        (self.m_repo / "private/src/SmartGrid.cpp").write_text("")
        (test_dir / "unit/infra_spike.cpp").write_text("")
        (test_dir / "support/TestMain.cpp").write_text(
            '#include "SmartGridBuildInfo.hpp"\n'
            '#include <cstdio>\n\n'
            'int main()\n'
            '{\n'
            '    std::puts(SmartGridBuildInfo::x_gitCommitSha);\n'
            '}\n\n'
        )
        build_dir = self.m_root / "cmake build"
        self.Run("cmake", "-S", str(test_dir), "-B", str(build_dir))
        self.Run("cmake", "--build", str(build_dir), "--target", "smartgrid_tests", "--parallel", "2")
        executable = build_dir / "smartgrid_tests"
        first_sha = self.Git("rev-parse", "HEAD")
        self.assertEqual(self.Run(str(executable)), first_sha)
        original_mtime = executable.stat().st_mtime_ns
        self.Run("cmake", "--build", str(build_dir), "--target", "smartgrid_tests", "--parallel", "2")
        self.assertEqual(executable.stat().st_mtime_ns, original_mtime)
        # macOS Make 3.81 compares dependency timestamps at whole-second precision.
        #
        time.sleep(1.1)
        self.Git("-c", "commit.gpgsign=false", "commit", "--allow-empty", "--quiet", "-m", "Second")
        second_sha = self.Git("rev-parse", "HEAD")
        self.assertNotEqual(first_sha, second_sha)
        self.Run("cmake", "--build", str(build_dir), "--target", "smartgrid_tests", "--parallel", "2")
        shutil.rmtree(self.m_repo / ".git")
        self.assertEqual(self.Run(str(executable)), second_sha)


if __name__ == "__main__":
    unittest.main()
