// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 AERA | Erez Yaary
//
// webgate's extension, its background: asks webgate on this Pi (the port
// in WEBGATE below) which address to open next, opens it in the browser's
// tab as you would, and sends back the snapshot the page's content.js
// makes once the page has settled. No automation protocol: the browser is
// an ordinary one, with its own profile.

const WEBGATE = "http://127.0.0.1:8281";
let wanted = null;          // {id, url} being opened

// the browser's view as a JPEG, width pixels wide (live view), and the
// view's own width (the screen's, in kiosk mode)
async function shot(width, quality) {
  const url = await chrome.tabs.captureVisibleTab(null, {format: "png"});
  const bmp = await createImageBitmap(await (await fetch(url)).blob());
  const w = Math.min(width, bmp.width), h = Math.round(bmp.height * w / bmp.width);
  const c = new OffscreenCanvas(w, h);
  c.getContext("2d").drawImage(bmp, 0, 0, w, h);
  const jpeg = await c.convertToBlob({type: "image/jpeg", quality: quality});
  return [new Uint8Array(await jpeg.arrayBuffer()), bmp.width];
}

async function next() {
  for (;;) {
    try {
      const r = await fetch(WEBGATE + "/next", {cache: "no-store"});
      if (r.status === 200) {
        const w = await r.json();
        const [tab] = await chrome.tabs.query({active: true, lastFocusedWindow: true});
        if (w.shot) {
          // live view: a picture of what the browser shows now
          let pic = new Uint8Array(0), full = 0, why = "";
          try {
            [pic, full] = await shot(w.shot, w.quality || 0.7);
          } catch (e) {
            why = "&why=" + encodeURIComponent(String(e).slice(0, 200));
          }
          await fetch(WEBGATE + "/shot?id=" + encodeURIComponent(w.id) + "&w=" + full + why, {method: "POST", body: pic});
        } else if (w.snap) {
          // the page shown now, as a snapshot (live view's "as a page")
          wanted = w;
          if (tab)
            await chrome.tabs.sendMessage(tab.id, {snapshot_now: true}).catch(() => {});
        } else {
          wanted = w;
          if (tab)
            await chrome.tabs.update(tab.id, {url: w.url});
          else
            await chrome.tabs.create({url: w.url});
        }
      }
    } catch (e) {
      await new Promise(ok => setTimeout(ok, 2000));    // webgate not there yet
    }
  }
}

chrome.runtime.onMessage.addListener((msg, sender) => {
  if (msg.snapshot && wanted) {
    const id = wanted.id;
    wanted = null;
    fetch(WEBGATE + "/snapshot?id=" + encodeURIComponent(id), {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: JSON.stringify({url: msg.url, title: msg.title, html: msg.html}),
    }).catch(() => {});
  }
});

next();
