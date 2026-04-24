#include <stdlib.h>
#include <math.h>
#if defined(__APPLE__) || defined(MACOSX)
#  define GL_SILENCE_DEPRECATION
#  include <GLUT/glut.h>
#else
#  if defined(WIN32)
#    pragma comment(linker, "/subsystem:¥"windows¥" /entry:¥"mainCRTStartup¥"")
#  endif
#  include <GL/glut.h>
#endif

/* 環境設定 */
#define ZNEAR      0.3                        /* 前方面の位置　　　　　　　　　　　　　 */
#define ZFAR       10.0                       /* 後方面の位置　　　　　　　　　　　　　 */
#define SCREEN_W   1.6                        /* ディスプレイの幅　　　　　　　　　　　 */
#define SCREEN_H   1.2                        /* ディスプレイの高さ　　　　　　　　　　 */
#define DISTANCE   3.2                        /* 視点とディスプレイの距離　　　　　　　 */
#define W (SCREEN_W * 0.5 * ZNEAR / DISTANCE) /* 前方面の幅の２分の１　　　　　　　　　 */
#define H (SCREEN_H * 0.5 * ZNEAR / DISTANCE) /* 前方面の高さの２分の１　　　　　　　　 */
static double parallax = 0.32;                /* 視差　　　　　　　　　　　　　　　　　 */
static double zoffset = 1.0;                  /* ディスプレイ面とオブジェクト中心の距離 */

/* ステレオ表示 */
#define NONE       0                          /* ステレオ表示なし　　　　　　　　　　　 */
#define QUADBUF    1                          /* クアッドバッファステレオを使う　　　　 */
#define BARRIER    2                          /* 視差バリアを使う　　　　　　　　　　　 */
#define STEREO     BARRIER                    /* ステレオ表示の選択　　　　　　　　　　 */
#define GAMEMODE   0                          /* ゲームモードを使うか否か　　　　　　　 */
#define FULLSCREEN 1                          /* フルスクリーンモードにするか否か　　　 */
#define BARRIERBIT 1                          /* 視差バリア用ステンシルバッファのビット */

/* 形状データ */
#define MAXKNOT    100000                     /* 結び目の最大数　　　　　　　　　　　　 */
#define SPAN       0.04                       /* 結び目の間隔　　　　　　　　　　　　　 */
#define WEIGHT     0.002                      /* 糸の重み？　　　　　　　　　　　　　　 */
#define KNOTSIZE   5.0                        /* 結び目の大きさ　　　　　　　　　　　　 */
static struct Knot {
  double p[3];                                /* 結び目の位置　　　　　　　　　　　　　 */
  double v[3];                                /* 法線ベクトル　　　　　　　　　　　　　 */
  double d[4];                                /* 隣接する結び目までの自然長　　　　　　 */
  struct Knot *n[4];                          /* 隣接する結び目　　　　　　　　　　　　 */
  char fix;                                   /* この結び目を固定するか否か　　　　　　 */
} knot[MAXKNOT];
int nknot;                                    /* 結び目の数　　　　　　　　　　　　　　 */

/* ドラッグ用のデータ */
static GLdouble model[16], proj[16];          /* モデルビュー行列と透視変換行列の保存用 */
static GLint view[4];                         /* ビューポート変換行列の保存用　　　　　 */
static struct Knot *hit = 0;                  /* 選択した結び目のポインタ　　　　　　　 */
static GLdouble cz;                           /* 選択した結び目の奥行き　　　　　　　　 */
static int mbutton = -1;                      /* 押したマウスボタン　　　　　　　　　　 */
static int animation = 0;                     /* アニメーション中か否か　　　　　　　　 */
#define FIXBIT 1                              /* 固定する結び目を示すビット　　　　　　 */
#define DRAGBIT 2                             /* ドラッグしている結び目を示すビット　　 */

