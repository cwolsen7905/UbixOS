/**********************************************************************
will add copyright bs later

$Id: main.cpp 89 2016-01-12 00:20:40Z reddawg $
**********************************************************************/

#include "ogPixCon.h"
#include "objgfx40.h"
#include "ogFont.h"
#include "ogSprite.h"
#include "ogBlit.h"
#ifdef __DJGPP__
#include "ogDisplay_VESA.h"
#endif
#include <math.h>
#include <iostream>
#include <stdio.h>
#include <time.h>

using namespace std;

void
testPixCon(void) {
  ogSurface * buf1 = new ogSurface();
  ogSurface * buf2 = new ogSurface();
  ogPixCon * pixCon = new ogPixCon(OG_PIXFMT_32BPP, OG_PIXFMT_16BPP);
  uInt8 r, g, b;

  buf1->ogCreate(100, 100, OG_PIXFMT_32BPP);
  buf2->ogCreate(100, 200, OG_PIXFMT_16BPP);
  
  buf1->ogSetPixel(0, 0, buf1->ogPack(128, 42, 69));
  buf2->ogSetPixel(0, 0, pixCon->ConvPix(buf1->ogGetPixel(0, 0)));

  buf2->ogUnpack(buf2->ogGetPixel(0, 0), r, g, b);
//  cout << "r: " << (uInt32)r << endl;
//  cout << "g: " << (uInt32)g << endl;
//  cout << "b: " << (uInt32)b << endl;
  return;
}; // textPixCon

void testSetPixel(ogSurface& buf) {
  uInt32 xx,yy;
  buf.ogClear(buf.ogPack(0,255,0));
  for (yy = 0; yy<=buf.ogGetMaxY(); yy++)
    for (xx = 0; xx<=buf.ogGetMaxX(); xx++)
      buf.ogSetPixel(xx,yy,xx*yy);
  getc(stdin);
  return;
} // testSetPixel

void testRect(ogSurface& buf) {
  uInt32 count;
  if (buf.ogGetBPP()==8)
    for (count=0; count<1000; count++)
      buf.ogRect(buf.ogGetMaxX() / 2 - count, buf.ogGetMaxY() / 2 - count,
                 buf.ogGetMaxX() / 2 + count, buf.ogGetMaxY() / 2 + count,
                 count);
  else
    for (count=0; count<1000; count++)
      buf.ogRect(buf.ogGetMaxX() / 2 - count, buf.ogGetMaxY() / 2 - count,
                 buf.ogGetMaxX() / 2 + count, buf.ogGetMaxY() / 2 + count,
                 buf.ogPack(count,count,count));
  getc(stdin);
  return;
} // testRect
  
void testLine(ogSurface & buf) {
  uInt32 count;
  uInt32 colour;
  buf.ogClear(buf.ogPack(0, 0, 0));
  for (count = 150; count > 0; count--) {
    buf.ogLine(buf.ogGetMaxX() / 2, buf.ogGetMaxY() / 2,
                  buf.ogGetMaxX(), count*4, buf.ogPack(192, 192, 192));
  } // for count
  getc(stdin);
  if (buf.ogGetBPP()==8)
    for (count=0; count<(buf.ogGetMaxX()+1); count+=2) {
      buf.ogLine(count-10,-10,buf.ogGetMaxX()-count+10,buf.ogGetMaxY()+10,count);
    } // for
  else {
    colour = 255;
    for (count = 0; count < (buf.ogGetMaxX()+1)/2; count+=4) {
      buf.ogLine(buf.ogGetMaxX()/2, buf.ogGetMaxY()/2,
                 buf.ogGetMaxX()/2-count,buf.ogGetMaxY(),
                 buf.ogPack(colour,colour,colour));
      buf.ogLine(buf.ogGetMaxX()/2, buf.ogGetMaxY()/2,
                 buf.ogGetMaxX()/2+count,buf.ogGetMaxY(),
                 buf.ogPack(colour,colour,colour));
      buf.ogLine(buf.ogGetMaxX()/2, buf.ogGetMaxY()/2,
                 buf.ogGetMaxX()/2-count,0,
                 buf.ogPack(0,colour,0));
      buf.ogLine(buf.ogGetMaxX()/2, buf.ogGetMaxY()/2,
                 buf.ogGetMaxX()/2+count,0,
                 buf.ogPack(0,colour,0));
                 
      --colour;
    } // for

    colour = 255;
    for (count = 0; count < (buf.ogGetMaxY()+1)/2; count+=4) {
      buf.ogLine(buf.ogGetMaxX()/2, buf.ogGetMaxY()/2,
                 0, buf.ogGetMaxY()/2-count,
                 buf.ogPack(colour,0,0));
      buf.ogLine(buf.ogGetMaxX()/2, buf.ogGetMaxY()/2,
                 0, buf.ogGetMaxY()/2+count,
                 buf.ogPack(colour,0,0));
      buf.ogLine(buf.ogGetMaxX()/2, buf.ogGetMaxY()/2,
                 buf.ogGetMaxX(), buf.ogGetMaxY()/2-count,
                 buf.ogPack(0,0,colour));
      buf.ogLine(buf.ogGetMaxX()/2, buf.ogGetMaxY()/2,
                 buf.ogGetMaxX(), buf.ogGetMaxY()/2+count,
                 buf.ogPack(0,0,colour));
      --colour;
    } // for
  } // else
  getc(stdin);
  return;
} // testLine

