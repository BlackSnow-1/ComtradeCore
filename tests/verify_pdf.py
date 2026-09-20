"""Validate the C++-generated PDF with an independent PDF implementation."""
import pathlib
import subprocess
import sys

pdf = pathlib.Path(sys.argv[1]).resolve()
text = subprocess.check_output(['pdftotext', '-enc', 'UTF-8', str(pdf), '-']).decode('utf-8')
for expected in ['COMTRADE', '辅助分析报告', '中文录波分析', '本地证据附录', 'cfgSha256']:
    assert expected in text, f'Missing PDF text: {expected}'
assert text.count('\f') >= 3, 'Expected multiple pages'
assert '\ufffd' not in text, 'Replacement glyph found'
assert 'test-secret' not in text, 'Credential leaked'
subprocess.run(['pdftoppm', '-f', '1', '-singlefile', '-scale-to', '1400', '-png', str(pdf), str(pdf.with_suffix(''))], check=True)
print(f'PDF verified: {text.count(chr(12))} pages, Chinese text extraction and rendering passed')
