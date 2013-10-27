#include "blitter.h"
#include "coplist.h"
#include "file.h"
#include "memory.h"
#include "print.h"
#include "3d.h"

#define X(x) ((x) + 0x81)
#define Y(y) ((y) + 0x2c)

static Object3D object;
static View3D view3d;

static BitmapT *screen;
static CopListT *cp;

void Load() {
  screen = NewBitmap(320, 256, 1, FALSE);
  cp = NewCopList(100);

  {
    static VertexT vertex[8] = {
      { -100, -100, -100 },
      {  100, -100, -100 },
      {  100,  100, -100 },
      { -100,  100, -100 },
      { -100, -100,  100 },
      {  100, -100,  100 },
      {  100,  100,  100 },
      { -100,  100,  100 }
    };

    static EdgeT edge[12] = {
      { 0, 1 },
      { 1, 2 },
      { 2, 3 },
      { 3, 0 },
      { 4, 5 },
      { 5, 6 },
      { 6, 7 },
      { 7, 4 },
      { 0, 4 },
      { 1, 5 },
      { 2, 6 },
      { 3, 7 }
    };

    static PointT point[8];
    static UBYTE pointFlags[8];
    static LineT line[12];
    static UBYTE lineFlags[12];

    object.nVertex = 8;
    object.nEdge = 12;
    object.vertex = vertex;
    object.edge = edge;
    object.point = point;
    object.pointFlags = pointFlags;
    object.line = line;
    object.lineFlags = lineFlags;
  }
}

void Kill() {
  DeleteCopList(cp);
  DeleteBitmap(screen);
}

#if 0
static WORD lines[256];
static WORD pixel[8] = { 128, 64, 32, 16, 8, 4, 2, 1 };

static void CalculateLines() {
  WORD i, j;
  WORD l = screen->width / 8;

  for (i = 0, j = 0; i < 256; i++, j += l)
    lines[i] = j;
}

static void ClearPoints() {
  UBYTE *plane = screen->planes[0];
  WORD *coords = (WORD *)point[buffer ^ 1];
  WORD i;

  for (i = 0; i < 8; i++) {
    WORD x = *coords++;
    WORD y = *coords++;

    if (x < 0 || y < 0 || x >= 320 || y >= 256)
     continue;

    plane[lines[y] + (x >> 3)] = 0;
  }
}

static void DrawPoints() {
  UBYTE *plane = screen->planes[0];
  WORD *coords = (WORD *)point[buffer];
  WORD i;

  for (i = 0; i < 8; i++) {
    WORD x = *coords++;
    WORD y = *coords++;

    if (x < 0 || y < 0 || x >= 320 || y >= 256)
     continue;

    plane[lines[y] + (x >> 3)] |= pixel[x & 7];
  }
}
#endif

static void DrawObject() {
  LineT *line = object.line;
  WORD i;

  for (i = 0; i < object.nEdge; i++, line++) {
    WORD x1 = line->x1;
    WORD y1 = line->y1;
    WORD x2 = line->x2;
    WORD y2 = line->y2;

    WaitBlitter();
    BlitterLine(screen, 0, x1, y1, x2, y2);
  }
}

void Main() {
  CopInit(cp);

  CopMove16(cp, bplcon0, BPLCON0_BPU(screen->depth) | BPLCON0_COLOR);
  CopMove16(cp, bplcon1, 0);
  CopMove16(cp, bplcon2, 0);
  CopMove32(cp, bplpt[0], screen->planes[0]);

  CopMove16(cp, ddfstrt, 0x38);
  CopMove16(cp, ddfstop, 0xd0);
  CopMakeDispWin(cp, X(0), Y(0), screen->width, screen->height);
  CopMove16(cp, color[0], 0x000);
  CopMove16(cp, color[1], 0xfff);

  CopEnd(cp);
  CopListActivate(cp);

  custom->dmacon = DMAF_SETCLR | DMAF_BLITTER | DMAF_RASTER | DMAF_MASTER;

  view3d.viewerX = 160;
  view3d.viewerY = 100;
  view3d.viewerZ = 300;

  while (!LeftMouseButton()) {
    view3d.rotateX++;
    view3d.rotateY++;
    view3d.rotateZ++;

    WaitLine(Y(0));

    custom->color[0] = 0x00f;
    CalculateView3D(&view3d);
    TransformVertices(&view3d, &object);
    ClipEdges(&object);
    custom->color[0] = 0x000;

    WaitLine(Y(200));

    custom->color[0] = 0xf00;

    WaitBlitter();
    BlitterClear(screen, 0);

    custom->color[0] = 0x0f0;
    DrawObject();
    custom->color[0] = 0x000;

#if 0
    WaitVBlank();
#endif
  }
}