void testClear(ogSurface & buf) {
  uInt32 count;
  if (buf.ogGetBPP()==8)
    for (count=0; count<256; count++)
      buf.ogClear(count);
  else {
    for (count=0; count<256; count+=8)
      buf.ogClear(buf.ogPack(count,0,0));
    for (count=0; count<256; count+=8)
      buf.ogClear(buf.ogPack(0,count,0));
    for (count=0; count<256; count+=8)
      buf.ogClear(buf.ogPack(0,0,count));
    for (count=0; count<256; count+=8)
      buf.ogClear(buf.ogPack(count,count,count));
  } // else
  getc(stdin);
  return;
} // testClear

void testCircle(ogSurface & buf) {
  uInt32 count;
  if (buf.ogGetBPP()==8)
    for (count=0; count<1000; count++)
      buf.ogCircle(buf.ogGetMaxX()/2,buf.ogGetMaxY()/2, count, count);
  else
    for (count=0; count<1000; count++)
      buf.ogCircle(buf.ogGetMaxX()/2,buf.ogGetMaxY()/2,count,buf.ogPack(count,0,count));
  getc(stdin);
} // testCircle

void testVFlip(ogSurface & buf) {
  buf.ogVFlip();
  getc(stdin);
  return;
} // testVFlip

void testHFlip(ogSurface & buf) {
  buf.ogHFlip();
  getc(stdin);  
  return;
} // testHFlip

void testArc(ogSurface & buf) {
  uInt32 radius;
  uInt32 mid_x, mid_y;
  mid_x = buf.ogGetMaxX()/2;
  mid_y = buf.ogGetMaxY()/2;
  if (buf.ogGetBPP()==8) {
    for (radius = 1; radius <9; radius++) {
      buf.ogArc(mid_x, mid_y, radius*10, 0, 90, radius*15);
      buf.ogArc(mid_x, mid_y, radius*10, 180,270, 249-(radius-1)*16);
    } // for
  } else {
    for (radius = 1; radius <255; radius++) {
      buf.ogArc(mid_x, mid_y, radius, 0, 90, buf.ogPack(radius,radius,0));
      buf.ogArc(mid_x, mid_y, radius, 180,270,buf.ogPack(0,255-radius,255-radius));
    } // for
  } // else
  getchar();
  return;
} // testArc

void testCubicBezierCurve(ogSurface & buf) {
  buf.ogCubicBezierCurve(100, 100,
                         300,50,
                         400,120,
                         350,300,
                         25, buf.ogPack(255,255,255));
  getchar();
  return;
} // testCubicBezierCurve

void testCurve(ogSurface & buf) {
  buf.ogCurve(10,10,100,30,35,160,20,buf.ogPack(255,255,255));
  getchar();
  return;
} // testCurve