/* 視点の回転 */
extern void qmul(double [], const double [], const double []);
extern void qrot(double [], double []);
#define SCALE 3.14159265358979323846          /* マウスの相対位置→回転角の換算係数　　 */
static int cx, cy;                            /* ドラッグ開始位置　　　　　　　　　　　 */
static double sx, sy;                         /* マウス移動量に対する相違位置のスケール */
static double cq[4] = { 1.0, 0.0, 0.0, 0.0 }; /* 回転の初期値（クォータニオン）　　　　 */
static double tq[4];                          /* ドラッグ中の回転（クォータニオン）　　 */
static double rt[16];                         /* 回転の変換行列　　　　　　　　　　　　 */

/* 表示モード */
#define KNOTONLY    1
#define WIRE        2
#define WIREKNOT    3
#define SURFACE     4
#define SURFACEKNOT 5
static int style = WIREKNOT;

/*
** 結び目間の自然長の計算
*/
void intervals(struct Knot *k, int n)
{
  /* 隣接する結び目までの距離を計算して自然長とする */
  while (--n >= 0) {
    if (k->n[0]) {
      double dx = k->n[0]->p[0] - k->p[0];
      double dy = k->n[0]->p[1] - k->p[1];
      double dz = k->n[0]->p[2] - k->p[2];
      k->d[0] = sqrt(dx * dx + dy * dy + dz * dz);
    }
    if (k->n[1]) {
      double dx = k->n[1]->p[0] - k->p[0];
      double dy = k->n[1]->p[1] - k->p[1];
      double dz = k->n[1]->p[2] - k->p[2];
      k->d[1] = sqrt(dx * dx + dy * dy + dz * dz);
    }
    if (k->n[2]) {
      double dx = k->n[2]->p[0] - k->p[0];
      double dy = k->n[2]->p[1] - k->p[1];
      double dz = k->n[2]->p[2] - k->p[2];
      k->d[2] = sqrt(dx * dx + dy * dy + dz * dz);
    }
    if (k->n[3]) {
      double dx = k->n[3]->p[0] - k->p[0];
      double dy = k->n[3]->p[1] - k->p[1];
      double dz = k->n[3]->p[2] - k->p[2];
      k->d[3] = sqrt(dx * dx + dy * dy + dz * dz);
    }
    ++k;
  }
}

/*
** 筒の作成
*/
int mkpipe(struct Knot *k, int nx, int nz)
{
  int i, j, n = nx * nz;

  for (j = 0; j < nz; j++) {
    for (i = 0; i < nx; i++) {
      struct Knot *l = k + nx * j + i;

      /* 結び目の初期位置の設定 */
      l->p[0] = (double)nx * SPAN * 0.1591549430918953
        * cos(6.283185307179586 * (double)i / (double)nx);
      l->p[1] = (double)j * SPAN - 0.5 * (SPAN * (double)(nz - 1));
      l->p[2] = (double)nx * SPAN * 0.1591549430918953
        * sin(6.283185307179586 * (double)i / (double)nx);

      /* 隣接する結び目 */
      l->n[0] = (i > 0     ) ? l - 1 : l + nx - 1;
      l->n[1] = (i < nx - 1) ? l + 1 : l - nx + 1;
      l->n[2] = (j > 0     ) ? l - nx : 0;
      l->n[3] = (j < nz - 1) ? l + nx : 0;

      /* 固定する結び目 */
      l->fix = 0;
    }
  }

  intervals(k, n);

  /* 上端の結び目を固定 */
  for (i = 0; i < nx; i++) k[n - nx + i].fix |= FIXBIT;

  return n;
}

