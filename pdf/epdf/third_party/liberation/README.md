# Liberation fonts in EConvert

[Liberation](https://github.com/liberationfonts/liberation-fonts) 2.1.5, the faces as Debian's `fonts-liberation`
1:2.1.5-3 carries them, unchanged. They are used under the **SIL Open Font Licence 1.1** (`OFL.txt`). They are
metric-compatible with Helvetica, Times and Courier.

EConvert draws a PDF's standard fonts with them when a page's fonts aren't embedded (src/conv/render.c). The
release carries them as separate files in `fonts\` beside ECONVERT.PRG, with this licence. They aren't
linked into the program and aren't sold on their own (the OFL's terms, the hub's clearance of 3 Nov 2026).

They are not modified: "Liberation" is a Reserved Font Name, so a changed face (a subset, for one) would need
another name. Only the files' names are EConvert's own, 8.3 names a TOS file system takes:

| File | Face |
|---|---|
| `fonts/sans.ttf` | LiberationSans-Regular |
| `fonts/sansb.ttf` | LiberationSans-Bold |
| `fonts/serif.ttf` | LiberationSerif-Regular |
| `fonts/serifb.ttf` | LiberationSerif-Bold |
| `fonts/mono.ttf` | LiberationMono-Regular |
| `fonts/monob.ttf` | LiberationMono-Bold |

The italic faces are left out until a page needs them. A face that isn't there is drawn with its upright
one (`sansi.ttf` becomes `sans.ttf`).
