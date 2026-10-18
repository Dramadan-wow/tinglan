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
    static GradientDrawable shape(int color,float radius,float density){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(radius*density);return d;}
    static RippleDrawable touch(int fill,int tint,int radius,float density){return new RippleDrawable(ColorStateList.valueOf((tint&0x00ffffff)|0x22000000),shape(fill,radius,density),shape(Color.WHITE,radius,density));}

    static final class Icon extends View {
        private final String name;private final int color;private final Paint p=new Paint(Paint.ANTI_ALIAS_FLAG);
        Icon(Context c,String name,int color){super(c);this.name=name;this.color=color;setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);}
        @Override protected void onDraw(Canvas canvas){super.onDraw(canvas);canvas.save();float scale=Math.min(getWidth(),getHeight())/24f;canvas.translate((getWidth()-24*scale)/2,(getHeight()-24*scale)/2);canvas.scale(scale,scale);p.setColor(color);p.setStyle(Paint.Style.STROKE);p.setStrokeWidth(1.7f);p.setStrokeCap(Paint.Cap.ROUND);p.setStrokeJoin(Paint.Join.ROUND);
            Path path=new Path();
            switch(name){
                case "chat":path.moveTo(5,4);path.lineTo(19,4);path.quadTo(21,4,21,6);path.lineTo(21,15);path.quadTo(21,17,19,17);path.lineTo(10,17);path.lineTo(5,21);path.lineTo(5,17);path.quadTo(3,17,3,15);path.lineTo(3,6);path.quadTo(3,4,5,4);canvas.drawPath(path,p);canvas.drawLine(7,9,17,9,p);canvas.drawLine(7,13,14,13,p);break;
                case "users":canvas.drawCircle(9,8,3,p);path.moveTo(3,20);path.lineTo(3,18);path.cubicTo(3,12,15,12,15,18);path.lineTo(15,20);canvas.drawPath(path,p);canvas.drawArc(new RectF(13,5,21,11),-90,160,false,p);canvas.drawArc(new RectF(14,14,22,22),-90,90,false,p);break;
                case "person":canvas.drawCircle(12,8,3.5f,p);path.moveTo(4,21);path.lineTo(4,19);path.cubicTo(4,12,20,12,20,19);path.lineTo(20,21);canvas.drawPath(path,p);break;
                case "compass":canvas.drawCircle(12,12,9,p);path.moveTo(16,8);path.lineTo(14,14);path.lineTo(8,16);path.lineTo(10,10);path.close();canvas.drawPath(path,p);break;
                case "heart":path.moveTo(12,20);path.cubicTo(0,12,2,3,8,5);path.quadTo(11,5,12,8);path.quadTo(14,4,18,5);path.cubicTo(25,8,18,17,12,20);canvas.drawPath(path,p);break;
                case "mail":canvas.drawRoundRect(new RectF(3,5,21,19),3,3,p);path.moveTo(3,6);path.lineTo(12,13);path.lineTo(21,6);canvas.drawPath(path,p);break;
                case "send":path.moveTo(3,4);path.lineTo(22,12);path.lineTo(3,20);path.lineTo(6,12);path.close();canvas.drawPath(path,p);canvas.drawLine(6,12,16,12,p);break;
                case "globe":canvas.drawCircle(12,12,9,p);canvas.drawOval(new RectF(8,3,16,21),p);canvas.drawLine(3,12,21,12,p);break;
                case "plus":canvas.drawLine(12,5,12,19,p);canvas.drawLine(5,12,19,12,p);break;
                case "back":path.moveTo(15,5);path.lineTo(8,12);path.lineTo(15,19);canvas.drawPath(path,p);break;
                case "search":canvas.drawCircle(10,10,6,p);canvas.drawLine(15,15,21,21,p);break;
                case "refresh":canvas.drawArc(new RectF(4,4,20,20),-40,290,false,p);path.moveTo(20,4);path.lineTo(20,10);path.lineTo(14,10);canvas.drawPath(path,p);break;
                default:path.moveTo(3,13);path.cubicTo(6,5,10,5,12,12);path.cubicTo(15,20,18,20,21,11);canvas.drawPath(path,p);canvas.drawLine(5,19,19,5,p);break;
            }canvas.restore();
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