/*
** 網の作成
*/
int mknet(struct Knot *k, int nx, int nz)
{
  int i, j, n = nx * nz;
  
  for (j = 0; j < nz; j++) {
    for (i = 0; i < nx; i++) {
      struct Knot *l = k + nx * j + i;
      
      /* 結び目の初期位置の設定 */
      l->p[0] = (double)i * SPAN - 0.5 * (SPAN * (double)(nx - 1));
      l->p[1] = 0.5;
      l->p[2] = (double)j * SPAN - 0.5 * (SPAN * (double)(nz - 1));
      
      /* 隣接する結び目 */
      l->n[0] = (i > 0     ) ? l - 1 : 0;
      l->n[1] = (i < nx - 1) ? l + 1 : 0;
      l->n[2] = (j > 0     ) ? l - nx : 0;
      l->n[3] = (j < nx - 1) ? l + nx : 0;
      
      /* 固定する結び目 */
      l->fix = 0;
    }
  }

  intervals(k, n);

  /* 四隅の結び目を固定する */
  k[0     ].fix |= FIXBIT;
  k[nx - 1].fix |= FIXBIT;
  k[n - nx].fix |= FIXBIT;
  k[n - 1 ].fix |= FIXBIT;

  return n;
}

/*
** 横糸の張力
*/
void forcex(struct Knot *k)
{
  if (k->fix == 0) {
    double f[3] = { 0.0, 0.0, 0.0 };
    
    if (k->n[0]) {
      double x0 = k->n[0]->p[0] - k->p[0];
      double y0 = k->n[0]->p[1] - k->p[1];
      double z0 = k->n[0]->p[2] - k->p[2];
      double r0 = sqrt(x0 * x0 + y0 * y0 + z0 * z0);
      double s0 = 1.0 - k->d[0] / r0;
      
      s0 *= s0 * s0;
      
      f[0] += x0 * s0;
      f[1] += y0 * s0;
      f[2] += z0 * s0;
    }
    
    if (k->n[1]) {
      double x1 = k->n[1]->p[0] - k->p[0];
      double y1 = k->n[1]->p[1] - k->p[1];
      double z1 = k->n[1]->p[2] - k->p[2];
      double r1 = sqrt(x1 * x1 + y1 * y1 + z1 * z1);
      double s1 = 1.0 - k->d[1] / r1;
      
      s1 *= s1 * s1;
      
      f[0] += x1 * s1;
      f[1] += y1 * s1;
      f[2] += z1 * s1;
    }
    
    k->p[0] += f[0];
    k->p[1] += f[1];
    k->p[2] += f[2];
  }
}

/*
** 縦糸の張力
*/
void forcez(struct Knot *k)
{
  if (k->fix == 0) {
    double f[3] = { 0.0, 0.0, 0.0 };
    
    if (k->n[2]) {
      double x2 = k->n[2]->p[0] - k->p[0];
      double y2 = k->n[2]->p[1] - k->p[1];
      double z2 = k->n[2]->p[2] - k->p[2];
      double r2 = sqrt(x2 * x2 + y2 * y2 + z2 * z2);
      double s2 = 1.0 - k->d[2] / r2;
      
      s2 *= s2 * s2;
      
      f[0] += x2 * s2;
      f[1] += y2 * s2;
      f[2] += z2 * s2;
    }
    
    if (k->n[3]) {
      double x3 = k->n[3]->p[0] - k->p[0];
      double y3 = k->n[3]->p[1] - k->p[1];
      double z3 = k->n[3]->p[2] - k->p[2];
      double r3 = sqrt(x3 * x3 + y3 * y3 + z3 * z3);
      double s3 = 1.0 - k->d[3] / r3;
      
      s3 *= s3 * s3;
      
      f[0] += x3 * s3;
      f[1] += y3 * s3;
      f[2] += z3 * s3;
    }
    
    k->p[0] += f[0];
    k->p[1] += f[1];
    k->p[2] += f[2];
  }
}

/*
** 重力
*/
void forcey(struct Knot *k)
{
    if (k->fix == 0) k->p[1] -= WEIGHT;
}

/*
** 結び目の位置の算出
*/
void optimize(int n)
{
  while (--n >= 0) {
    struct Knot *k;
    int i;

    k = knot;
    do {
      struct Knot *k1, *k2;

      forcex(k);
      for (k1 = k; (k2 = k1->n[1]) != 0; k1 = k2) {
        forcex(k2);
        if (k2 == k) break;
      }
    }
    while ((k = k->n[3]) && k != knot);

    k = knot;
    do {
      struct Knot *k1, *k2;

      forcez(k);
      for (k1 = k; (k2 = k1->n[3]) != 0; k1 = k2) {
        forcez(k2);
        if (k2 == k) break;
      }
    }
    while ((k = k->n[1]) && k != knot);

    for (i = 0; i < nknot; i++) {
      forcey(knot + i);
    }
  }
}

