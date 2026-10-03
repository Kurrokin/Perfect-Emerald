# v6.1 UI BAG Modern / Perfect: the new bag look (orange background, dark item list
# panel, dark description panel with a white item-icon box) built for the game's
# existing bag layout. Special tiles at fixed numbers the bag code draws itself:
#   11 = background (pocket-switch wipe), 17 = list panel fill,
#   0x17 / 0x2B = pocket dots (palette 1: inactive / active).
import struct, os
from PIL import Image
B='/home/claude/pokeemerald_modern_hack_source/'
import sys
PERFECT = len(sys.argv) > 1 and sys.argv[1] == 'perfect'
OUT=B+'graphics/bag/modern/'; os.makedirs(OUT,exist_ok=True)
SUF='_perfect' if PERFECT else ''
P0=['#ffd065','#ffd065','#ffba45','#2f2e36','#4e5364','#242424','#f8f8f8','#9dabb2','#ef5a00','#3b3949','#383838','#ffd065','#ffd065','#ffd065','#ffd065','#000000']
idx0={c:i for i,c in reversed(list(enumerate(P0)))}
W,H=240,160
img=[['#ffd065']*W for _ in range(H)]
def rect(x0,y0,x1,y1,c):
    for y in range(max(0,y0),min(H,y1)):
        for x in range(max(0,x0),min(W,x1)): img[y][x]=c
def rrect(x0,y0,x1,y1,c,border=None):
    rect(x0,y0,x1,y1,c)
    for (cx,cy) in ((x0,y0),(x1-1,y0),(x0,y1-1),(x1-1,y1-1)): img[cy][cx]='#ffd065' if cy<136 else '#ffba45'
    if border:
        for x in range(x0+1,x1-1): img[y0][x]=border; img[y1-1][x]=border
        for y in range(y0+1,y1-1): img[y][x0]=border; img[y][x1-1]=border
rect(0,136,W,H,'#ffba45')                      # darker band at the bottom
# item list panel: the wipe area is tiles 14..28 x 2..17 -> fill flat; a dark frame just outside it
rrect(110,14,234,146,'#242424')
rect(112,16,232,144,'#2f2e36')
# description panel (window 0,13 14x6 -> 0..112 x 104..152) and the item icon box
rrect(2,102,110,154,'#4e5364','#242424')
rrect(10,75,40,103,'#f8f8f8','#242424')   # v6.3: clear of the party panels
# Perfect: six party panels (3 x 2) where the bag picture was
if PERFECT:
    for r in range(2):
        for c in range(3):
            x0=4+c*35; y0=33+r*21
            rrect(x0,y0,x0+33,y0+19,'#2f2e36','#242424')
# pocket name tab (window 4,1 8x2 -> 32..96 x 8..24)
rrect(28,7,100,25,'#2f2e36','#242424')
# tiles
tiles={}; order=[None]*512
def put_fixed(i,t): order[i]=t; tiles[t]=i
flat=lambda c:tuple([idx0[c]]*64)
put_fixed(0,flat('#ffd065'))
put_fixed(11,flat('#ffd065'))
put_fixed(17,flat('#2f2e36'))
# pocket dots: palette 1 entries (12 = orange, 3 = dark, 1 = white)
def dot(colour_idx):
    t=[12]*64
    for y in range(2,6):
        for x in range(2,6): t[y*8+x]=colour_idx
    return tuple(t)
order[0x17]=dot(3); order[0x2B]=dot(1)
nxt=1
def alloc(t):
    global nxt
    if t in tiles: return tiles[t]
    while order[nxt] is not None: nxt+=1
    order[nxt]=t; tiles[t]=nxt; return nxt
tm=[[0]*32 for _ in range(32)]
for ty in range(20):
    for tx in range(30):
        t=tuple(idx0[img[ty*8+y][tx*8+x]] for y in range(8) for x in range(8))
        tm[ty][tx]=alloc(t)
n=max(i for i,t in enumerate(order) if t is not None)+1
print('tiles',n)
pal_rgb=[(int(c[1:3],16),int(c[3:5],16),int(c[5:7],16)) for c in P0]
im=Image.new('P',(128,((n+15)//16)*8)); im.putpalette([v for c in pal_rgb for v in c]); px=im.load()
for i in range(n):
    t=order[i] or tuple([0]*64)
    for y in range(8):
        for x in range(8): px[(i%16)*8+x,(i//16)*8+y]=t[y*8+x]
im.save(OUT+'menu'+SUF+'.png')
with open(OUT+'menu'+SUF+'.bin','wb') as f:
    for row in tm:
        for v in row: f.write(struct.pack('<H',v))
# palettes: 0 = background, 1 = the game's text palette with white text, a dark shadow and the dot colours
van=open(B+'graphics/bag/menu_male.gbapal','rb').read()
p1=[struct.unpack_from('<H',van,32+i*2)[0] for i in range(16)]
def rgb15(c): r,g,b=c; return (r>>3)|((g>>3)<<5)|((b>>3)<<10)
p1[1]=rgb15((248,248,248)); p1[3]=rgb15((40,40,48)); p1[12]=rgb15((255,208,101))
p1[4]=rgb15((36,36,36))   # v6.3: pocket name shadow dark (was tan) on the dark tab
# HP bar colours for the Perfect party panels
p1[13]=rgb15((24,198,33)); p1[11]=rgb15((239,173,0)); p1[15]=rgb15((255,74,57))
with open(OUT+'menu_pal.gbapal','wb') as f:
    for c in pal_rgb: f.write(struct.pack('<H',rgb15(c)))
    for v in p1: f.write(struct.pack('<H',v))
# preview
prev=Image.new('RGB',(W,H)); pp=prev.load()
for y in range(H):
    for x in range(W): pp[x,y]=pal_rgb[idx0[img[y][x]]]
prev.resize((480,320),Image.NEAREST).save('/home/claude/bagui/preview'+SUF+'.png')
