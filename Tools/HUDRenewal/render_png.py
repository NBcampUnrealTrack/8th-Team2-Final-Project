# -*- coding: utf-8 -*-
"""Exports every mock canvas of hud_proposal.html at 1920x1080 (png/<id>.png) and a page overview (png/_page.png)."""
import pathlib
import sys
from playwright.sync_api import sync_playwright

DOC = pathlib.Path(__file__).resolve().parents[2] / 'Docs' / 'HUD_Renewal_Proposal_20261014'
ONLY = sys.argv[1:]

with sync_playwright() as p:
    browser = p.chromium.launch()
    page = browser.new_page(viewport={'width': 1400, 'height': 900})
    errors = []
    page.on('pageerror', lambda e: errors.append(str(e)))
    page.goto((DOC / 'hud_proposal.html').as_uri())
    page.wait_for_timeout(600)
    out = DOC / 'png'
    out.mkdir(exist_ok=True)
    if not ONLY:
        page.screenshot(path=str(out / '_page.png'), full_page=True)
    page.add_style_tag(content='.wrap{max-width:none!important;padding:0!important} .stage{width:1920px!important;border-radius:0!important}')
    page.set_viewport_size({'width': 1960, 'height': 1200})
    page.wait_for_timeout(400)
    for stage in page.query_selector_all('.stage'):
        sid = stage.get_attribute('id')
        if ONLY and sid not in ONLY:
            continue
        stage.scroll_into_view_if_needed()
        page.wait_for_timeout(150)
        stage.screenshot(path=str(out / (sid + '.png')))
        print('png', sid)
    print('js errors', errors)
    browser.close()