/*
** 結び目のみ表示
*/
void knotonly(struct Knot *k, int n)
{
  glBegin(GL_POINTS);
  while (--n >= 0) {
    if (k->fix & FIXBIT)
      glColor3d(0.0, 0.0, 1.0);
    else if (k->fix & DRAGBIT)
      glColor3d(0.0, 1.0, 0.0);
    else
      glColor3d(1.0, 0.0, 0.0);
    glVertex3dv(k++->p);
  }
  glEnd();
}

/*
** ワイヤーフレーム表示
*/
void wireframe(struct Knot *k, int n)
{
  struct Knot *k0;

  glColor3d(0.0, 0.0, 0.0);

  k0 = k;
  do {
    struct Knot *k1, *k2;

    glBegin(GL_LINE_STRIP);
    glVertex3dv(k0->p);
    for (k1 = k0; (k2 = k1->n[1]) != 0; k1 = k2) {
      glVertex3dv(k2->p);
      if (k2 == k0) break;
    }
    glEnd();
  }
  while ((k0 = k0->n[3]) && k0 != k);

  k0 = k;
  do {
    struct Knot *k1, *k2;

    glBegin(GL_LINE_STRIP);
    glVertex3dv(k0->p);
    for (k1 = k0; (k2 = k1->n[3]) != 0; k1 = k2) {
      glVertex3dv(k2->p);
      if (k2 == k0) break;
    }
    glEnd();
  }
  while ((k0 = k0->n[1]) && k0 != k);

  if (style & 1) knotonly(k, n);
}

/*
** 隠面消去表示
*/
void surface(struct Knot *k, int n)
{
  static float color[] = { 0.8, 0.8, 0.8, 1.0 };
  struct Knot *k0, *k3;
  int i;

  /* 結び目の法線ベクトル */
  for (i = 0; i < n; i++) {
    double d[4][3], v[3], a;

    k0 = k + i;

    if (k0->n[0]) {
      d[0][0] = k0->n[0]->p[0] - k0->p[0];
      d[0][1] = k0->n[0]->p[1] - k0->p[1];
      d[0][2] = k0->n[0]->p[2] - k0->p[2];
    }
    if (k0->n[1]) {
      d[1][0] = k0->n[1]->p[0] - k0->p[0];
      d[1][1] = k0->n[1]->p[1] - k0->p[1];
      d[1][2] = k0->n[1]->p[2] - k0->p[2];
    }
    if (k0->n[2]) {
      d[2][0] = k0->n[2]->p[0] - k0->p[0];
      d[2][1] = k0->n[2]->p[1] - k0->p[1];
      d[2][2] = k0->n[2]->p[2] - k0->p[2];
    }
    if (k0->n[3]) {
      d[3][0] = k0->n[3]->p[0] - k0->p[0];
      d[3][1] = k0->n[3]->p[1] - k0->p[1];
      d[3][2] = k0->n[3]->p[2] - k0->p[2];
    }

    v[0] = v[1] = v[2] = 0.0;

    if (k0->n[0]) {
      if (k0->n[2]) {
        v[0] += d[2][1] * d[0][2] - d[2][2] * d[0][1];
        v[1] += d[2][2] * d[0][0] - d[2][0] * d[0][2];
        v[2] += d[2][0] * d[0][1] - d[2][1] * d[0][0];
      }
      if (k0->n[3]) {
        v[0] += d[0][1] * d[3][2] - d[0][2] * d[3][1];
        v[1] += d[0][2] * d[3][0] - d[0][0] * d[3][2];
        v[2] += d[0][0] * d[3][1] - d[0][1] * d[3][0];
      }
    }
    if (k0->n[1]) {
      if (k0->n[2]) {
        v[0] += d[1][1] * d[2][2] - d[1][2] * d[2][1];
        v[1] += d[1][2] * d[2][0] - d[1][0] * d[2][2];
        v[2] += d[1][0] * d[2][1] - d[1][1] * d[2][0];
      }
      if (k0->n[3]) {
        v[0] += d[3][1] * d[1][2] - d[3][2] * d[1][1];
        v[1] += d[3][2] * d[1][0] - d[3][0] * d[1][2];
        v[2] += d[3][0] * d[1][1] - d[3][1] * d[1][0];
      }
    }

    a = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (a != 0.0) {
      k0->v[0] = v[0] / a;
      k0->v[1] = v[1] / a;
      k0->v[2] = v[2] / a;
    }
  }

  /* 描画 */

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LIGHTING);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, color);

  for (k0 = k; (k3 = k0->n[3]) != 0; k0 = k3) {
    struct Knot *k1, *k2;

    glBegin(GL_QUAD_STRIP);
    glNormal3dv(k0->v);
    glVertex3dv(k0->p);
    glNormal3dv(k3->v);
    glVertex3dv(k3->p);
    for (k1 = k0; (k2 = k1->n[1]) != 0; k1 = k2) {
      glNormal3dv(k2->v);
      glVertex3dv(k2->p);
      glNormal3dv(k2->n[3]->v);
      glVertex3dv(k2->n[3]->p);
      if (k2 == k0) break;
    }
    glEnd();
    if (k3 == k) break;
  }

  glDisable(GL_LIGHTING);

  if (style & 1) knotonly(k, n);

  glDisable(GL_DEPTH_TEST);
}