void testSprite(ogSurface & buf) {
  uInt32 count;
  uInt32 w,h;
  ogSprite * sprite = NULL;
  sprite = new ogSprite();
  
  buf.ogClear(buf.ogPack(0, 0, 0));
  testLine(buf);
  if (buf.ogGetBPP()==8)
    sprite->Get(buf,
                buf.ogGetMaxX()/2-80,buf.ogGetMaxY()/2-80,
                buf.ogGetMaxX()/2+80,buf.ogGetMaxY()/2+80);
  else
    sprite->Get(buf,
                buf.ogGetMaxX()/2-150,buf.ogGetMaxY()/2-150,
                buf.ogGetMaxX()/2+150,buf.ogGetMaxY()/2+150);

  sprite->Save("test.spr");
  delete sprite;
  sprite = new ogSprite();
  sprite->Load("test.spr");
  w = sprite->GetWidth()/2;
  h = sprite->GetHeight()/2;
  buf.ogClear(buf.ogPack(0, 0, 0));
  sprite->Put(buf,-10000,-10000);  // test *really* off the screen
  sprite->Put(buf,10000,10000);    // test *really* off the screen
  
  sprite->Put(buf,buf.ogGetMaxX()/2-w,buf.ogGetMaxY()/2-h);
  sprite->Put(buf,-w,-h);
  sprite->Put(buf,buf.ogGetMaxX()/2-w,-h);
  sprite->Put(buf,buf.ogGetMaxX()-w,-h);
  sprite->Put(buf,-w,buf.ogGetMaxY()/2-h);
  sprite->Put(buf,buf.ogGetMaxX()-w,buf.ogGetMaxY()/2-h);
  sprite->Put(buf,-w,buf.ogGetMaxY()-h);
  sprite->Put(buf,buf.ogGetMaxX()/2-w,buf.ogGetMaxY()-h);
  sprite->Put(buf,buf.ogGetMaxX()-w,buf.ogGetMaxY()-h);
  getc(stdin);
  for (count = 0; count < 256; count++) {
    sprite->Put(buf,random() % (buf.ogGetMaxX()+sprite->GetWidth()) - sprite->GetWidth(),
                    random() % (buf.ogGetMaxY()+sprite->GetHeight()) - sprite->GetHeight());
  } // for
  delete sprite;
  getc(stdin);
  return;
} // testSprite

void testBlit(ogSurface & buf) {
  int32 xx, yy, count;
  ogBlit * blit = NULL;
  ogBlit * blit2 = NULL;

  blit = new ogBlit();
 
  buf.ogClear(buf.ogPack(0, 0, 0));
  for (xx= 0; xx<= 20; xx++) {
    buf.ogLine(128,0,128-xx*6,255,buf.ogPack(255,255,255));
    buf.ogLine(128,0,128+xx*6,255,buf.ogPack(255,255,255));
    buf.ogLine(128,255,128-xx*6,0,buf.ogPack(255,255,255));
    buf.ogLine(128,255,128+xx*6,0,buf.ogPack(255,255,255));
  } // for

  buf.ogFillCircle(128,128,60,buf.ogPack(255,255,255));
  blit->Get(buf,0,0,255,255);
  for (yy = 0; yy<=(int32)buf.ogGetMaxY(); yy++)
    for (xx = 0; xx<=(int32)buf.ogGetMaxX(); xx++)
      buf.ogSetPixel(xx,yy,xx*yy);
  blit->Save("test.blt");
  blit2 = new ogBlit(*blit, true);

  delete blit;
  blit2->Save("test2.blt"); 
  blit = new ogBlit();
  blit->Load("test.blt");

  blit->Put(buf,-10000,-10000);  // test *really* off the screen
  blit->Put(buf,10000,10000);    // test *really* off the screen

  blit->Put(buf,-128,-128);
  blit->Put(buf,buf.ogGetMaxX()/2-128,-128);
  blit->Put(buf,buf.ogGetMaxX()-128,-128);
  blit->Put(buf,-128,buf.ogGetMaxY()/2-128);
  blit->Put(buf,buf.ogGetMaxX()/2-128,buf.ogGetMaxY()/2-128);
  blit->Put(buf,buf.ogGetMaxX()-128,buf.ogGetMaxY()/2-128);
  blit->Put(buf,-128,buf.ogGetMaxY()-128);
  blit->Put(buf,buf.ogGetMaxX()/2-128,buf.ogGetMaxY()-128);
  blit->Put(buf,buf.ogGetMaxX()-128,buf.ogGetMaxY()-128);

  getc(stdin);
  buf.ogClear(buf.ogPack(0, 0, 0));
  for (yy = 0; yy<=(int32)buf.ogGetMaxY(); yy++)
    for (xx = 0; xx<=(int32)buf.ogGetMaxX(); xx++)
      buf.ogSetPixel(xx,yy,xx*yy);
  blit->GetBlitWithMask(buf,
                        buf.ogGetMaxX()/2-blit->GetWidth(),
                        buf.ogGetMaxY()/2-blit->GetHeight());
  buf.ogClear(buf.ogPack(0, 0, 0));
  blit->Put(buf,-10000,-10000);  // test *really* off the screen
  blit->Put(buf,10000,10000);    // test *really* off the screen
  
  blit->Put(buf,-128,-128);
  blit->Put(buf,buf.ogGetMaxX()/2-128,-128);
  blit->Put(buf,buf.ogGetMaxX()-128,-128);
  blit->Put(buf,-128,buf.ogGetMaxY()/2-128);
  blit->Put(buf,buf.ogGetMaxX()/2-128,buf.ogGetMaxY()/2-128);
  blit->Put(buf,buf.ogGetMaxX()-128,buf.ogGetMaxY()/2-128);
  blit->Put(buf,-128,buf.ogGetMaxY()-128);
  blit->Put(buf,buf.ogGetMaxX()/2-128,buf.ogGetMaxY()-128);
  blit->Put(buf,buf.ogGetMaxX()-128,buf.ogGetMaxY()-128);
  
  getc(stdin);
  for (count = 0; count < 1000; count++) {
    blit->Put(buf,random() % (buf.ogGetMaxX()+blit->GetWidth()) - blit->GetWidth(),
                  random() % (buf.ogGetMaxY()+blit->GetHeight()) - blit->GetHeight());
  } // for
  getc(stdin);
  for (yy = 0; yy<=(int32)buf.ogGetMaxY(); yy++)
    for (xx = 0; xx<=(int32)buf.ogGetMaxX(); xx++)
      buf.ogSetPixel(xx,yy,xx*yy);

  blit->GetBlitWithMask(buf,buf.ogGetMaxX()/2,buf.ogGetMaxY()-128);
  buf.ogClear(buf.ogPack(128,128,128));
  blit->Put(buf,buf.ogGetMaxX()/2-128,buf.ogGetMaxY()/2-128);  
  getc(stdin);
  delete blit;
  delete blit2;
  
  return;
} // testBlit

