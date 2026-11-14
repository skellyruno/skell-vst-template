"""Pure-Python Quick4 workspace safety tests; no UI captures or network access."""
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import quick4_projects
import quick4_workspace as workspace


class WorkspaceTests(unittest.TestCase):
    def test_preset_is_wired_to_real_four_scene_flag(self):
        cfg = quick4_projects.PROJECTS["skell-preset-vst"]
        self.assertEqual(cfg["target"], "SkellPresetUIRenderer")
        self.assertEqual(cfg["quick_args"], ["--quick4"])

    def test_offline_requires_existing_pinned_juce(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaisesRegex(RuntimeError, "Offline: no JUCE"):
                workspace.ensure_juce(Path(temp) / "JUCE-8.0.12", offline=True)

    def test_existing_juce_cache_is_not_recloned(self):
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / "JUCE-8.0.12"
            target.mkdir()
            (target / "CMakeLists.txt").write_text("project(JUCE)\n")
            with mock.patch.object(workspace, "run") as runner:
                workspace.ensure_juce(target, offline=True)
                runner.assert_not_called()

    def test_existing_plugin_cache_is_not_refetched_by_default(self):
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / "skell-preset-vst"
            (target / ".git").mkdir(parents=True)
            (target / "CMakeLists.txt").write_text("project(SkellPreset)\n")
            with mock.patch.object(workspace, "run") as runner:
                workspace.ensure_plugin("skell-preset-vst", target,
                                        ref=None, update=False, offline=True)
                runner.assert_not_called()

    def test_ref_switch_rejects_dirty_worktree(self):
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / "skell-preset-vst"
            (target / ".git").mkdir(parents=True)
            (target / "CMakeLists.txt").write_text("project(SkellPreset)\n")
            with mock.patch.object(workspace, "output", return_value=" M Source/PluginEditor.cpp"):
                with mock.patch.object(workspace, "run") as runner:
                    with self.assertRaisesRegex(RuntimeError, "uncommitted changes"):
                        workspace.ensure_plugin("skell-preset-vst", target,
                                                ref="feat/ui", update=True,
                                                offline=False)
                    runner.assert_not_called()

    def test_update_requires_explicit_ref(self):
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / "skell-preset-vst"
            (target / ".git").mkdir(parents=True)
            (target / "CMakeLists.txt").write_text("project(SkellPreset)\n")
            with self.assertRaisesRegex(RuntimeError, "explicit --ref"):
                workspace.ensure_plugin("skell-preset-vst", target, ref=None,
                                        update=True, offline=False)

    def test_no_overwrite_of_existing_non_git_source(self):
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / "skell-preset-vst"
            target.mkdir()
            (target / "README.md").write_text("user project\n")
            with self.assertRaisesRegex(RuntimeError, "Refusing to overwrite"):
                workspace.ensure_plugin("skell-preset-vst", target, ref=None,
                                        update=False, offline=False)

    def test_full_mode_is_separate(self):
        self.assertNotIn("--full", quick4_projects.PROJECTS["skell-preset-vst"]["quick_args"])


if __name__ == "__main__":
    unittest.main()