/*
** 画面表示
*/
void display(void)
{
  static float lightpos[] = { 0.0, 0.0, 1.0, 0.0 };
  GLdouble f = parallax * 0.5 * ZNEAR / DISTANCE;
  GLuint obj;

  if (animation) optimize(5);
  
  /* 右目の画像 */
#if STEREO == QUADBUF
  glDrawBuffer(GL_BACK_RIGHT);
#elif STEREO == BARRIER
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
#endif
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  
#if STEREO != NONE
  glFrustum(-W - f, W - f, -H, H, ZNEAR, ZFAR);
#else
  glFrustum(-W, W, -H, H, ZNEAR, ZFAR);
#endif
  
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
#if STEREO != NONE
  glTranslated(-parallax * 0.5, 0.0, -(DISTANCE + zoffset));
#else
  glTranslated(0.0, 0.0, -(DISTANCE + zoffset));
#endif
  glLightfv(GL_LIGHT0, GL_POSITION, lightpos);
  glMultMatrixd(rt);
  
#if STEREO == BARRIER
  /* 奇数ラインにＲ・Ｂを表示 */
  glStencilFunc(GL_NOTEQUAL, BARRIERBIT, BARRIERBIT);
  glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
  obj = glGenLists(1);
  glNewList(obj, GL_COMPILE_AND_EXECUTE);
#endif

  if (style & 2)
    wireframe(knot, nknot);
  else if (style & 4)
    surface(knot, nknot);
  else if (style & 1)
    knotonly(knot, nknot);

#if STEREO != NONE

#  if STEREO == BARRIER
  glEndList();
  /* 偶数ラインにＧを表示 */
  glStencilFunc(GL_EQUAL, BARRIERBIT, BARRIERBIT);
  glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
  /* シーンの描画 */
  glCallList(obj);
#  endif
  
  /* 左目の画像 */
#  if STEREO == QUADBUF
  glDrawBuffer(GL_BACK_LEFT);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#  endif
  
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-W + f, W + f, -H, H, ZNEAR, ZFAR);
  
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslated(parallax * 0.5, 0.0, -(DISTANCE + zoffset));
  glLightfv(GL_LIGHT0, GL_POSITION, lightpos);
  glMultMatrixd(rt);
  