void
testBlit2(ogSurface & buf) {
  ogBlit * blit = new ogBlit();
  ogSurface * buf2 = new ogSurface();
  ogSurface * blitsource = new ogSurface();
  uInt32 xx,yy,count,colour;
  buf.ogClear(buf.ogPack(0, 0, 0));
  if (!buf2->ogClone(buf)) cout << "Clone failed!!!" << endl;
  if (!blitsource->ogClone(buf)) cout << "Clone failed!!" << endl;

  colour = 255;
  for (count = 0; count < (buf2->ogGetMaxX()+1)/2; count+=4) {
    buf2->ogLine(buf2->ogGetMaxX()/2, buf2->ogGetMaxY()/2,
               buf2->ogGetMaxX()/2-count,buf2->ogGetMaxY(),
               buf2->ogPack(colour,colour,colour));
    buf2->ogLine(buf2->ogGetMaxX()/2, buf2->ogGetMaxY()/2,
               buf2->ogGetMaxX()/2+count,buf2->ogGetMaxY(),
               buf2->ogPack(colour,colour,colour));
    buf2->ogLine(buf2->ogGetMaxX()/2, buf2->ogGetMaxY()/2,
               buf2->ogGetMaxX()/2-count,0,
               buf2->ogPack(0,colour,0));
    buf2->ogLine(buf2->ogGetMaxX()/2, buf2->ogGetMaxY()/2,
               buf2->ogGetMaxX()/2+count,0,
               buf2->ogPack(0,colour,0));
    --colour;
  } // for

  colour = 255;
  for (count = 0; count < (buf.ogGetMaxY()+1)/2; count+=4) {
    buf2->ogLine(buf2->ogGetMaxX()/2, buf2->ogGetMaxY()/2,
               0, buf2->ogGetMaxY()/2-count,
               buf2->ogPack(colour,0,0));
    buf2->ogLine(buf2->ogGetMaxX()/2, buf2->ogGetMaxY()/2,
               0, buf2->ogGetMaxY()/2+count,
               buf2->ogPack(colour,0,0));
    buf2->ogLine(buf2->ogGetMaxX()/2, buf2->ogGetMaxY()/2,
               buf2->ogGetMaxX(), buf2->ogGetMaxY()/2-count,
               buf2->ogPack(0,0,colour));
    buf2->ogLine(buf2->ogGetMaxX()/2, buf2->ogGetMaxY()/2,
               buf2->ogGetMaxX(), buf2->ogGetMaxY()/2+count,
               buf2->ogPack(0,0,colour));
    --colour;
  } // for
  for (yy = 0; yy<=buf2->ogGetMaxY(); yy++)
    for (xx = 0; xx<=buf2->ogGetMaxX(); xx++)
      buf2->ogSetPixel(xx,yy, xx*yy);
  for (count = 0; count<200; count++)
    blitsource->ogFillCircle(random() % blitsource->ogGetMaxX(), random() % blitsource->ogGetMaxY(),
                             random() % 35+1,blitsource->ogPack(255,255,255));

  blit->Get(*blitsource,0,0,blitsource->ogGetMaxX(),blitsource->ogGetMaxY());
  blit->Save("bigblit.blt");
  delete blit;
  blit = new ogBlit;
  blit->Load("bigblit.blt");
  for (count = 0; count<=256; count+=4) {
  for (yy = 0; yy<=buf2->ogGetMaxY(); yy++)
    for (xx = 0; xx<=buf2->ogGetMaxX(); xx++)
      buf2->ogSetPixel(xx,yy,xx*yy*count);
  
    blit->GetBlitWithMask(*buf2,0,0);
    blit->Put(buf,0,0);
  } // for

  getc(stdin);
  delete blitsource;
  delete buf2;
  delete blit;
  return;
} // testBlit2

