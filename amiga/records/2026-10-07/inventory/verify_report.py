from pathlib import Path
import json,fitz
from playwright.sync_api import sync_playwright
r=Path(__file__).resolve().parent
with sync_playwright() as p:
 browser=p.chromium.launch(executable_path='/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless=True)
 page=browser.new_page(viewport={'width':1440,'height':1100},device_scale_factor=1)
 errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto((r/'A4000TX-System-Report.html').as_uri(),wait_until='load')
 assert page.locator('.loaded-table tbody tr').count()==105
 assert page.locator('.installed-table tbody tr').count()==494
 page.locator('[data-filter="loaded-table"]').fill('68060')
 assert page.locator('.loaded-table tbody tr:visible').count()==1
 page.locator('[data-filter="loaded-table"]').fill('')
 page.locator('[data-filter="installed-table"]').fill('fat95')
 assert page.locator('.installed-table tbody tr:visible').count()==1
 page.locator('[data-filter="installed-table"]').fill('')
 assert page.evaluate('document.documentElement.scrollWidth <= window.innerWidth')
 page.evaluate('window.scrollTo(0,0)')
 page.screenshot(path=str(r/'report-preview.png'),full_page=False)
 page.pdf(path=str(r/'A4000TX-System-Report.pdf'),print_background=True,prefer_css_page_size=True)
 page.set_viewport_size({'width':390,'height':844})
 assert page.evaluate('document.documentElement.scrollWidth <= window.innerWidth')
 assert not errors,errors
 browser.close()
doc=fitz.open(r/'A4000TX-System-Report.pdf')
texts=[p.get_text() for p in doc]
assert all(len(t.strip())>20 for t in texts)
alltext='\n'.join(texts)
for s in ['624 MiB','SDCFXS','StikyRMB','poseidon.library','freewaytritonz3usb.device','lib']:
 assert s in alltext,s
# Check horizontal clipping at page edges and save a rendered first-page sample.
clipped=[]
for i,p in enumerate(doc):
 for b in p.get_text('blocks'):
  if b[0]<0 or b[2]>p.rect.width+1:clipped.append([i+1,b[:4]])
assert not clipped,clipped
for n in [0,2]:
 if n<len(doc):doc[n].get_pixmap(matrix=fitz.Matrix(1.3,1.3)).save(r/f'pdf-page-{n+1}.png')
res={'html_loaded_rows':105,'html_installed_rows':494,'filters':'passed','desktop_and_mobile_overflow':'none','javascript_errors':errors,'pdf_pages':len(doc),'pdf_empty_pages':0,'pdf_horizontal_clipping':clipped,'amiga_final_ping':'alive; Chip free 1889728 bytes; Fast free 595714680 bytes'}
(r/'verification.json').write_text(json.dumps(res,indent=2));print(json.dumps(res,indent=2))