#  if STEREO == BARRIER
  /* 偶数ラインにＲ・Ｂを表示 */
  glStencilFunc(GL_EQUAL, BARRIERBIT, BARRIERBIT);
  glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
  glClear(GL_DEPTH_BUFFER_BIT);
#  endif
  /* シーンの描画 */
  glCallList(obj);
#  if STEREO == BARRIER
  /* 奇数ラインにＧを表示 */
  glStencilFunc(GL_NOTEQUAL, BARRIERBIT, BARRIERBIT);
  glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
  /* シーンの描画 */
  glCallList(obj);
#  endif

  glDeleteLists(1, 1);
#endif

  glutSwapBuffers();
}

void resize(int w, int h)
{
#if STEREO == BARRIER
  int x;
#endif

  /* マウスポインタ位置のウィンドウ内の相対的位置への換算用 */
  sx = 1.0 / (double)w;
  sy = 1.0 / (double)h;

  /* ウィンドウ全体をビューポートにする */
  glViewport(0, 0, w, h);
  
#if STEREO == BARRIER
  /* ステンシルバッファにマスクを描く */
  glClearStencil(0);
  glStencilFunc(GL_ALWAYS, BARRIERBIT, BARRIERBIT);
  glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
  glDisable(GL_DEPTH_TEST);
  glDrawBuffer(GL_NONE);
  glClear(GL_STENCIL_BUFFER_BIT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-0.5, (GLdouble)w, -0.5, (GLdouble)h, -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glBegin(GL_LINES);
  for (x = 0; x < w; x += 2) {
    glVertex2d(x, 0);
    glVertex2d(x, h - 1);
  }
  glEnd();
  glFlush();
  glDrawBuffer(GL_BACK);
  glEnable(GL_DEPTH_TEST);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
#endif
}

void idle(void)
{
  glutPostRedisplay();
}

struct Knot *pick(struct Knot *k, int n, int x, int y, GLdouble *z)
{
  GLdouble ux, uy, zmin;
  struct Knot *kmin;
  
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-W, W, -H, H, ZNEAR, ZFAR);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslated(0.0, 0.0, -(DISTANCE + zoffset));
  glMultMatrixd(rt);
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, view);
  
  ux = x;
  uy = view[3] - y;

  zmin = 1000.0;
  kmin = 0;

  for (; --n >= 0; k++) {
    GLdouble wx, wy, wz;
    
    gluProject(k->p[0], k->p[1], k->p[2],
       model, proj, view, &wx, &wy, &wz);
    if (fabs(ux - wx) <= KNOTSIZE && fabs(uy - wy) <= KNOTSIZE && wz < zmin) {
      /* もっとも手前にある結び目を探す */
      zmin = wz;
      kmin = k;
    }
  }

  *z = zmin;
  return kmin;
}

void mouse(int button, int state, int x, int y)
{
  switch (mbutton = button) {
  case GLUT_LEFT_BUTTON:
    if (state == GLUT_DOWN) {
      /* カーソル位置の結び目を探す */
      hit = pick(knot, nknot, x, y, &cz);
      if (hit) {
        /* 見つけた結び目にドラッグ用の印をつける */
        hit->fix |= DRAGBIT;
        if (!animation) glutIdleFunc(idle);
      }
    }
    else {
      if (hit) {
        /* ドラッグしていた結び目から印を消す */
        hit->fix &= ~DRAGBIT;
        hit = 0;
      }
      if (!animation) glutIdleFunc(0);
    }
    break;
  case GLUT_MIDDLE_BUTTON:
    if (state == GLUT_DOWN) {
      /* カーソル位置の結び目を探す */
      hit = pick(knot, nknot, x, y, &cz);
      if (hit) {
        /* その結び目を固定する印を反転する */
        hit->fix ^= FIXBIT;
        if (!animation) glutPostRedisplay();
      }
    }
    else {
      hit = 0;
    }
    break;
  case GLUT_RIGHT_BUTTON:
    if (state == GLUT_DOWN) {
      /* ドラッグ開始点を記録 */
      cx = x;
      cy = y;
      /* アニメーション開始 */
      if (!animation) glutIdleFunc(idle);
    }
    else {
      /* アニメーション終了 */
      if (!animation) glutIdleFunc(0);
      /* 回転の保存 */
      cq[0] = tq[0];
      cq[1] = tq[1];
      cq[2] = tq[2];
      cq[3] = tq[3];
    }
    break;
  default:
    break;
  }
}