void
testBlit3(ogSurface & buf) {
  int32 count;
  ogBlit * blit = NULL;
  ogSurface * buf2 = NULL;
  buf2 = new ogSurface();
  blit = new ogBlit();
  buf.ogClear(buf.ogPack(0, 0, 0));
  buf.ogFillCircle(buf.ogGetMaxX()/2,buf.ogGetMaxY()/2,14,buf.ogPack(255,255,255));
  blit->Get(buf,buf.ogGetMaxX()/2-20,buf.ogGetMaxY()/2-20,
                buf.ogGetMaxX()/2+20,buf.ogGetMaxY()/2+20);
  blit->Put(buf,0,0);
  for (count=0; count<(int32)buf.ogGetMaxX(); count++)
    buf.ogLine(count,0,count,buf.ogGetMaxY(),count);
  buf2->ogClone(buf);    
  blit->GetBlitWithMask(*buf2,10,10);
  buf.ogClear(buf.ogPack(63,63,63));
  for (count=-40; count<(int32)buf.ogGetMaxX()+10; count++) {
    blit->GetBlitWithMask(*buf2,count,buf2->ogGetMaxY()/2-20);
    blit->Put(buf,count,buf.ogGetMaxY()/2-20);
    blit->GetBlitWithMask(*buf2,buf2->ogGetMaxX()/2-20,count);
    blit->Put(buf,buf.ogGetMaxX()/2-20,count);
   }
  getc(stdin);
  delete blit;
  return;
} // testBlit3

void
testSaveLoadPal(ogSurface & buf) {
  uInt32 count;
  testRect(buf);
  for (count=0; count<256; count++)
    buf.ogSetPalette(count,count,count,count);
  if (buf.ogSavePalette("test.pal")==false) cout << "SavePal() failed" << endl;
  for (count=0; count<256; count++)
    buf.ogSetPalette(count,0,0,0);
  if (buf.ogLoadPalette("test.pal")==false) cout << "LoadPal() failed" << endl;
  testRect(buf);
} // testSaveLoadPal

void
testPolygon(ogSurface & buf) {
  ogPoint2d points[16];
  uInt32 count;
  buf.ogClear(buf.ogPack(0, 0, 0));
  for (count=0; count<16; count++) {
    points[count].x = random() % buf.ogGetMaxX();
    points[count].y = random() % buf.ogGetMaxY();
  } // for
  buf.ogFillPolygon(16, points, buf.ogPack(random() & 255,random() & 255,random() & 255));
  getc(stdin);
  return;
} // testPolygon

void
testSpline(ogSurface & buf) {
  ogPoint2d points[8];
  uInt32 i;
  for (i=0; i<8; i++) {
    points[i].x = random() % buf.ogGetMaxX();
    points[i].y = random() % buf.ogGetMaxY();
  } // for
  buf.ogClear(buf.ogPack(0, 0, 0));
  buf.ogPolygon(8, points, buf.ogPack(22,229,52));
  buf.ogSpline(8, points, 24, buf.ogPack(64,64,255));
  buf.ogBSpline(8, points, 24, buf.ogPack(255,128,128));
  getchar();
  return;
} // testSpline

