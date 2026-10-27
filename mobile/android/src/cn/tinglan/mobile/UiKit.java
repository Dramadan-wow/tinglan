package cn.tinglan.mobile;

import android.content.Context;
import android.graphics.*;
import android.graphics.drawable.*;
import android.content.res.ColorStateList;
import android.view.View;
import java.util.HashMap;
import java.util.Map;

/** Shared native drawing primitives; original desktop artwork is packaged at build time. */
final class UiKit {
    static final class Palette {
        final int background,ink,accent,muted,line,surface,soft,onAccent;
        Palette(int background,int ink,int accent,int muted,int line,int surface,int soft,int onAccent){
            this.background=background;this.ink=ink;this.accent=accent;this.muted=muted;this.line=line;this.surface=surface;this.soft=soft;this.onAccent=onAccent;
        }
    }
    static final Palette RIVER=new Palette(0xfff3f6f3,0xff203c33,0xff285e4b,0xff7b8b82,0xffe5ebe5,0xffffffff,0xffe8eee8,0xffffffff);
    static final Palette YVONNE=new Palette(0xfff6f0f7,0xff39273f,0xffd573a4,0xff856c86,0xffebdfea,0xfffffbfe,0xfff6e3ee,0xff2c2034);
    /** Desktop signed-in workspace colours (gui/ThemeHsx.cpp, ThemeHsx::workspace). */
    static final Palette SKY=new Palette(0xfff5f9ff,0xff233650,0xff1689ff,0xff8295b0,0xffe2ecf8,0xffffffff,0xffd5eaff,0xffffffff);
    static final int ONLINE=0xff25b57a,BADGE=0xffff515e;
    private static final int[] SEEDS={0xff578fcb,0xff8c9fda,0xffc5a17d,0xff79afaa,0xffb691c4,0xff669bac};
    /** Same per-user colour as ThemeHsx::color on desktop. */
    static int seed(int id){return SEEDS[(int)((id&0xffffffffL)%SEEDS.length)];}
    static int blend(int color,int other,float amount){
        int r=Math.round(Color.red(color)+(Color.red(other)-Color.red(color))*amount),g=Math.round(Color.green(color)+(Color.green(other)-Color.green(color))*amount),b=Math.round(Color.blue(color)+(Color.blue(other)-Color.blue(color))*amount);
        return Color.rgb(r,g,b);
    }
    /** Bubble catalog from workspace/AppearanceHsx.cpp: start, end and text colour. Desktop draws the default "jade" in workspace blue. */
    static int[] bubble(String id){
        switch(id==null?"":id){
            case "sky":return new int[]{0xffddecff,0xffe9f3ff,0xff274c70};
            case "peach":return new int[]{0xffffe1e8,0xfffff1ef,0xff7d4157};
            case "lilac":return new int[]{0xffe8dffc,0xfff3edff,0xff5a467b};
            case "sun":return new int[]{0xffffe8ba,0xfffff3d6,0xff79522e};
            case "paper":return new int[]{0xffffffff,0xfff7f8fc,0xff47566c};
            case "mint":return new int[]{0xffa9f1df,0xffddfcef,0xff216352};
            case "sakura":return new int[]{0xfff9cbe1,0xfffce9f5,0xff824765};
            case "candy":return new int[]{0xfff7c7ff,0xffbedfff,0xff63407c};
            case "ocean":return new int[]{0xff95d8ff,0xffd6f4ff,0xff255b80};
            case "sunset":return new int[]{0xffffc89b,0xffffe2e9,0xff864c3d};
            case "aurora":return new int[]{0xffb3eee2,0xffd4c4ff,0xff425b72};
            case "galaxy":return new int[]{0xff41497e,0xff7461a3,0xffffffff};
            case "gold":return new int[]{0xffffedb2,0xffefd4a2,0xff7d5930};
            case "prism":return new int[]{0xffeec7ff,0xffadf5ed,0xff574678};
            default:return new int[]{0xffb9dfff,0xffcee8ff,0xff233650};
        }
    }
    /** Avatar frame rings from the same catalog; "none" has no ring. */
    static int[] frame(String id){
        switch(id==null?"":id){
            case "mint":return new int[]{0xff70cba3,0xffa8ead2};
            case "rose":return new int[]{0xffed91bd,0xffb8a3fa};
            case "star":return new int[]{0xff798be8,0xff89dfec};
            case "gold":return new int[]{0xffd7a74a,0xffffe8a4};
            default:return null;
        }
    }
    /** Desktop BubbleHsx outline: the top corner next to the sender's avatar is nearly square. */
    static GradientDrawable bubble(int[] style,boolean own,float density){
        GradientDrawable d=new GradientDrawable(GradientDrawable.Orientation.TL_BR,new int[]{style[0],style[1]});float big=16*density,small=5*density;
        d.setCornerRadii(own?new float[]{big,big,small,small,big,big,big,big}:new float[]{small,small,big,big,big,big,big,big});
        d.setStroke(Math.max(1,Math.round(density)),blend(style[0],Color.BLACK,.08f));return d;
    }
    static GradientDrawable shape(int color,float radius,float density){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(radius*density);return d;}
    static RippleDrawable touch(int fill,int tint,int radius,float density){return new RippleDrawable(ColorStateList.valueOf((tint&0x00ffffff)|0x22000000),shape(fill,radius,density),shape(Color.WHITE,radius,density));}