void motion(int x, int y)
{
  if (mbutton == GLUT_RIGHT_BUTTON) {
    
    /* マウスポインタの位置のドラッグ開始位置からの変位 */
    double dx = (x - cx) * sx;
    double dy = (y - cy) * sy;
    
    /* マウスポインタの位置のドラッグ開始位置からの距離 */
    double a = sqrt(dx * dx + dy * dy);
    
    if (a != 0.0) {
      double ar = a * SCALE * 0.5;
      double as = sin(ar) / a;
      double dq[4] = { cos(ar), dy * as, dx * as, 0.0 };

      /* クォータニオンを掛けて回転を合成 */
      qmul(tq, dq, cq);
      /* クォータニオンから回転の変換行列を求める */
      qrot(rt, tq);
    }
  }
  else if (mbutton == GLUT_LEFT_BUTTON) {
    if (hit) {
      gluUnProject((GLdouble)x, (GLdouble)(view[3] - y), cz,
        model, proj, view, hit->p, hit->p + 1, hit->p + 2);
    }
  }
}

void keyboard(unsigned char key, int x, int y)
{
  switch (key) {
  case ' ':
    if ((animation = 1 - animation) != 0)
      glutIdleFunc(idle);
    else
      glutIdleFunc(0);
    break;
  case 'p':
  case 'P':
    animation = 0;
    glutIdleFunc(0);
    nknot = mkpipe(knot, 40, 10);
    glutPostRedisplay();
    break;
  case 'm':
  case 'M':
    animation = 0;
    glutIdleFunc(0);
    nknot = mknet(knot, 20, 20);
    glutPostRedisplay();
    break;
  case 'o':
  case 'O':
    parallax += SCREEN_W / 30.0;
    glutPostRedisplay();
    break;
  case 'c':
  case 'C':
    parallax -= SCREEN_W / 30.0;
    glutPostRedisplay();
    break;
  case 'n':
  case 'N':
    zoffset -= 0.1;
    glutPostRedisplay();
    break;
  case 'f':
  case 'F':
    zoffset += 0.1;
    glutPostRedisplay();
    break;
  case 'v':
  case 'V':
    if (++style > SURFACEKNOT) style = KNOTONLY;
    glutPostRedisplay();
    break;
  case '\033':
  case 'q':
  case 'Q':
    exit(0);
  default:
    break;
  }
}

void init(void)
{
  /* OpenGL の初期設定 */
  glClearColor(1.0, 1.0, 1.0, 0.0);
  glPointSize(KNOTSIZE);
  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);
  glEnable(GL_LIGHT0);
#if STEREO == BARRIER
  glEnable(GL_STENCIL_TEST);
#endif

  /* 形状データの作成 */
  nknot = mknet(knot, 20, 20);

  /* 回転行列の初期化 */
  qrot(rt, cq);
}

int main(int argc, char *argv[])
{
  glutInitWindowSize(500, 500);
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH
#if STEREO == QUADBUF
    | GLUT_STEREO
#elif STEREO == BARRIER
    | GLUT_STENCIL
#endif
    );
#if GAMEMODE
  glutGameModeString("width=1024 height=768 bpp‾24 hertz=100");
  glutEnterGameMode();
#else
  glutCreateWindow(argv[0]);
#  if FULLSCREEN
  glutFullScreen();
#  endif
#endif
  glutDisplayFunc(display);
  glutReshapeFunc(resize);
  glutMouseFunc(mouse);
  glutMotionFunc(motion);
  glutKeyboardFunc(keyboard);
  init();
  glutMainLoop();
  
  return 0;
}
