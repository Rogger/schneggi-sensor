import json
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest


class FlashAdapterMakeTests(unittest.TestCase):
    def test_rule_violations_stop_parallel_exports(self):
        makefile = Path(__file__).parents[1] / 'hardware/flash_adapter/Makefile'
        for failed_check in ('drc', 'erc'):
            with self.subTest(check=failed_check), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                shutil.copyfile(makefile, root / 'Makefile')
                checker = root / 'kicad.py'
                # KiCad normally returns success for rule violations unless
                # explicitly told to turn them into an exit status.
                checker.write_text(
                    'import json, sys\n'
                    'from pathlib import Path\n'
                    'with Path("calls.jsonl").open("a") as calls:\n'
                    '    calls.write(json.dumps(sys.argv[1:]) + "\\n")\n'
                    f'violations = sys.argv[2] == {failed_check!r}\n'
                    'sys.exit(5 if violations and "--exit-code-violations" in sys.argv else 0)\n'
                )
                result = subprocess.run(
                    ['make', '-j4', 'jlcpcb',
                     f'KICAD_CLI={shlex.quote(sys.executable)} {shlex.quote(str(checker))}'],
                    cwd=root, capture_output=True, text=True,
                )
                self.assertNotEqual(result.returncode, 0)
                calls = [json.loads(line) for line in (root / 'calls.jsonl').read_text().splitlines()]
                self.assertTrue(calls)
                self.assertFalse(any('export' in call for call in calls), calls)
                self.assertEqual(calls[-1][1], failed_check)
                self.assertFalse((root / 'jlcpcb').exists())


if __name__ == '__main__':
    unittest.main()