void
testFont(ogSurface & buf) {
  uInt32 xx, yy;
  ogBitFont * font = new ogBitFont();
  font->Load("SCRIPT.DPF", 0);
  font->SetFGColor(255, 255, 255, 255);
  font->SetBGColor(0, 0, 0, 255);

  font->PutString(buf, 0, 0, "abAByzYZ");

  for (yy = 0; yy<font->GetHeight(); yy++) {
    for (xx = 0; xx<79; xx++)
      if ((buf.ogGetPixel(xx, yy) & buf.ogGetAlphaMasker()) == 0) 
        cout << " ";
      else
        cout << "*";
    cout << endl;
  }

  delete font;
  return;
} // testFont

void
testGouraud(ogSurface & buf) {
  uInt8 r, g, b;
  ogPoint2d points[4];
  ogRGBA8 colours[4];
  r = g = b = 0;

  points[0].x = buf.ogGetMaxX() - 150;
  points[0].y = 0;
  points[1].x = buf.ogGetMaxX();
  points[1].y = 0;
  points[2].x = buf.ogGetMaxX();
  points[2].y = 150;
  points[3].x = buf.ogGetMaxX() - 250;
  points[3].y = 250;
  colours[0].red = 255;
  colours[0].green = 0;
  colours[0].blue = 0;
  colours[0].alpha = 255;
  colours[1].red = 0;
  colours[1].green = 255;
  colours[1].blue = 128;
  colours[1].alpha = 255;
  colours[2].red = 128;
  colours[2].green = 255;
  colours[2].blue = 128;
  colours[2].alpha = 255;
  colours[3].red = 63;
  colours[3].green = 63;
  colours[3].blue = 63;
  colours[3].alpha = 255;
  buf.ogFillGouraudPolygon(4, points, colours);
  getc(stdin);
  return;
} // testGouraud

void
testCopyBuf(ogSurface & buf) {
  ogSurface * buf2 = new ogSurface();
  ogPixelFmt pixFmt;
  buf.ogGetPixFmt(pixFmt);
  buf2->ogCreate(400, 400, OG_PIXFMT_32BPP);
  buf2->ogClear(buf2->ogPack(255, 128, 255));
  buf.ogCopyBuf(0, 0, *buf2, 0, 0, buf2->ogGetMaxX(), buf2->ogGetMaxY());
  delete buf2;
  getc(stdin);
  return;
}

void
testFillRect(ogSurface & buf) {
  uInt32 count;
  for (count = 100; count > 0; count--) {
    buf.ogFillRect(count, count, 0, 0, buf.ogPack(count*2, count*2, count*2));
  }
  getc(stdin);
  
  return;
} // testFillRect()
int main() {
  ogSurface* buf    = NULL;
 cout <<  "Starting program" << endl;
#ifdef __DJGPP__
  buf = new ogDisplay_VESA();
  buf->ogCreate(800, 600, OG_PIXFMT_32BPP);
#else
  buf = new ogSurface();
  if (buf->ogCreate(1024, 768, OG_PIXFMT_16BPP)==false) exit(1);
#endif
  srandom(time(NULL));

//  buf->SetBlending(true);
//  buf->SetAlpha(127);
  buf->ogSetAntiAliasing(true);
  
//cout << "testPixCon()" << endl;
//  testPixCon();
 cout << "testFillRect()" << endl;
  testFillRect(*buf);
 cout << "testCopyBuf()" << endl;
  testCopyBuf(*buf);
 cout << "testGouraud()" << endl;
  testGouraud(*buf);
 cout << "testFont()" << endl;
  testFont(*buf);
 cout << "TestRect()" << endl;
  testRect(*buf);
 cout << "testCircle()" << endl;
  testCircle(*buf);
 cout << "TestLine()" << endl;
  testLine(*buf);
cout << "TestVFlip()" << endl;
  testVFlip(*buf);
cout << "TestHFlip()" << endl;
  testHFlip(*buf);
cout << "TestArc()" << endl;
  testArc(*buf);
cout << "TestSetPixel()" << endl;
  testSetPixel(*buf);
cout << "TestClear()" << endl;
  testClear(*buf);
cout << "TestSprite()" << endl;
  testSprite(*buf);
cout << "TestBlit()" << endl;
   testBlit(*buf);
cout << "TestBlit2()" << endl;
   testBlit2(*buf);
//  testBlit3(*buf);  // special test for 320x200x8bpp
//  testSaveLoadPal(*buf);    // use an 8bpp mode
cout << "TestPolygon()" << endl;
  testPolygon(*buf);
cout << "TestCurve()" << endl;
  testCurve(*buf);
cout << "TestSpline()" << endl;
  testSpline(*buf);
  delete buf;
//  buf->SetPixel(0, 0, buf->Pack(0, 0, 0)); 
  return(0);
}
