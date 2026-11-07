#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A small PDF for pdfgate's tests: a few pages of text in the standard
fonts, a heading and a few lines drawn, made here (no file of anyone's).

    python3 tests/pdf/small-pdf.py OUT.pdf [PAGES]
"""
import sys

WORDS = ("The Gateway converts this page for the Atari, the same bytes as "
         "EConvert on a Falcon. ").split()


def page_text(n):
    lines = ["BT /F2 20 Tf 72 740 Td (Page %d of the small test) Tj ET" % n]
    y = 700
    for i in range(30):
        words = " ".join(WORDS[(i + j) % len(WORDS)] for j in range(11))
        lines.append("BT /F1 11 Tf 72 %d Td (%s) Tj ET" % (y, words))
        y -= 18
    lines += ["0.2 0.4 0.8 RG 2 w 72 120 m 540 120 l S",
              "0.9 0.3 0.2 rg 72 60 120 40 re f"]
    return "\n".join(lines).encode()


def make(pages):
    objs = [b"<< /Type /Catalog /Pages 2 0 R >>", None,
            b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
            b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>"]
    kids = []
    for n in range(1, pages + 1):
        body = page_text(n)
        objs.append(b"<< /Length %d >>\nstream\n" % len(body) + body + b"\nendstream")
        objs.append(b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents %d 0 R "
                    b"/Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> >>" % (len(objs)))
        kids.append(len(objs))
    objs[1] = b"<< /Type /Pages /Kids [%s] /Count %d >>" % (b" ".join(b"%d 0 R" % k for k in kids), pages)
    out, offs = bytearray(b"%PDF-1.4\n"), []
    for i, o in enumerate(objs, 1):
        offs.append(len(out))
        out += b"%d 0 obj\n" % i + o + b"\nendobj\n"
    xref = len(out)
    out += b"xref\n0 %d\n0000000000 65535 f \n" % (len(objs) + 1)
    out += b"".join(b"%010d 00000 n \n" % o for o in offs)
    out += b"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (len(objs) + 1, xref)
    return bytes(out)


if __name__ == "__main__":
    with open(sys.argv[1], "wb") as f:
        f.write(make(int(sys.argv[2]) if len(sys.argv) > 2 else 3))
