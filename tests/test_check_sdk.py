from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from check_sdk import check_project


def git(path, *args):
    return subprocess.check_output(
        ['git', '-C', str(path), *args], text=True, stderr=subprocess.PIPE).strip()


class CheckSdkTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.upstream = self.root / 'upstream'
        self.upstream.mkdir()
        git(self.upstream, 'init', '-q')
        git(self.upstream, 'config', 'user.name', 'SDK tests')
        git(self.upstream, 'config', 'user.email', 'sdk-tests@example.com')
        git(self.upstream, 'config', 'commit.gpgsign', 'false')
        git(self.upstream, 'commit', '--allow-empty', '-qm', 'Old revision')
        self.stale = git(self.upstream, 'rev-parse', 'HEAD')
        git(self.upstream, 'commit', '--allow-empty', '-qm', 'SDK release')
        git(self.upstream, 'tag', '-a', '-m', '3.4.1 release', 'ncs-v3.4.1')

    def check(self, path):
        project = SimpleNamespace(
            name='mcuboot',
            revision='ncs-v3.4.1',
            url=self.upstream.as_uri(),
            sha=lambda ref: git(path, 'rev-parse', '--verify', ref + '^{commit}'),
        )
        return check_project(project, path)

    def test_rejects_stale_checkout_even_if_manifest_rev_matches_head(self):
        git(self.upstream, 'update-ref', 'refs/heads/manifest-rev', self.stale)
        git(self.upstream, 'checkout', '--detach', self.stale)
        self.assertEqual(self.check(self.upstream), [
            'mcuboot: manifest-rev differs from ncs-v3.4.1',
            'mcuboot: HEAD differs from ncs-v3.4.1',
        ])

    def test_shallow_checkout_uses_remote_annotated_tag_commit(self):
        shallow = self.root / 'shallow'
        shallow.mkdir()
        git(shallow, 'init', '-q')
        url = self.upstream.as_uri()
        git(shallow, 'fetch', '--depth=1', '--no-tags', url, 'ncs-v3.4.1')
        git(shallow, 'checkout', '--detach', 'FETCH_HEAD^{commit}')
        git(shallow, 'update-ref', 'refs/heads/manifest-rev', 'HEAD')
        self.assertEqual(git(shallow, 'tag', '--list'), '')
        self.assertEqual(self.check(shallow), [])

        git(shallow, 'fetch', '--depth=1', '--no-tags', url, self.stale)
        git(shallow, 'checkout', '--detach', 'FETCH_HEAD')
        git(shallow, 'update-ref', 'refs/heads/manifest-rev', 'HEAD')
        self.assertEqual(self.check(shallow), [
            'mcuboot: manifest-rev differs from ncs-v3.4.1',
            'mcuboot: HEAD differs from ncs-v3.4.1',
        ])
