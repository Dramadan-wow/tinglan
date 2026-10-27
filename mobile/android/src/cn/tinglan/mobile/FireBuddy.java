package cn.tinglan.mobile;

import android.content.Context;
import android.graphics.*;
import android.view.View;
import org.json.JSONObject;
import java.text.SimpleDateFormat;
import java.util.*;

/** The friendship flame of desktop gui/FireBuddyHsx: the same 48×58 drawing, skins, outfits and streak rules. */
final class FireBuddy extends View {
    static final class Style {
        final String id,name;final int tier,a,b,ink;
        Style(String id,String name,int tier,int a,int b,int ink){this.id=id;this.name=name;this.tier=tier;this.a=a;this.b=b;this.ink=ink;}
    }
    /** Catalogs from workspace/AppearanceHsx.cpp ("pet" and "outfit"). */
    static final Style[] PETS={new Style("flame","暖橙小火人",0,0xffff963b,0xffffd56e,0xff603d29),new Style("mint","薄荷精灵",1,0xff52c7a8,0xffbcf2bb,0xff325b4f),
        new Style("pink","樱粉小火人",2,0xffeb80ac,0xffffd0e8,0xff6b3e57),new Style("violet","星焰精灵",3,0xff9b81ed,0xffd6c8ff,0xff4c3e75),new Style("gold","鎏金守护者",4,0xffe7ad3d,0xfffff3a9,0xff694918)};
    static final Style[] OUTFITS={new Style("none","素颜火苗",0,0xffffffff,0xffffffff,0xffffffff),new Style("scarf","暖冬围巾",0,0xffe2574c,0xffffd9c7,0xffb8382f),new Style("bow","蝴蝶结",0,0xfff27aa8,0xffffd0e2,0xffc9467b),
        new Style("straw","夏日草帽",1,0xfff0cf86,0xffd7a24e,0xffe2574c),new Style("glasses","圆框眼镜",1,0xff33405a,0xffbfe3ff,0xff33405a),new Style("headphones","音乐耳机",2,0xff7a86e8,0xffdfe3ff,0xff4c58b8),new Style("crown","星河王冠",3,0xfff6c443,0xfffff2b3,0xffd65a7a)};
    static final String[] TIERS={"免费","星芽","月汐","星河","云境"};
    static Style find(Style[] all,String id){for(Style s:all)if(s.id.equals(id))return s;return all[0];}

    /** Streak exactly as StudioStoreHsx::streak: a day counts when both sides sent a private message on it. */
    static int[] streak(Workspace w,int service,int other){
        Map<String,Integer> days=new HashMap<>();SimpleDateFormat day=new SimpleDateFormat("yyyyMMdd",Locale.ROOT);String today=day.format(new Date());
        for(JSONObject m:w.history(service,0,other)){if(m.optBoolean("recalled"))continue;String d=day.format(new Date(m.optLong("created")*1000));if(d.compareTo(today)>0)continue;
            Integer bits=days.get(d);days.put(d,(bits==null?0:bits)|(m.optInt("sender")==w.viewer?1:2));}
        Calendar c=Calendar.getInstance();Integer now=days.get(today);if(now==null||now!=3)c.add(Calendar.DATE,-1);int count=0;
        while(true){Integer bits=days.get(day.format(c.getTime()));if(bits==null||bits!=3)break;count++;c.add(Calendar.DATE,-1);}
        return new int[]{count,Math.min(5,1+count/7),now==null?0:now};
    }
    static String status(int today){return today==3?"今日已点亮":today==1?"今天还差对方的一条消息":today==2?"今天还差你的一条消息":"双方今天各发一条消息，即可点亮";}