    /** Draws a 24-unit line icon into a size×size square at (x,y). */
    static void glyph(Canvas canvas,String name,int color,float x,float y,float size,Paint p){
        canvas.save();float scale=size/24f;canvas.translate(x,y);canvas.scale(scale,scale);p.setShader(null);p.setColor(color);p.setStyle(Paint.Style.STROKE);p.setStrokeWidth(1.7f);p.setStrokeCap(Paint.Cap.ROUND);p.setStrokeJoin(Paint.Join.ROUND);
        Path path=new Path();
        switch(name){
            case "chat":path.moveTo(5,4);path.lineTo(19,4);path.quadTo(21,4,21,6);path.lineTo(21,15);path.quadTo(21,17,19,17);path.lineTo(10,17);path.lineTo(5,21);path.lineTo(5,17);path.quadTo(3,17,3,15);path.lineTo(3,6);path.quadTo(3,4,5,4);canvas.drawPath(path,p);canvas.drawLine(7,9,17,9,p);canvas.drawLine(7,13,14,13,p);break;
            case "users":canvas.drawCircle(9,8,3,p);path.moveTo(3,20);path.lineTo(3,18);path.cubicTo(3,12,15,12,15,18);path.lineTo(15,20);canvas.drawPath(path,p);canvas.drawArc(new RectF(13,5,21,11),-90,160,false,p);canvas.drawArc(new RectF(14,14,22,22),-90,90,false,p);break;
            case "group":canvas.drawCircle(12,8,3.2f,p);path.moveTo(6.5f,20);path.cubicTo(6.5f,13.5f,17.5f,13.5f,17.5f,20);canvas.drawPath(path,p);canvas.drawCircle(5,10,2.2f,p);canvas.drawCircle(19,10,2.2f,p);path.reset();path.moveTo(1.5f,18.5f);path.quadTo(1.5f,14.5f,5,14.5f);path.moveTo(22.5f,18.5f);path.quadTo(22.5f,14.5f,19,14.5f);canvas.drawPath(path,p);break;
            case "person":canvas.drawCircle(12,8,3.5f,p);path.moveTo(4,21);path.lineTo(4,19);path.cubicTo(4,12,20,12,20,19);path.lineTo(20,21);canvas.drawPath(path,p);break;
            case "bell":path.moveTo(6,17);path.lineTo(6,11);path.cubicTo(6,7,8.6f,4.5f,12,4.5f);path.cubicTo(15.4f,4.5f,18,7,18,11);path.lineTo(18,17);canvas.drawPath(path,p);canvas.drawLine(4,17,20,17,p);canvas.drawArc(new RectF(10,17,14,21),0,180,false,p);canvas.drawLine(12,2.5f,12,4.5f,p);break;
            case "smile":canvas.drawCircle(12,12,9,p);canvas.drawArc(new RectF(8,9,16,17),25,130,false,p);canvas.drawCircle(9,10,.5f,p);canvas.drawCircle(15,10,.5f,p);break;
            case "info":canvas.drawCircle(12,12,9,p);canvas.drawLine(12,11,12,16.5f,p);canvas.drawCircle(12,7.8f,.4f,p);break;
            case "compass":canvas.drawCircle(12,12,9,p);path.moveTo(16,8);path.lineTo(14,14);path.lineTo(8,16);path.lineTo(10,10);path.close();canvas.drawPath(path,p);break;
            case "heart":path.moveTo(12,20);path.cubicTo(0,12,2,3,8,5);path.quadTo(11,5,12,8);path.quadTo(14,4,18,5);path.cubicTo(25,8,18,17,12,20);canvas.drawPath(path,p);break;
            case "mail":canvas.drawRoundRect(new RectF(3,5,21,19),3,3,p);path.moveTo(3,6);path.lineTo(12,13);path.lineTo(21,6);canvas.drawPath(path,p);break;
            case "send":path.moveTo(3,4);path.lineTo(22,12);path.lineTo(3,20);path.lineTo(6,12);path.close();canvas.drawPath(path,p);canvas.drawLine(6,12,16,12,p);break;
            case "globe":canvas.drawCircle(12,12,9,p);canvas.drawOval(new RectF(8,3,16,21),p);canvas.drawLine(3,12,21,12,p);break;
            case "plus":canvas.drawLine(12,5,12,19,p);canvas.drawLine(5,12,19,12,p);break;
            case "back":path.moveTo(15,5);path.lineTo(8,12);path.lineTo(15,19);canvas.drawPath(path,p);break;
            case "search":canvas.drawCircle(10,10,6,p);canvas.drawLine(15,15,21,21,p);break;
            case "scan":path.moveTo(3,8);path.lineTo(3,3);path.lineTo(8,3);path.moveTo(16,3);path.lineTo(21,3);path.lineTo(21,8);path.moveTo(21,16);path.lineTo(21,21);path.lineTo(16,21);path.moveTo(8,21);path.lineTo(3,21);path.lineTo(3,16);canvas.drawPath(path,p);canvas.drawLine(6,12,18,12,p);break;
            case "flame":path.moveTo(12,2.5f);path.cubicTo(14.5f,8,20,9,20,15);path.cubicTo(20,23,4,23,4,15);path.cubicTo(4,11,6.5f,9,8.5f,8);path.cubicTo(8,12,11,11.5f,12,2.5f);canvas.drawPath(path,p);canvas.drawCircle(9.5f,15.5f,.5f,p);canvas.drawCircle(14.5f,15.5f,.5f,p);break;
            // The arrowhead sits on the arc's clockwise end, pointing along it.
            case "refresh":canvas.drawArc(new RectF(4,4,20,20),30,280,false,p);path.moveTo(13.65f,6.18f);path.lineTo(17.14f,5.87f);path.lineTo(16.83f,2.38f);canvas.drawPath(path,p);break;
            case "crown":path.moveTo(4,18);path.lineTo(3,7);path.lineTo(8.5f,11.5f);path.lineTo(12,5);path.lineTo(15.5f,11.5f);path.lineTo(21,7);path.lineTo(20,18);path.close();canvas.drawPath(path,p);canvas.drawLine(5,21,19,21,p);break;
            default:path.moveTo(3,13);path.cubicTo(6,5,10,5,12,12);path.cubicTo(15,20,18,20,21,11);canvas.drawPath(path,p);canvas.drawLine(5,19,19,5,p);break;
        }canvas.restore();
    }
    static final class Icon extends View {
        private final String name;private final int color;private final Paint p=new Paint(Paint.ANTI_ALIAS_FLAG);
        Icon(Context c,String name,int color){super(c);this.name=name;this.color=color;setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);}
        @Override protected void onDraw(Canvas canvas){super.onDraw(canvas);float size=Math.min(getWidth(),getHeight());glyph(canvas,name,color,(getWidth()-size)/2,(getHeight()-size)/2,size,p);}
    }
    /** Desktop AvatarHsx: seeded colour circle with a white initial or the uploaded image, plus an optional frame ring. Groups use a tinted group glyph as in the desktop conversation list. */
    static final class Avatar extends View {
        private final String letter;private final int color;private final Bitmap image;private final int[] ring;private final boolean group;private final float density;
        private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG|Paint.FILTER_BITMAP_FLAG);private final Path clip=new Path();
        Avatar(Context c,String name,int id,Bitmap image,String frame,boolean group){
            super(c);String value=name==null||name.isEmpty()?"澜":name;letter=value.substring(0,value.offsetByCodePoints(0,1));color=seed(id);this.image=image;ring=frame(frame);this.group=group;density=c.getResources().getDisplayMetrics().density;
            setContentDescription(value+(group?"的群头像":"的头像"));
        }
        @Override protected void onDraw(Canvas canvas){
            super.onDraw(canvas);float size=Math.min(getWidth(),getHeight()),cx=getWidth()/2f,cy=getHeight()/2f,r=size/2f-(ring==null?0:3*density);
            paint.setShader(null);paint.setStyle(Paint.Style.FILL);
            if(group){paint.setColor(blend(color,Color.WHITE,.78f));canvas.drawCircle(cx,cy,r,paint);glyph(canvas,"group",color,cx-r*.58f,cy-r*.58f,r*1.16f,paint);}
            else if(image!=null){canvas.save();clip.reset();clip.addCircle(cx,cy,r,Path.Direction.CW);canvas.clipPath(clip);float s=Math.max(2*r/image.getWidth(),2*r/image.getHeight()),w=image.getWidth()*s,h=image.getHeight()*s;canvas.drawBitmap(image,null,new RectF(cx-w/2,cy-h/2,cx+w/2,cy+h/2),paint);canvas.restore();}
            else{paint.setColor(color);canvas.drawCircle(cx,cy,r,paint);paint.setColor(Color.WHITE);paint.setTextSize(r*.92f);paint.setTypeface(Typeface.DEFAULT_BOLD);paint.setTextAlign(Paint.Align.CENTER);Paint.FontMetrics m=paint.getFontMetrics();canvas.drawText(letter,cx,cy-(m.ascent+m.descent)/2,paint);}
            if(ring!=null){paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(2.5f*density);paint.setShader(new LinearGradient(0,0,getWidth(),getHeight(),ring[0],ring[1],Shader.TileMode.CLAMP));canvas.drawCircle(cx,cy,size/2f-1.3f*density,paint);paint.setShader(null);}
        }
    }
    static final class Artwork extends View {
        private static final Map<Integer,Bitmap> CACHE=new HashMap<>();
        private final boolean yvonne;private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG|Paint.FILTER_BITMAP_FLAG);private final float density;
        Artwork(Context context,boolean yvonne){super(context);this.yvonne=yvonne;density=getResources().getDisplayMetrics().density;setContentDescription(yvonne?"伊冯灵感主题，绚色共振":"听澜山水主题，听见此刻");}
        private Bitmap art(){int id=yvonne?R.drawable.yvonne_social:R.drawable.mist_river;if(!CACHE.containsKey(id)){BitmapFactory.Options options=new BitmapFactory.Options();options.inScaled=false;CACHE.put(id,BitmapFactory.decodeResource(getResources(),id,options));}return CACHE.get(id);}
        @Override protected void onDraw(Canvas canvas){super.onDraw(canvas);float w=getWidth(),h=getHeight();Path clip=new Path();clip.addRoundRect(new RectF(0,0,w,h),24*density,24*density,Path.Direction.CW);canvas.save();canvas.clipPath(clip);canvas.drawColor(yvonne?0xff241e30:0xff274b3d);Bitmap bitmap=art();
            if(bitmap!=null){float s=Math.max(w/bitmap.getWidth(),h/bitmap.getHeight());float bw=bitmap.getWidth()*s,bh=bitmap.getHeight()*s;float x=yvonne?w-bw:(w-bw)/2;float y=yvonne?(h-bh)*.25f:(h-bh)*.62f;canvas.drawBitmap(bitmap,null,new RectF(x,y,x+bw,y+bh),paint);}
            if(yvonne)paint.setShader(new LinearGradient(0,0,w,0,new int[]{0xff241e30,0xee241e30,0x18241e30},new float[]{0,.28f,.8f},Shader.TileMode.CLAMP));
            else paint.setShader(new LinearGradient(0,0,0,h,new int[]{0x00213f32,0xb5213f32},null,Shader.TileMode.CLAMP));canvas.drawRect(0,0,w,h,paint);paint.setShader(null);
            float x=22*density;float titleY=yvonne?h*.48f:h*.63f;paint.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));paint.setTextSize(10*density);paint.setColor(yvonne?0xffe5f483:0xffe4ecdf);canvas.drawText(yvonne?"YVONNE / SOCIAL CHANNEL":"TINGLAN / MOMENTS TOGETHER",x,yvonne?30*density:titleY-35*density,paint);
            paint.setTextSize((yvonne?30:29)*density);paint.setTypeface(Typeface.DEFAULT_BOLD);paint.setColor(yvonne?0xffffeff7:Color.WHITE);canvas.drawText(yvonne?"绚色共振":"听见此刻",x,titleY,paint);
            paint.setTypeface(Typeface.DEFAULT);paint.setTextSize(11*density);paint.setColor(yvonne?0xffdcc5d9:0xffe8eee8);canvas.drawText(yvonne?"把日常的灵感，发送到同一频率。":"让每一次相遇，都有回响。",x,titleY+25*density,paint);
            if(yvonne){paint.setColor(0xffed8bbb);canvas.drawRoundRect(new RectF(x,h-43*density,x+78*density,h-21*density),11*density,11*density,paint);paint.setColor(0xff302238);paint.setTextSize(10*density);canvas.drawText("伊冯灵感",x+18*density,h-28*density,paint);}
            canvas.restore();
        }
    }
}