    private Style skin=PETS[0],outfit=OUTFITS[0];
    private int days,level=1,today;
    private boolean active=true;
    private final boolean swatch;
    private float phase;
    private final Paint p=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path path=new Path();
    private final Runnable tick=new Runnable(){public void run(){phase+=.15f;invalidate();postDelayed(this,80);}};
    /** swatch: a still wardrobe sample; otherwise the bobbing buddy with its level badge. */
    FireBuddy(Context context,boolean swatch){super(context);this.swatch=swatch;setImportantForAccessibility(swatch?IMPORTANT_FOR_ACCESSIBILITY_NO:IMPORTANT_FOR_ACCESSIBILITY_YES);}
    void setLook(Style skin,Style outfit){this.skin=skin;this.outfit=outfit;invalidate();}
    void setState(int[] streak){days=streak[0];level=streak[1];today=streak[2];active=days>0;
        setContentDescription(active?"小火人，连续聊天 "+days+" 天，等级 "+level+"。轻点打招呼，长按换装":"小火人，"+status(today)+"。轻点打招呼，长按换装");invalidate();}
    int days(){return days;}
    int level(){return level;}
    int today(){return today;}
    @Override protected void onAttachedToWindow(){super.onAttachedToWindow();if(!swatch)post(tick);}
    @Override protected void onDetachedFromWindow(){removeCallbacks(tick);super.onDetachedFromWindow();}
    @Override protected void onDraw(Canvas canvas){
        super.onDraw(canvas);float w=getWidth(),h=getHeight();
        if(swatch){draw(canvas,new RectF(0,0,w,h),true);return;}
        float d=getResources().getDisplayMetrics().density,bob=(float)Math.sin(phase)*1.2f*d;boolean lit=active;
        p.setShader(null);p.setStyle(Paint.Style.FILL);p.setColor(Color.argb(lit?38:26,40,80,140));float shadow=(40-bob/d*2)*d;canvas.drawOval(new RectF(w/2-shadow/2,h-13.5f*d,w/2+shadow/2,h-4.5f*d),p);
        draw(canvas,new RectF(12*d,8*d+bob,w-12*d,h-14*d+bob),lit);
        RectF pill=new RectF(w-34*d,6*d,w-4*d,21*d);p.setColor(lit?0xffff8a3d:0xffc3cbd8);canvas.drawRoundRect(pill,7.5f*d,7.5f*d,p);
        p.setColor(Color.WHITE);p.setTextSize(9*d);p.setTypeface(Typeface.DEFAULT_BOLD);p.setTextAlign(Paint.Align.CENTER);Paint.FontMetrics m=p.getFontMetrics();
        canvas.drawText(lit?"Lv."+level:"zZ",pill.centerX(),pill.centerY()-(m.ascent+m.descent)/2,p);p.setTextAlign(Paint.Align.LEFT);
    }
    private void draw(Canvas c,RectF box,boolean lit){
        c.save();float s=Math.min(box.width()/48,box.height()/58);c.translate(box.centerX()-24*s,box.centerY()-29*s);c.scale(s,s);p.setStyle(Paint.Style.FILL);
        path.reset();path.moveTo(24,1);path.cubicTo(29,16,43,17,43,32);path.cubicTo(44,55,3,57,4,32);path.cubicTo(5,23,9,18,14,16);path.cubicTo(11,27,20,23,24,1);path.close();
        p.setShader(new LinearGradient(0,0,0,52,lit?skin.a:0xffaab5c6,lit?skin.b:0xffd9e0eb,Shader.TileMode.CLAMP));c.drawPath(path,p);p.setShader(null);
        p.setColor(Color.argb(115,255,255,255));c.drawOval(new RectF(10,27,37,49),p);
        p.setStyle(Paint.Style.STROKE);p.setColor(0xff6b4d43);p.setStrokeWidth(2);p.setStrokeCap(Paint.Cap.ROUND);
        boolean blink=!swatch&&phase%6>5.7f;if(blink){c.drawLine(16,34,19,34,p);c.drawLine(29,34,32,34,p);}else{c.drawPoint(18,34,p);c.drawPoint(30,34,p);}
        c.drawArc(new RectF(21,35,28,41),0,180,false,p);p.setStyle(Paint.Style.FILL);
        int a=outfit.a,b=outfit.b,ink=outfit.ink;
        switch(outfit.id){
            case "scarf":{Path band=new Path();band.addRoundRect(new RectF(7,42,41,49),3.5f,3.5f,Path.Direction.CW);p.setColor(a);c.drawPath(band,p);c.save();c.clipPath(band);p.setColor(b);for(int i=0;i<5;i++)c.drawRect(9+i*7,42,11.6f+i*7,49,p);c.restore();
                p.setColor(a);c.drawRoundRect(new RectF(29,46,36,58),3,3,p);p.setColor(ink);c.drawRect(29.5f,54.5f,35.5f,56.1f,p);break;}
            case "bow":{path.reset();path.moveTo(33,15);path.cubicTo(27,9,24,12,25,16);path.cubicTo(25,20,29,20,33,15);path.moveTo(33,15);path.cubicTo(39,9,42,12,41,16);path.cubicTo(41,20,37,20,33,15);
                p.setColor(a);c.drawPath(path,p);p.setColor(b);c.drawCircle(27.5f,14.8f,1.4f,p);c.drawCircle(38.5f,14.8f,1.4f,p);p.setColor(ink);c.drawCircle(33,15,2.2f,p);break;}
            case "straw":{p.setColor(b);c.drawOval(new RectF(5,14,43,21.5f),p);p.setColor(a);c.drawRoundRect(new RectF(14.5f,6.5f,33.5f,17.5f),5,5,p);p.setColor(ink);c.drawRect(14.5f,13,33.5f,15.6f,p);
                p.setColor(UiKit.blend(a,Color.WHITE,.06f));c.drawOval(new RectF(9,16.5f,39,20.1f),p);break;}
            case "glasses":{p.setColor(Color.argb(110,Color.red(b),Color.green(b),Color.blue(b)));c.drawCircle(18,34,4.6f,p);c.drawCircle(30,34,4.6f,p);
                p.setStyle(Paint.Style.STROKE);p.setStrokeWidth(1.6f);p.setColor(a);c.drawCircle(18,34,4.6f,p);c.drawCircle(30,34,4.6f,p);c.drawLine(22.6f,33.2f,25.4f,33.2f,p);c.drawLine(13.4f,33,10.2f,31.4f,p);c.drawLine(34.6f,33,37.8f,31.4f,p);p.setStyle(Paint.Style.FILL);break;}
            case "headphones":{p.setStyle(Paint.Style.STROKE);p.setStrokeWidth(3);p.setColor(a);c.drawArc(new RectF(6,10,42,50),-160,140,false,p);p.setStyle(Paint.Style.FILL);
                c.drawRoundRect(new RectF(2.5f,25,10.5f,38),3.5f,3.5f,p);c.drawRoundRect(new RectF(37.5f,25,45.5f,38),3.5f,3.5f,p);p.setColor(b);c.drawRoundRect(new RectF(4.5f,27.5f,8.5f,35.5f),2,2,p);c.drawRoundRect(new RectF(39.5f,27.5f,43.5f,35.5f),2,2,p);
                p.setColor(ink);c.drawCircle(6.5f,37.5f,1.2f,p);break;}
            case "crown":{path.reset();path.moveTo(15,13);path.lineTo(14,4);path.lineTo(19.5f,8.5f);path.lineTo(24,1.5f);path.lineTo(28.5f,8.5f);path.lineTo(34,4);path.lineTo(33,13);path.close();
                p.setShader(new LinearGradient(0,1,0,13,b,a,Shader.TileMode.CLAMP));c.drawPath(path,p);p.setShader(null);p.setStyle(Paint.Style.STROKE);p.setStrokeWidth(.8f);p.setColor(UiKit.blend(a,Color.BLACK,.13f));c.drawPath(path,p);p.setStyle(Paint.Style.FILL);
                p.setColor(ink);c.drawCircle(24,9.5f,1.6f,p);p.setColor(0xff7bc6ff);c.drawCircle(18.5f,10.5f,1.1f,p);c.drawCircle(29.5f,10.5f,1.1f,p);break;}
            default:break;
        }
        c.restore();
    }
}
