package cn.tinglan.mobile;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputType;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.*;
import org.json.JSONArray;
import org.json.JSONObject;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.*;

/** Native Android client. Account authority and all mutations remain on the existing server. */
public final class MainActivity extends Activity {
    // Pages follow the desktop sidebar: 会话, 好友, 群组, 通知, 朋友圈 and 账号. Weibo keeps FEED and MINE.
    private static final int CHATS=0,FRIENDS=1,FEED=2,MINE=3,GROUPS=4,NOTICES=5,MEMBER=6;
    private int selectedTier=1;
    private static final String[] EMOJI={"😊","🎉","❤️","👍","🌷","🌙","✨","🌈"};
    private int BG,INK,GREEN,MUTED,LINE;
    private UiKit.Palette palette=UiKit.RIVER;
    private int weiboFilter=0;
    private boolean weiboInbox=false;
    private String directPost="";
    private android.widget.FrameLayout pageFrame;
    private static final String[] SERVICES={"QQ","微信","微博"};
    private SharedPreferences prefs;
    private ConnectionProfile profile;
    private ChatClient client;
    private Workspace workspace;
    private LinearLayout outer,root,body,chatMessages,conversationPane;
    private ScrollView scroll;
    private TextView status;
    private EditText nickname,password,composer;
    private Button loginButton;
    private View sendButton;
    private int service=0,tab=0,chatKind=-1,chatTarget=0;
    private String connectionState="未连接",channel="moments",conversationQuery="";
    private boolean connecting=false,register=false,sending=false,destroyed=false,flush=false;
    private final Map<String,Runnable> successes=new HashMap<>();
    private final Map<String,Runnable> completions=new HashMap<>();
    private final Map<String,Bitmap> faces=new HashMap<>();
    private String sendRequest="";
    private String lastReadBatch="";
    private static final int SCAN=11;
    private CheckBox autoSign;
    private boolean autoSigning=false;
    private String scannedCode="";
    // The private-chat fire buddy floats over the messages; its dragged position survives re-renders.
    private FireBuddy buddy;
    private TextView buddyBubble;
    private float buddyX,buddyY;
    @Override public void onCreate(Bundle saved){super.onCreate(saved);prefs=getSharedPreferences("connection",MODE_PRIVATE);cleanUpdates();service=Math.max(0,Math.min(2,prefs.getInt("service",0)));try{String code=prefs.getString("code","");if(!code.isEmpty())profile=ConnectionProfile.parse(code);}catch(Exception e){profile=null;}
        if(profile!=null&&!prefs.getString("token","").isEmpty())autoLogin();else showLogin();}
    @Override protected void onDestroy(){destroyed=true;if(client!=null)client.close();super.onDestroy();}
    @Override public void onConfigurationChanged(android.content.res.Configuration configuration){
        super.onConfigurationChanged(configuration);
        if(workspace==null){String nick=nickname==null?prefs.getString("nickname",""):nickname.getText().toString();String secret=password==null?"":password.getText().toString();showLogin();nickname.setText(nick);password.setText(secret);}
        else if(chatKind>=0){int kind=chatKind,target=chatTarget;String draft=composer==null?"":composer.getText().toString();if(kind==2)openDirectMessage(target,directPost);else openChat(kind,target);if(composer!=null)composer.setText(draft);}
        else showHome();
    }
    @Override public void onBackPressed(){if(chatKind>=0)closeChat();else if(workspace!=null&&tab==MEMBER){chatKind=-1;tab=MINE;showHome();}else if(workspace!=null&&service!=2&&tab!=CHATS)go(CHATS);else if(workspace!=null)new AlertDialog.Builder(this).setTitle("离开听澜？").setMessage("聊天记录保存在服务器，应用回到后台后可能暂停连接。").setPositiveButton("回到桌面",(d,w)->moveTaskToBack(true)).setNegativeButton("取消",null).show();else super.onBackPressed();}
    private float density(){return getResources().getDisplayMetrics().density;}
    private int dp(float value){return (int)(value*density()+.5f);}
    private boolean wide(){return getResources().getConfiguration().screenWidthDp>=720;}
    /** Conversation list beside the chat only when both fit: sidebar 212 + list 316 + at least ~430 dp of chat. */
    private boolean splitView(){return getResources().getConfiguration().screenWidthDp>=960;}
    private GradientDrawable bg(int color,float radius){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(radius));return d;}
    private GradientDrawable outline(int color,float radius){GradientDrawable d=bg(color,radius);d.setStroke(dp(1),LINE);return d;}
    private LinearLayout column(){LinearLayout v=new LinearLayout(this);v.setOrientation(LinearLayout.VERTICAL);return v;}
    private LinearLayout row(){LinearLayout v=new LinearLayout(this);v.setOrientation(LinearLayout.HORIZONTAL);v.setGravity(Gravity.CENTER_VERTICAL);return v;}
    private TextView text(String value,int size,int color,boolean bold){TextView t=new TextView(this);t.setText(value);t.setTextSize(size);t.setTextColor(color);if(bold)t.setTypeface(Typeface.DEFAULT,Typeface.BOLD);t.setLineSpacing(dp(3),1);return t;}
    private TextView line(String value,int size,int color,boolean bold){TextView t=text(value,size,color,bold);t.setSingleLine(true);t.setEllipsize(TextUtils.TruncateAt.END);return t;}
    private void gap(LinearLayout v,int height){Space s=new Space(this);v.addView(s,new LinearLayout.LayoutParams(1,dp(height)));}
    private Button button(String title,boolean primary,Runnable action){
        Button b=new Button(this);b.setAllCaps(false);b.setText(title);b.setTextSize(14);b.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));
        b.setTextColor(primary?palette.onAccent:blog()?0xff984570:GREEN);b.setBackground(UiKit.touch(primary?GREEN:palette.soft,GREEN,14,density()));
        b.setStateListAnimator(null);b.setMinimumWidth(0);b.setMinWidth(0);b.setMinimumHeight(0);b.setMinHeight(dp(44));b.setPadding(dp(13),dp(7),dp(13),dp(7));b.setOnClickListener(v->action.run());return b;
    }
    private boolean blog(){return service==2||(workspace!=null&&tab==FEED&&channel.equals("weibo"));}
    private void applyPalette(){
        // Like the desktop client: mountain-and-river login, light blue signed-in workspace, Yvonne chrome for Weibo.
        palette=blog()?UiKit.YVONNE:workspace==null?UiKit.RIVER:UiKit.SKY;BG=palette.background;INK=palette.ink;GREEN=palette.accent;MUTED=palette.muted;LINE=palette.line;
        getTheme().applyStyle(blog()?R.style.YvonneTheme:workspace==null?R.style.AppTheme:R.style.SkyTheme,true);
        getWindow().setStatusBarColor(BG);getWindow().setNavigationBarColor(BG);
    }
    private View icon(String name,int color,int size){UiKit.Icon view=new UiKit.Icon(this,name,color);view.setLayoutParams(new LinearLayout.LayoutParams(dp(size),dp(size)));return view;}
    private FrameLayout iconAction(String name,String label,Runnable tap){
        FrameLayout hit=new FrameLayout(this);hit.setContentDescription(label);hit.setBackground(UiKit.touch(palette.surface,GREEN,16,density()));
        UiKit.Icon image=new UiKit.Icon(this,name,INK);FrameLayout.LayoutParams imageSize=new FrameLayout.LayoutParams(dp(21),dp(21),Gravity.CENTER);hit.addView(image,imageSize);hit.setOnClickListener(v->tap.run());LinearLayout.LayoutParams size=new LinearLayout.LayoutParams(dp(46),dp(46));size.leftMargin=dp(6);hit.setLayoutParams(size);return hit;
    }
    private TextView pill(String label,int fill,int color){TextView t=text(label,11,color,true);t.setPadding(dp(10),dp(4),dp(10),dp(4));t.setBackground(bg(fill,20));return t;}
    private TextView badge(int count){TextView b=text(count>99?"99+":String.valueOf(count),9,Color.WHITE,true);b.setLineSpacing(0,1);b.setIncludeFontPadding(false);b.setGravity(Gravity.CENTER);b.setMinWidth(dp(17));b.setPadding(dp(5),0,dp(5),0);b.setBackground(bg(UiKit.BADGE,9));return b;}
    private View dot(int color){View d=new View(this);d.setBackground(bg(color,4));d.setLayoutParams(new LinearLayout.LayoutParams(dp(7),dp(7)));return d;}
    private void serviceStrip(LinearLayout parent,boolean login){
        LinearLayout strip=row();strip.setPadding(dp(4),dp(4),dp(4),dp(4));strip.setBackground(bg(login||blog()?palette.soft:0xffe8f1fc,17));
        for(int i=0;i<3;i++){final int selected=i;TextView item=text(SERVICES[i],13,service==i?INK:MUTED,service==i);item.setGravity(Gravity.CENTER);item.setBackground(UiKit.touch(service==i?palette.surface:Color.TRANSPARENT,GREEN,13,density()));item.setContentDescription(SERVICES[i]+(service==i?"，当前服务":""));LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(0,dp(37),1);strip.addView(item,lp);
            item.setOnClickListener(v->{if(connecting)return;if(login){if(nickname!=null)prefs.edit().putString("nickname",nickname.getText().toString().trim()).apply();service=selected;prefs.edit().putInt("service",service).apply();showLogin();}
                else if(workspace.account(workspace.viewer,selected).length()>0){service=selected;weiboInbox=false;weiboFilter=0;channel=selected==2?"weibo":"moments";tab=selected==2?FEED:CHATS;prefs.edit().putInt("service",service).apply();showHome();}else toast("当前账号尚未开通"+SERVICES[selected]+"，可在「我的」开通微博");});
        }parent.addView(strip,new LinearLayout.LayoutParams(-1,dp(45)));
    }
    private EditText input(String hint,boolean secret){
        EditText e=new EditText(this);e.setHint(hint);e.setTextSize(15);e.setSingleLine(true);e.setTextColor(INK);e.setHintTextColor(MUTED);e.setHighlightColor(palette.soft);e.setPadding(dp(15),dp(13),dp(15),dp(13));e.setBackground(outline(BG,14));
        e.setInputType(InputType.TYPE_CLASS_TEXT|(secret?InputType.TYPE_TEXT_VARIATION_PASSWORD:InputType.TYPE_TEXT_FLAG_CAP_SENTENCES));e.setSaveEnabled(false);e.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO);return e;
    }
    private void field(LinearLayout parent,String label,EditText e){parent.addView(text(label,13,MUTED,false));gap(parent,8);parent.addView(e,new LinearLayout.LayoutParams(-1,dp(54)));gap(parent,18);}
    private LinearLayout card(LinearLayout parent){
        LinearLayout c=column();c.setPadding(dp(19),dp(18),dp(19),dp(18));c.setBackground(outline(palette.surface,workspace==null?22:16));c.setElevation(dp(.5f));LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);lp.bottomMargin=dp(12);parent.addView(c,lp);return c;
    }
    private void section(LinearLayout parent,String title,int count){TextView t=text(count>=0?title+" · "+count:title,13,MUTED,true);t.setPadding(dp(4),dp(4),0,dp(8));parent.addView(t);}
    private interface TextAction{void accept(String value);}
    private TextWatcher watcher(TextAction change){return new TextWatcher(){public void beforeTextChanged(CharSequence s,int a,int b,int c){}public void onTextChanged(CharSequence s,int a,int b,int c){}public void afterTextChanged(Editable s){change.accept(s.toString().trim());}};}
    private EditText searchBox(LinearLayout parent,String hint){
        LinearLayout box=row();box.setPadding(dp(12),0,dp(6),0);box.setBackground(bg(blog()?palette.soft:0xffe9f1fb,14));box.addView(icon("search",MUTED,18));
        EditText e=input(hint,false);e.setBackground(null);e.setTextSize(14);e.setPadding(dp(9),0,0,0);box.addView(e,new LinearLayout.LayoutParams(0,-1,1));parent.addView(box,new LinearLayout.LayoutParams(-1,dp(42)));return e;
    }
    /** Phone pages fill a centered column; signed-in tablets add the desktop sidebar beside the page. */
    private void base(){base(false);}
    private void base(boolean flat){
        applyPalette();flush=flat;pageFrame=new FrameLayout(this);pageFrame.setBackgroundColor(BG);final boolean shell=workspace!=null&&wide();
        outer=shell?row():column();outer.setGravity(Gravity.TOP);root=outer;
        int available=getResources().getDisplayMetrics().widthPixels;FrameLayout.LayoutParams fit=new FrameLayout.LayoutParams(shell?-1:Math.min(available,dp(workspace==null?940:860)),-1,Gravity.TOP|Gravity.CENTER_HORIZONTAL);pageFrame.addView(outer,fit);setContentView(pageFrame);
        if(shell){sidebar(outer);root=column();outer.addView(root,new LinearLayout.LayoutParams(0,-1,1));}
        pageFrame.setOnApplyWindowInsetsListener((v,insets)->{int top=insets.getSystemWindowInsetTop(),bottom=insets.getSystemWindowInsetBottom();
            if(shell){outer.setPadding(0,top,0,bottom);root.setPadding(flush?0:dp(28),flush?0:dp(20),flush?0:dp(28),flush?0:dp(12));}
            else root.setPadding(dp(20),dp(10)+top,dp(20),dp(8)+bottom);return insets.consumeSystemWindowInsets();});
    }
    private void makeScroll(){scroll=new ScrollView(this);scroll.setFillViewport(true);scroll.setClipToPadding(false);scroll.setVerticalScrollBarEnabled(false);body=column();body.setPadding(0,dp(6),0,dp(18));scroll.addView(body,new ScrollView.LayoutParams(-1,-2));root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));}
    private void top(String title,String sub,String action,Runnable tap){
        LinearLayout h=row();h.setPadding(0,dp(4),0,dp(4));LinearLayout labels=column();TextView small=text(blog()?"YVONNE / TINGLAN":"TINGLAN / STAY CONNECTED",9,MUTED,true);small.setLetterSpacing(.13f);labels.addView(small);labels.addView(text(title,27,INK,true));
        if(sub!=null&&!sub.isEmpty())labels.addView(text(sub,11,MUTED,false));h.addView(labels,new LinearLayout.LayoutParams(0,-2,1));
        if(action!=null){String glyph=action.equals("返回")?"back":action.equals("添加")?"plus":action.contains("连接")||action.equals("服务器")?"globe":action.equals("私信")?"mail":"refresh";h.addView(iconAction(glyph,action,tap));}root.addView(h);gap(root,12);
    }
    private boolean online(){return client!=null&&client.isOnline();}
    private void connectionLabel(){
        if(workspace!=null&&wide())return;LinearLayout line=row();line.setPadding(dp(3),0,0,dp(7));line.addView(dot(online()?UiKit.ONLINE:MUTED));
        status=text(connectionState,11,MUTED,false);status.setPadding(dp(6),0,0,0);line.addView(status);root.addView(line);
    }
    private static final class Nav{
        final String label,glyph,category;final int badge;final boolean active;final Runnable go;
        Nav(String label,String glyph,int badge,boolean active,String category,Runnable go){this.label=label;this.glyph=glyph;this.badge=badge;this.active=active;this.category=category;this.go=go;}
    }
    private List<Nav> navEntries(boolean side){
        List<Nav> out=new ArrayList<>();
        if(service==2){int current=tab==MINE||tab==MEMBER?3:weiboInbox?2:weiboFilter==1?1:0;String[] labels=side?new String[]{"微博首页","我的关注","微博私信","我的空间"}:new String[]{"广场","关注","私信","我的"};String[] glyphs={"compass","heart","mail","person"};
            for(int i=0;i<4;i++){final int index=i;out.add(new Nav(labels[i],glyphs[i],0,current==i,side&&i==3?"频道与个人空间":null,()->{hideKeyboard();chatKind=-1;tab=index==3?MINE:FEED;channel="weibo";weiboInbox=index==2;weiboFilter=index==1?1:0;showHome();}));}
            return out;
        }
        int notices=noticeCount();
        out.add(new Nav("会话","chat",unreadTotal(),tab==CHATS,null,()->go(CHATS)));
        out.add(new Nav("好友","users",side?0:notices,tab==FRIENDS||(!side&&tab==NOTICES),null,()->go(FRIENDS)));
        out.add(new Nav("群组","group",0,tab==GROUPS,null,()->go(GROUPS)));
        if(side)out.add(new Nav("通知","bell",notices,tab==NOTICES,null,()->go(NOTICES)));
        out.add(new Nav("朋友圈","compass",0,tab==FEED,side?"我的听澜":null,()->{channel="moments";go(FEED);}));
        out.add(new Nav(side?"账号":"我的","person",0,tab==MINE||tab==MEMBER,null,()->go(MINE)));
        return out;
    }
    private void go(int page){hideKeyboard();chatKind=-1;tab=page;weiboInbox=false;showHome();}
    private void navigation(){
        LinearLayout nav=row();nav.setPadding(dp(4),dp(6),dp(4),dp(4));nav.setBackground(outline(palette.surface,22));
        for(Nav n:navEntries(false)){LinearLayout item=column();item.setGravity(Gravity.CENTER);item.setContentDescription(n.label+(n.badge>0?"，"+n.badge+" 条新内容":""));
            FrameLayout capsule=new FrameLayout(this);capsule.setBackground(bg(n.active?palette.soft:Color.TRANSPARENT,14));UiKit.Icon glyph=new UiKit.Icon(this,n.glyph,n.active?GREEN:MUTED);capsule.addView(glyph,new FrameLayout.LayoutParams(dp(22),dp(22),Gravity.CENTER));
            if(n.badge>0)capsule.addView(badge(n.badge),new FrameLayout.LayoutParams(-2,dp(16),Gravity.TOP|Gravity.END));
            item.addView(capsule,new LinearLayout.LayoutParams(dp(54),dp(31)));TextView label=text(n.label,10,n.active?INK:MUTED,n.active);label.setGravity(Gravity.CENTER);item.addView(label);
            nav.addView(item,new LinearLayout.LayoutParams(0,dp(55),1));item.setOnClickListener(v->n.go.run());
        }LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,dp(65));lp.topMargin=dp(4);root.addView(nav,lp);
    }
    /** Tablet counterpart of the desktop MainWindowHsx sidebar. */
    private void sidebar(LinearLayout parent){
        boolean wb=blog();LinearLayout side=column();side.setPadding(dp(12),dp(20),dp(12),dp(14));
        side.setBackground(wb?bg(0xff251f30,0):new GradientDrawable(GradientDrawable.Orientation.TL_BR,new int[]{0xfff6fbff,0xffe4f0ff}));
        LinearLayout brand=row();brand.setPadding(dp(10),0,0,0);brand.addView(icon("wave",wb?0xffed8bbb:GREEN,26));TextView name=text(wb?"绚色微博":"听澜",wb?21:25,wb?0xffffd5e9:0xff163e70,true);name.setPadding(dp(8),0,0,0);brand.addView(name);side.addView(brand);
        TextView mark=text(wb?"YVONNE / WEIBO":"T I N G L A N",8,wb?0xffb99bb8:0xff92acce,false);mark.setLetterSpacing(.2f);mark.setPadding(dp(46),dp(2),0,0);side.addView(mark);gap(side,16);
        LinearLayout me=row();me.setPadding(dp(8),dp(8),dp(6),dp(8));me.setBackground(UiKit.touch(wb?0xff342a40:0x80ffffff,GREEN,14,density()));me.addView(avatarUser(workspace.viewer,44),new LinearLayout.LayoutParams(dp(44),dp(44)));
        LinearLayout who=column();who.setPadding(dp(10),0,0,0);who.addView(line(workspace.name(workspace.viewer),14,wb?0xfff5d9eb:0xff233b5d,true));LinearLayout state=row();state.addView(dot(online()?UiKit.ONLINE:MUTED));status=line(connectionState,10,wb?0xffcdb3cc:MUTED,false);status.setPadding(dp(5),0,0,0);state.addView(status,new LinearLayout.LayoutParams(0,-2,1));who.addView(state);
        me.addView(who,new LinearLayout.LayoutParams(0,-2,1));me.setContentDescription("我的资料");me.setOnClickListener(v->{if(service==2){chatKind=-1;tab=MINE;showHome();}else go(MINE);});side.addView(me);gap(side,14);
        for(Nav n:navEntries(true)){if(n.category!=null){gap(side,10);TextView c=text(n.category,9,wb?0xffb99bb8:0xff95a6be,false);c.setLetterSpacing(.08f);c.setPadding(dp(14),0,0,dp(6));side.addView(c);}side.addView(sideItem(n,wb),new LinearLayout.LayoutParams(-1,dp(46)));gap(side,3);}
        side.addView(new View(this),new LinearLayout.LayoutParams(1,0,1));
        TextView label=text("当前服务",10,wb?0xffb99bb8:MUTED,false);label.setPadding(dp(6),0,0,dp(6));side.addView(label);serviceStrip(side,false);gap(side,14);
        TextView quote=text(wb?"灵感，正在连线。":"山水有相逢。",12,wb?0xffb99bb8:0xff8ba5c6,false);quote.setTypeface(Typeface.SERIF);quote.setPadding(dp(10),0,0,0);side.addView(quote);
        parent.addView(side,new LinearLayout.LayoutParams(dp(212),-1));
    }
    private View sideItem(Nav n,boolean wb){
        int on=wb?0xffffe5f1:GREEN,off=wb?0xffcdb3cc:0xff526d91;LinearLayout item=row();item.setPadding(dp(12),0,dp(10),0);
        item.setBackground(UiKit.touch(n.active?(wb?0xff4a3553:0xffd5eaff):Color.TRANSPARENT,on,11,density()));item.addView(icon(n.glyph,n.active?on:(wb?0xffb99bb8:0xff6580a6),20));
        TextView label=text(n.label,14,n.active?on:off,n.active);label.setPadding(dp(12),0,0,0);item.addView(label,new LinearLayout.LayoutParams(0,-2,1));
        if(n.badge>0)item.addView(badge(n.badge),new LinearLayout.LayoutParams(-2,dp(18)));item.setContentDescription(n.label);item.setOnClickListener(v->n.go.run());return item;
    }
    private View artwork(boolean yvonne,int height){UiKit.Artwork view=new UiKit.Artwork(this,yvonne);view.setLayoutParams(new LinearLayout.LayoutParams(-1,dp(height)));return view;}

    private void showLogin(){
        base();top("听澜",null,"服务器",this::connectionDialog);makeScroll();boolean wide=getResources().getDisplayMetrics().widthPixels/getResources().getDisplayMetrics().density>=650;
        LinearLayout host=wide?row():column();if(wide)host.setGravity(Gravity.CENTER);body.addView(host,new LinearLayout.LayoutParams(-1,wide?dp(590):-2));
        LinearLayout art=column();art.addView(artwork(service==2,wide?430:202));if(wide){gap(art,18);art.addView(text(service==2?"今天的灵感，值得被看见。":"与你同频，也与你同在。",19,INK,true));}
        LinearLayout.LayoutParams artSize=new LinearLayout.LayoutParams(wide?0:-1,-2,wide?1:0);if(wide)artSize.rightMargin=dp(28);host.addView(art,artSize);
        LinearLayout form=column();if(!wide)gap(form,18);LinearLayout card=card(form);card.addView(text(register?"开启新的相遇":"欢迎回来",23,INK,true));gap(card,5);card.addView(text(service==2?"伊冯灵感 · 记录属于你的频率":"选择服务，继续你的云端会话。",12,MUTED,false));gap(card,20);serviceStrip(card,true);gap(card,21);
        nickname=input("注册时的昵称",false);nickname.setText(prefs.getString("nickname",""));password=input(register?"设置至少 12 字节的密码":"输入账号密码",true);field(card,"昵称",nickname);field(card,"密码",password);
        autoSign=new CheckBox(this);autoSign.setText("下次打开自动登录");autoSign.setTextSize(13);autoSign.setTextColor(MUTED);autoSign.setButtonTintList(android.content.res.ColorStateList.valueOf(GREEN));autoSign.setChecked(prefs.getBoolean("autoLogin",true));card.addView(autoSign);gap(card,10);
        loginButton=button(autoSigning?"取消自动登录":register?"创建账号并登录":"进入听澜  →",!autoSigning,autoSigning?this::cancelAutoLogin:this::login);card.addView(loginButton,new LinearLayout.LayoutParams(-1,dp(50)));gap(card,10);
        TextView mode=text(register?"已有账号？  返回登录":"还没有账号？  创建账号",13,GREEN,true);mode.setGravity(Gravity.CENTER);mode.setPadding(0,dp(10),0,dp(6));mode.setOnClickListener(v->{if(!connecting){prefs.edit().putString("nickname",nickname.getText().toString().trim()).apply();register=!register;showLogin();}});card.addView(mode);
        status=text(connecting?connectionState:profile==null?"尚未设置服务器 · 点击右上角连接":"已保存服务器 · "+profile.host,11,MUTED,false);status.setGravity(Gravity.CENTER);form.addView(status);gap(form,10);TextView privacy=text("密码只用于本次云端认证，不保存到手机。自动登录只保存一个可吊销的登录凭证，退出账号或修改密码后失效。",11,MUTED,false);privacy.setGravity(Gravity.CENTER);form.addView(privacy);host.addView(form,new LinearLayout.LayoutParams(wide?0:-1,-2,wide?1:0));
        loginButton.setEnabled(!connecting||autoSigning);nickname.setEnabled(!connecting);password.setEnabled(!connecting);autoSign.setEnabled(!connecting);
    }
    private void connectionDialog(){
        if(connecting||workspace!=null){toast("请先退出账号再更换服务器");return;}
        LinearLayout c=column();c.setPadding(dp(22),dp(12),dp(22),0);c.addView(text("粘贴电脑端或维护者提供的完整连接码。",14,MUTED,false));gap(c,12);
        EditText code=input("TINGLAN1.…",false);code.setSingleLine(false);code.setMinLines(3);code.setMaxLines(6);code.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_MULTI_LINE);c.addView(code);
        gap(c,8);c.addView(button("从剪贴板粘贴",false,()->{ClipboardManager clipboard=(ClipboardManager)getSystemService(CLIPBOARD_SERVICE);if(clipboard.hasPrimaryClip()){ClipData clip=clipboard.getPrimaryClip();if(clip!=null&&clip.getItemCount()>0)code.setText(clip.getItemAt(0).coerceToText(this));}}));
        if(profile!=null){gap(c,10);c.addView(text("当前："+profile.host+":"+profile.port+"\nCA SHA-256："+profile.fingerprint,11,MUTED,false));}
        AlertDialog dialog=new AlertDialog.Builder(this).setTitle("服务器连接").setView(c).setNegativeButton("取消",null).setPositiveButton("检查连接码",null).create();
        dialog.setOnShowListener(d->dialog.getButton(-1).setOnClickListener(v->{try{String raw=code.getText().toString().trim();ConnectionProfile parsed=ConnectionProfile.parse(raw);new AlertDialog.Builder(this).setTitle("确认服务器与证书").setMessage(parsed.host+":"+parsed.port+"\n\nCA SHA-256\n"+parsed.fingerprint+"\n\n请确认连接码来自你的服务器维护者。").setNegativeButton("取消",null).setPositiveButton("信任并保存",(d2,w)->{profile=parsed;prefs.edit().putString("code",raw).remove("token").remove("tokenService").apply();dialog.dismiss();showLogin();}).show();}catch(Exception e){toast(e.getMessage());}}));dialog.show();
    }
    private void login(){
        if(profile==null){connectionDialog();return;}String nick=nickname.getText().toString().trim(),secret=password.getText().toString();int bytes=secret.getBytes(StandardCharsets.UTF_8).length;
        if(nick.isEmpty()||nick.getBytes(StandardCharsets.UTF_8).length>60||bytes<1||bytes>128||(register&&bytes<12)){toast(register?"昵称不能为空，密码需为 12–128 字节":"请输入昵称和密码（密码最多 128 字节）");return;}
        boolean remember=autoSign==null||autoSign.isChecked();SharedPreferences.Editor edit=prefs.edit().putString("nickname",nick).putInt("service",service).putBoolean("autoLogin",remember);if(!remember)edit.remove("token").remove("tokenService");edit.apply();
        connecting=true;password.setText("");nickname.setEnabled(false);password.setEnabled(false);loginButton.setEnabled(false);autoSign.setEnabled(false);
        start(nick,secret,"",remember);
    }
    /** Sign in with the token saved by an earlier "下次打开自动登录"; the password is never stored. */
    private void autoLogin(){
        service=Math.max(0,Math.min(2,prefs.getInt("tokenService",service)));connecting=true;autoSigning=true;connectionState="正在自动登录…";showLogin();
        start(prefs.getString("nickname",""),"",prefs.getString("token",""),true);
    }
    private void cancelAutoLogin(){if(client!=null){client.close();client=null;}connecting=false;autoSigning=false;connectionState="已取消自动登录";showLogin();}
    private void start(String nick,String secret,String token,boolean remember){
        final ChatClient[] holder=new ChatClient[1];
        ChatClient next=new ChatClient(profile,nick,secret,token,service,register,remember,new ChatClient.Listener(){
            private void ui(Runnable r){runOnUiThread(()->{if(!destroyed&&client==holder[0])r.run();});}
            public void state(String detail,boolean online){ui(()->{connectionState=detail;if(status!=null)status.setText(detail);enableSend(online&&!sending);});}
            public void token(String value,int signedService){ui(()->prefs.edit().putString("token",value).putInt("tokenService",signedService).apply());}
            public void signedOut(){ui(()->prefs.edit().remove("token").remove("tokenService").apply());}
            public void qr(JSONObject result){ui(()->qrAnswer(result));}
            public void found(JSONArray users){ui(()->showFound(users));}
            public void searchFailed(String detail){ui(()->{if(searchStatus!=null)searchStatus.setText(detail);});}
            public void snapshot(JSONObject data){ui(()->{try{boolean first=workspace==null;int position=scroll==null?0:scroll.getScrollY();workspace=new Workspace(data);connecting=false;autoSigning=false;register=false;if(first){tab=service==2?FEED:CHATS;channel=service==2?"weibo":"moments";weiboInbox=false;weiboFilter=0;autoUpdateCheck();}String name=workspace.name(workspace.viewer);prefs.edit().putString("nickname",name).apply();if(workspace.account(workspace.viewer,service).length()==0){JSONArray all=workspace.users.get(workspace.viewer).getJSONArray("services");service=all.getJSONObject(0).getInt("type");}
                if(chatKind>=0){if(conversationPane!=null)fillConversations(conversationPane,conversationQuery);renderMessages();}else{showHome();if(!first)scroll.post(()->scroll.scrollTo(0,position));}}catch(Exception e){logout();toast("同步失败："+e.getMessage());}});}
            public void result(String id,JSONObject result){ui(()->{Runnable done=completions.remove(id);if(done!=null)done.run();Runnable good=successes.remove(id);if(result.optBoolean("ok")){if(good!=null)good.run();}else toast(result.optString("detail","服务器拒绝操作"));});}
            public void failure(String detail,boolean terminal){ui(()->{if(terminal){logout();new AlertDialog.Builder(MainActivity.this).setTitle("连接未完成").setMessage(detail).setPositiveButton("知道了",null).show();}else toast(detail);});}
        });holder[0]=next;client=next;next.start();
    }
    /** 退出账号: the server forgets this phone's automatic sign-in before the connection closes. */
    private void signOut(){
        prefs.edit().remove("token").remove("tokenService").apply();ChatClient leaving=client;client=null;
        if(leaving!=null){leaving.signOut();new android.os.Handler(getMainLooper()).postDelayed(leaving::close,3000);}logout();
    }
    private void logout(){if(client!=null){client.close();client=null;}workspace=null;connecting=false;autoSigning=false;sending=false;buddy=null;buddyBubble=null;chatKind=-1;sendRequest="";composer=null;sendButton=null;conversationPane=null;faces.clear();successes.clear();completions.clear();showLogin();}
    private String command(JSONObject data,Runnable good,Runnable done){try{if(client==null)throw new IllegalStateException("请先登录");String id=client.command(data);if(good!=null)successes.put(id,good);if(done!=null)completions.put(id,done);return id;}catch(Exception e){toast(e.getMessage());if(done!=null)done.run();return "";}}
    private void command(JSONObject data){command(data,null,null);}

    private String[] heading(){
        if(tab==MEMBER)return new String[]{"会员与充值","解锁更多气泡、头像框与小火人外观"};
        if(service==2)return tab==MINE?new String[]{"我的空间","个人资料与服务，在此从容打理"}:weiboInbox?new String[]{"微博私信","同频的人，会有回音"}:weiboFilter==1?new String[]{"我的关注","关注的灵感，正在路上"}:new String[]{"微博","发现 · 关注 · 与同频的人交换灵感"};
        switch(tab){
            case FRIENDS:return new String[]{"好友","与你相伴的人"};
            case GROUPS:return new String[]{"群组","在这里，找到同频的人"};
            case NOTICES:return new String[]{"通知","好友邀请与群组申请，都在这里"};
            case FEED:return channel.equals("weibo")?new String[]{"微博","发现 · 关注 · 与同频的人交换灵感"}:new String[]{"朋友圈","把生活分享给朋友"};
            case MINE:return new String[]{"我的","照顾好每一个自己"};
            default:return new String[]{"会话","每一次相逢，都值得倾听"};
        }
    }
    private View bellAction(){int count=noticeCount();FrameLayout bell=iconAction("bell",count>0?"通知，"+count+" 条待处理":"通知",()->go(NOTICES));if(count>0)bell.addView(badge(count),new FrameLayout.LayoutParams(-2,dp(16),Gravity.TOP|Gravity.END));return bell;}
    private void pageHeader(){
        String[] h=heading();boolean big=wide();LinearLayout bar=row();bar.setPadding(0,dp(4),0,dp(4));
        if(!big){View me=avatarUser(workspace.viewer,40);me.setContentDescription("我的资料");me.setOnClickListener(v->{if(service==2){tab=MINE;showHome();}else go(MINE);});LinearLayout.LayoutParams size=new LinearLayout.LayoutParams(dp(40),dp(40));size.rightMargin=dp(12);bar.addView(me,size);}
        LinearLayout labels=column();labels.addView(line(h[0],big?24:21,INK,true));labels.addView(line(h[1],big?12:11,MUTED,false));bar.addView(labels,new LinearLayout.LayoutParams(0,-2,1));
        int actions=0;
        if(tab==MEMBER){bar.addView(iconAction("back","返回我的",()->{tab=MINE;showHome();}));actions++;}
        if(service!=2&&tab==CHATS){bar.addView(iconAction("scan","扫一扫登录电脑",this::scan));actions++;}
        if(service!=2&&tab!=NOTICES){bar.addView(bellAction());actions++;}
        if(service!=2&&(tab==FRIENDS||tab==CHATS)){bar.addView(iconAction("plus","添加好友",this::addFriend));actions++;}
        else if(service!=2&&tab==GROUPS){bar.addView(iconAction("plus","创建群聊",this::createGroup));actions++;}
        else if(service==2&&!weiboInbox&&tab!=MINE){bar.addView(iconAction("mail","私信",()->{tab=FEED;weiboInbox=true;showHome();}));actions++;}
        if(actions<2)bar.addView(iconAction("refresh","刷新",()->command(Wire.obj("op","sync"))));
        root.addView(bar);gap(root,big?14:10);
    }
    private void showHome(){
        if(workspace==null){showLogin();return;}chatKind=-1;composer=null;sendButton=null;conversationPane=null;
        if(service==2&&tab!=MINE&&tab!=MEMBER)tab=FEED;
        boolean split=splitView()&&service!=2&&tab==CHATS;base(split);
        if(split){emptyChat(splitChats());return;}
        pageHeader();if(!wide()){serviceStrip(root,false);gap(root,9);connectionLabel();}makeScroll();
        if(tab==CHATS)conversations();else if(tab==FRIENDS)friends();else if(tab==GROUPS)groups();else if(tab==NOTICES)notices();else if(tab==MEMBER)membership();
        else if(tab==FEED){if(weiboInbox&&channel.equals("weibo"))renderWeiboInbox();else feed();}else mine();
        if(!wide())navigation();
    }
    private JSONObject appearance(int id){JSONObject all=workspace==null?null:workspace.raw.optJSONObject("appearances");JSONObject look=all==null?null:all.optJSONObject(String.valueOf(id));return look==null?new JSONObject():look;}
    private Bitmap face(String encoded){
        if(encoded.isEmpty()||encoded.length()>=180000)return null;if(faces.containsKey(encoded))return faces.get(encoded);Bitmap bitmap=null;
        try{byte[] bytes=java.util.Base64.getDecoder().decode(encoded);BitmapFactory.Options options=new BitmapFactory.Options();options.inScaled=false;options.inSampleSize=2;bitmap=BitmapFactory.decodeByteArray(bytes,0,bytes.length,options);}catch(Exception ignored){}
        faces.put(encoded,bitmap);return bitmap;
    }
    private View avatarUser(int id,int size){JSONObject look=appearance(id);View v=new UiKit.Avatar(this,workspace==null?"澜":workspace.name(id),id,face(look.optString("avatar")),look.optString("frame","none"),false);v.setLayoutParams(new LinearLayout.LayoutParams(dp(size),dp(size)));return v;}
    private View groupAvatar(int id,String name,int size){View v=new UiKit.Avatar(this,name,id,null,"none",true);v.setLayoutParams(new LinearLayout.LayoutParams(dp(size),dp(size)));return v;}
    /** Desktop ConversationDelegateHsx row: avatar, name and time, preview and unread badge. */
    private LinearLayout listRow(LinearLayout parent,View face,String title,String time,String subtitle,int unread,String tag,boolean selected,Runnable tap){
        LinearLayout r=row();r.setPadding(dp(12),dp(11),dp(12),dp(11));r.setBackground(UiKit.touch(selected?(blog()?palette.soft:0xffdceeff):Color.TRANSPARENT,GREEN,14,density()));r.addView(face,new LinearLayout.LayoutParams(dp(50),dp(50)));
        LinearLayout labels=column();labels.setPadding(dp(13),0,0,0);LinearLayout first=row();first.addView(line(title,15,INK,true),new LinearLayout.LayoutParams(0,-2,1));if(!time.isEmpty()){TextView t=text(time,10,MUTED,false);t.setPadding(dp(8),0,0,0);first.addView(t);}labels.addView(first);gap(labels,3);
        LinearLayout second=row();second.addView(line(subtitle,12,MUTED,false),new LinearLayout.LayoutParams(0,-2,1));
        if(unread>0){LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-2,dp(17));lp.leftMargin=dp(8);second.addView(badge(unread),lp);}
        else if(!tag.isEmpty()){LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-2,-2);lp.leftMargin=dp(8);second.addView(pill(tag,palette.soft,GREEN),lp);}
        labels.addView(second);r.addView(labels,new LinearLayout.LayoutParams(0,-2,1));r.setContentDescription(title+(unread>0?"，"+unread+" 条未读":""));r.setOnClickListener(v->tap.run());
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);lp.bottomMargin=dp(3);parent.addView(r,lp);return r;
    }
    private void empty(LinearLayout parent,String title,String detail){
        LinearLayout c=column();c.setGravity(Gravity.CENTER);c.setPadding(dp(20),dp(30),dp(20),dp(28));FrameLayout circle=new FrameLayout(this);circle.setBackground(bg(palette.soft,32));UiKit.Icon glyph=new UiKit.Icon(this,blog()?"compass":"chat",GREEN);circle.addView(glyph,new FrameLayout.LayoutParams(dp(28),dp(28),Gravity.CENTER));c.addView(circle,new LinearLayout.LayoutParams(dp(64),dp(64)));gap(c,16);TextView heading=text(title,18,INK,true);heading.setGravity(Gravity.CENTER);c.addView(heading,new LinearLayout.LayoutParams(-1,-2));gap(c,8);TextView hint=text(detail,13,MUTED,false);hint.setGravity(Gravity.CENTER);c.addView(hint,new LinearLayout.LayoutParams(-1,-2));parent.addView(c,new LinearLayout.LayoutParams(-1,-2));
    }
    private int unreadTotal(){int count=0;for(Workspace.Conversation c:workspace.conversations(service))count+=c.unread;return count;}
    private boolean manages(JSONObject group){return group.optInt("owner")==workspace.viewer||Workspace.contains(group.optJSONArray("admins"),workspace.viewer);}
    /** Same scope as the desktop 通知 badge: incoming friend requests for this service and join requests in groups the viewer manages. */
    private int noticeCount(){
        if(workspace==null||service==2)return 0;int count=0;JSONArray list=workspace.array("requests");
        for(int i=0;i<list.length();i++){JSONObject r=list.optJSONObject(i);if(r!=null&&r.optInt("status",-1)==0&&r.optInt("to")==workspace.viewer&&r.optInt("destination")==service)count++;}
        for(JSONObject g:workspace.groups.values())if(workspace.member(g,service)&&manages(g)){JSONArray pending=g.optJSONArray("pending");if(pending!=null)count+=pending.length();}
        return count;
    }

    private void conversations(){EditText search=searchBox(body,"搜索联系人或群聊");gap(body,10);LinearLayout list=column();body.addView(list);fillConversations(list,"");search.addTextChangedListener(watcher(q->fillConversations(list,q)));}
    private void fillConversations(LinearLayout list,String query){
        conversationQuery=query;list.removeAllViews();int shown=0;String needle=query.toLowerCase(Locale.ROOT);
        for(Workspace.Conversation c:workspace.conversations(service)){
            if(!needle.isEmpty()&&!(c.title+c.preview).toLowerCase(Locale.ROOT).contains(needle))continue;shown++;
            listRow(list,c.kind==1?groupAvatar(c.target,c.title,50):avatarUser(c.target,50),c.title,when(c.time),c.preview,c.unread,"",chatKind==c.kind&&chatTarget==c.target,()->openChat(c.kind,c.target));
        }
        if(shown==0)empty(list,needle.isEmpty()?"还没有会话":"没有找到相关会话",needle.isEmpty()?"去好友页添加好友，或创建你的第一个群聊。":"换个关键词试试。");
    }
    private View tile(String glyph,String title,String detail,Runnable tap){
        LinearLayout t=row();t.setPadding(dp(12),dp(12),dp(12),dp(12));t.setBackground(UiKit.touch(palette.surface,GREEN,16,density()));FrameLayout circle=new FrameLayout(this);circle.setBackground(bg(palette.soft,19));circle.addView(new UiKit.Icon(this,glyph,GREEN),new FrameLayout.LayoutParams(dp(20),dp(20),Gravity.CENTER));t.addView(circle,new LinearLayout.LayoutParams(dp(38),dp(38)));
        LinearLayout labels=column();labels.setPadding(dp(10),0,0,0);labels.addView(line(title,14,INK,true));labels.addView(line(detail,11,MUTED,false));t.addView(labels,new LinearLayout.LayoutParams(0,-2,1));t.setContentDescription(title);t.setOnClickListener(v->tap.run());return t;
    }
    private void friends(){
        int pending=noticeCount();LinearLayout tools=row();tools.addView(tile("bell","新的朋友",pending>0?pending+" 条待处理":"好友与入群申请",()->go(NOTICES)),new LinearLayout.LayoutParams(0,-2,1));
        LinearLayout.LayoutParams right=new LinearLayout.LayoutParams(0,-2,1);right.leftMargin=dp(8);tools.addView(tile("plus","添加好友","向云端用户发送申请",this::addFriend),right);body.addView(tools);gap(body,16);
        JSONArray friends=workspace.account(workspace.viewer,service).optJSONArray("friends");int count=friends==null?0:friends.length();section(body,"我的好友",count);
        if(count==0)empty(body,"还没有好友","点击「添加好友」，发送第一条好友申请。");
        for(int i=0;i<count;i++){JSONObject f=friends.optJSONObject(i);if(f==null)continue;int id=f.optInt("id");JSONObject u=workspace.users.get(id);boolean on=u!=null&&u.optBoolean("online");String place=u==null?"":u.optString("location");
            listRow(body,avatarUser(id,50),workspace.display(id,service),"",(on?"● 在线":"○ 离线")+" · "+SERVICES[Math.max(0,Math.min(2,f.optInt("contact")))]+(place.isEmpty()||place.equals("未公开")?"":" · "+place),0,"聊天",false,()->openChat(0,id));}
    }
    private void groups(){
        body.addView(button("创建群聊",true,this::createGroup),new LinearLayout.LayoutParams(-1,dp(46)));gap(body,16);
        List<JSONObject> joined=new ArrayList<>(),others=new ArrayList<>();for(JSONObject g:workspace.groups.values())(workspace.member(g,service)?joined:others).add(g);
        section(body,"我加入的群",joined.size());if(joined.isEmpty())empty(body,"还没有加入群聊","创建一个群，或申请加入下方的群。");
        for(JSONObject g:joined){int id=g.optInt("id");JSONArray members=g.optJSONArray("members");String name=g.optString("name");listRow(body,groupAvatar(id,name,50),name,"","群号 "+id+" · "+(members==null?0:members.length())+" 人"+(manages(g)?" · 管理者":""),0,"",false,()->openChat(1,id));}
        if(!others.isEmpty()){gap(body,10);section(body,"可申请加入",others.size());
            for(JSONObject g:others){String name=g.optString("name");int gid=g.optInt("id");JSONArray members=g.optJSONArray("members");listRow(body,groupAvatar(gid,name,50),name,"","群号 "+gid+" · "+(members==null?0:members.length())+" 人",0,"申请",false,()->new AlertDialog.Builder(this).setTitle("申请加入此群？").setMessage(name).setNegativeButton("取消",null).setPositiveButton("申请",(d,w)->command(Wire.obj("op","group.apply","group",gid))).show());}}
    }
    private void notices(){
        JSONArray list=workspace.array("requests");int found=0;
        for(int i=0;i<list.length();i++){JSONObject r=list.optJSONObject(i);if(r==null||r.optInt("status",-1)!=0)continue;boolean incoming=r.optInt("to")==workspace.viewer&&r.optInt("destination")==service;boolean outgoing=r.optInt("from")==workspace.viewer&&r.optInt("source")==service;if(!incoming&&!outgoing)continue;
            if(found++==0)section(body,"好友申请",-1);int peer=r.optInt(incoming?"from":"to");String id=r.optString("id");LinearLayout c=card(body);LinearLayout head=row();head.addView(avatarUser(peer,44),new LinearLayout.LayoutParams(dp(44),dp(44)));
            LinearLayout who=column();who.setPadding(dp(12),0,0,0);who.addView(line(workspace.name(peer),15,INK,true));who.addView(line(incoming?"通过 "+SERVICES[Math.max(0,Math.min(2,r.optInt("source")))]+" 申请添加你为好友":"已发送申请，等待对方确认",11,MUTED,false));head.addView(who,new LinearLayout.LayoutParams(0,-2,1));c.addView(head);
            String reason=r.optString("reason");if(!reason.isEmpty()){gap(c,10);TextView quote=text(reason,13,INK,false);quote.setPadding(dp(12),dp(9),dp(12),dp(9));quote.setBackground(bg(BG,10));c.addView(quote);}
            gap(c,12);LinearLayout actions=row();
            if(incoming){actions.addView(button("接受",true,()->command(Wire.obj("op","friend.respond","message",id,"enabled",true))),new LinearLayout.LayoutParams(0,dp(44),1));LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(0,dp(44),1);lp.leftMargin=dp(8);actions.addView(button("拒绝",false,()->command(Wire.obj("op","friend.respond","message",id,"enabled",false))),lp);}
            else actions.addView(button("撤销申请",false,()->command(Wire.obj("op","friend.cancel","message",id))),new LinearLayout.LayoutParams(-1,dp(44)));c.addView(actions);
        }
        int joins=0;
        for(JSONObject g:workspace.groups.values()){if(!workspace.member(g,service)||!manages(g))continue;JSONArray pending=g.optJSONArray("pending");if(pending==null)continue;final int groupId=g.optInt("id");
            for(int j=0;j<pending.length();j++){JSONArray request=pending.optJSONArray(j);if(request==null)continue;if(joins++==0){gap(body,8);section(body,"入群申请",-1);}int user=request.optInt(0),contact=request.optInt(1);
                LinearLayout c=card(body);LinearLayout head=row();head.addView(avatarUser(user,44),new LinearLayout.LayoutParams(dp(44),dp(44)));LinearLayout who=column();who.setPadding(dp(12),0,0,0);who.addView(line(workspace.name(user),15,INK,true));who.addView(line("申请加入 "+g.optString("name"),11,MUTED,false));head.addView(who,new LinearLayout.LayoutParams(0,-2,1));c.addView(head);gap(c,12);
                LinearLayout actions=row();actions.addView(button("同意加入",true,()->command(Wire.obj("op","group.approve","group",groupId,"target",user,"contact",contact,"enabled",true))),new LinearLayout.LayoutParams(0,dp(44),1));LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(0,dp(44),1);lp.leftMargin=dp(8);actions.addView(button("拒绝",false,()->command(Wire.obj("op","group.approve","group",groupId,"target",user,"contact",contact,"enabled",false))),lp);c.addView(actions);}
        }
        if(found+joins==0)empty(body,"暂无新的通知","好友邀请与群组申请，都会出现在这里。");
    }
    // Add friend is search-only: the server answers a full nickname or service number and lists nobody else.
    private LinearLayout searchResults;
    private TextView searchStatus;
    private AlertDialog searchDialog;
    private void addFriend(){
        LinearLayout c=column();c.setPadding(dp(22),dp(10),dp(22),0);c.addView(text("输入对方完整的昵称或服务号码查找。为保护隐私，听澜不展示全部用户。",13,MUTED,false));gap(c,12);
        LinearLayout bar=row();EditText query=input("完整昵称或服务号码",false);query.setImeOptions(android.view.inputmethod.EditorInfo.IME_ACTION_SEARCH);bar.addView(query,new LinearLayout.LayoutParams(0,dp(50),1));
        Runnable find=()->{String term=query.getText().toString().trim();if(term.isEmpty()){searchStatus.setText("请输入完整的昵称或服务号码");return;}searchResults.removeAllViews();searchStatus.setText("正在查找…");try{client.search(term);}catch(Exception e){searchStatus.setText(e.getMessage());}};
        LinearLayout.LayoutParams go=new LinearLayout.LayoutParams(-2,dp(50));go.leftMargin=dp(8);bar.addView(button("查找",true,find),go);c.addView(bar);
        query.setOnEditorActionListener((v,action,event)->{find.run();return true;});
        gap(c,10);searchStatus=text("",12,MUTED,false);c.addView(searchStatus);gap(c,6);searchResults=column();c.addView(searchResults);
        searchDialog=new AlertDialog.Builder(this).setTitle("添加好友").setView(c).setNegativeButton("关闭",null).create();searchDialog.setOnDismissListener(d->{searchResults=null;searchStatus=null;searchDialog=null;});searchDialog.show();
    }
    private void showFound(JSONArray users){
        if(searchResults==null||workspace==null)return;searchResults.removeAllViews();
        searchStatus.setText(users.length()==0?"没有找到。请确认对方的完整昵称或服务号码。":"找到 "+users.length()+" 位用户");
        JSONArray mine=workspace.account(workspace.viewer,service).optJSONArray("friends");
        for(int i=0;i<users.length();i++){
            JSONObject u=users.optJSONObject(i);if(u==null)continue;final int id=u.optInt("id");boolean already=false;if(mine!=null)for(int j=0;j<mine.length();j++)if(mine.optJSONObject(j)!=null&&mine.optJSONObject(j).optInt("id")==id)already=true;
            JSONArray accounts=u.optJSONArray("services");List<Integer> types=new ArrayList<>();List<String> labels=new ArrayList<>();
            if(accounts!=null)for(int k=0;k<accounts.length();k++){JSONObject s=accounts.optJSONObject(k);int t=s==null?-1:s.optInt("type",-1);if(t>=0&&t<3){types.add(t);labels.add(SERVICES[t]+(s.optString("nick").isEmpty()?"":" · "+s.optString("nick")));}}
            final boolean isFriend=already;View face=new UiKit.Avatar(this,u.optString("name"),id,null,"none",false);
            listRow(searchResults,face,u.optString("name"),"",String.join("  ",labels),0,isFriend?"已是好友":"添加",false,()->{
                if(isFriend){toast("你们已经是好友");return;}
                if(searchDialog!=null)searchDialog.dismiss();
                new AlertDialog.Builder(this).setTitle("选择对方的服务").setItems(labels.toArray(new String[0]),(d2,w2)->askText("好友申请","写一句验证信息","你好，我是 "+workspace.name(workspace.viewer),reason->command(Wire.obj("op","friend.request","target",id,"contact",types.get(w2),"reason",reason),()->toast("好友申请已发送，等待对方确认"),null))).show();
            });
        }
    }
    private void createGroup(){askText("创建群聊","输入群名称","",name->command(Wire.obj("op","group.create","name",name,"mode",service==1?1:0,"parent",0)));}

    /** Tablet 会话 page: desktop ChatPageHsx conversation pane beside the chat pane. Returns the chat pane. */
    private LinearLayout splitChats(){
        LinearLayout panes=row();panes.setGravity(Gravity.TOP);root.addView(panes,new LinearLayout.LayoutParams(-1,-1));
        LinearLayout list=column();list.setBackground(new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,new int[]{0xfff6faff,0xffeff6ff}));list.setPadding(dp(12),dp(16),dp(12),dp(8));
        LinearLayout head=row();head.setPadding(dp(4),0,0,0);head.addView(line("会话",19,INK,true),new LinearLayout.LayoutParams(0,-2,1));head.addView(bellAction());head.addView(iconAction("plus","查找 / 添加好友",()->go(FRIENDS)));list.addView(head);gap(list,10);
        EditText search=searchBox(list,"搜索联系人或群聊");gap(list,10);ScrollView listScroll=new ScrollView(this);listScroll.setVerticalScrollBarEnabled(false);LinearLayout rows=column();listScroll.addView(rows);list.addView(listScroll,new LinearLayout.LayoutParams(-1,0,1));
        TextView note=text("每一条消息，都是一次靠近",10,0xff9aacc3,false);note.setGravity(Gravity.CENTER);note.setPadding(0,dp(6),0,0);list.addView(note);
        conversationPane=rows;fillConversations(rows,"");search.addTextChangedListener(watcher(q->fillConversations(rows,q)));
        panes.addView(list,new LinearLayout.LayoutParams(dp(316),-1));View divider=new View(this);divider.setBackgroundColor(LINE);panes.addView(divider,new LinearLayout.LayoutParams(dp(1),-1));
        LinearLayout chat=column();chat.setBackgroundColor(0xfff2f7fe);panes.addView(chat,new LinearLayout.LayoutParams(0,-1,1));scroll=listScroll;return chat;
    }
    private void emptyChat(LinearLayout chat){
        LinearLayout c=column();c.setGravity(Gravity.CENTER);c.addView(icon("wave",0xff9ac5ee,72));gap(c,18);TextView title=text("心意，在这里相遇",24,0xff729ac3,false);title.setTypeface(Typeface.SERIF);title.setLetterSpacing(.08f);title.setGravity(Gravity.CENTER);c.addView(title,new LinearLayout.LayoutParams(-1,-2));gap(c,8);TextView words=text("不必匆忙，一句问候也很好。",12,0xffa2b4ca,false);words.setGravity(Gravity.CENTER);c.addView(words,new LinearLayout.LayoutParams(-1,-2));
        chat.addView(c,new LinearLayout.LayoutParams(-1,-1));
    }
    private void chatHeader(LinearLayout parent,View face,String title,String subtitle,boolean back,String actionLabel,Runnable action){
        boolean pane=wide();LinearLayout h=row();if(pane){h.setPadding(dp(18),dp(12),dp(14),dp(12));h.setBackgroundColor(blog()?palette.surface:0xfff8fbff);}else h.setPadding(0,dp(2),0,dp(8));
        if(back){FrameLayout b=iconAction("back","返回",this::closeChat);((LinearLayout.LayoutParams)b.getLayoutParams()).leftMargin=0;h.addView(b);}
        LinearLayout.LayoutParams fs=new LinearLayout.LayoutParams(dp(44),dp(44));fs.leftMargin=back?dp(8):0;fs.rightMargin=dp(12);h.addView(face,fs);
        LinearLayout labels=column();labels.addView(line(title,18,INK,true));labels.addView(line(subtitle,11,MUTED,false));h.addView(labels,new LinearLayout.LayoutParams(0,-2,1));
        if(action!=null)h.addView(iconAction("info",actionLabel,action));parent.addView(h);
        if(pane){View rule=new View(this);rule.setBackgroundColor(LINE);parent.addView(rule,new LinearLayout.LayoutParams(-1,dp(1)));}
    }
    private void messageArea(LinearLayout parent){
        scroll=new ScrollView(this);scroll.setFillViewport(true);scroll.setVerticalScrollBarEnabled(false);chatMessages=column();boolean pane=wide();chatMessages.setPadding(pane?dp(18):dp(2),dp(12),pane?dp(18):dp(2),dp(12));scroll.addView(chatMessages);
        buddy=null;buddyBubble=null;
        if(chatKind!=0){parent.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));return;}
        FrameLayout stage=new FrameLayout(this);stage.addView(scroll,new FrameLayout.LayoutParams(-1,-1));buddyStage(stage);parent.addView(stage,new LinearLayout.LayoutParams(-1,0,1));
    }
    /** Desktop floating buddy: hovers at the lower right of the messages, can be dragged, a tap says hello, a long press opens the shared wardrobe. */
    private void buddyStage(FrameLayout stage){
        buddyBubble=text("",12,0xff5b4334,false);buddyBubble.setPadding(dp(14),dp(9),dp(14),dp(9));buddyBubble.setMaxWidth(dp(210));buddyBubble.setBackground(outline(0xf5ffffff,16));buddyBubble.setElevation(dp(2));buddyBubble.setVisibility(View.GONE);
        FrameLayout.LayoutParams at=new FrameLayout.LayoutParams(-2,-2,Gravity.BOTTOM|Gravity.END);at.rightMargin=dp(96);at.bottomMargin=dp(46);stage.addView(buddyBubble,at);
        buddy=new FireBuddy(this,false);FrameLayout.LayoutParams size=new FrameLayout.LayoutParams(dp(92),dp(104),Gravity.BOTTOM|Gravity.END);size.rightMargin=dp(8);size.bottomMargin=dp(6);stage.addView(buddy,size);
        buddy.setTranslationX(buddyX);buddy.setTranslationY(buddyY);buddyBubble.setTranslationX(buddyX);buddyBubble.setTranslationY(buddyY);
        final float slop=android.view.ViewConfiguration.get(this).getScaledTouchSlop();final float[] start=new float[4];final boolean[] moved={false};final int friend=chatTarget;
        final Runnable hold=()->{moved[0]=true;wardrobe(friend);};
        buddy.setOnTouchListener((v,e)->{
            switch(e.getActionMasked()){
                case android.view.MotionEvent.ACTION_DOWN:start[0]=e.getRawX();start[1]=e.getRawY();start[2]=v.getTranslationX();start[3]=v.getTranslationY();moved[0]=false;v.postDelayed(hold,550);return true;
                case android.view.MotionEvent.ACTION_MOVE:{float dx=e.getRawX()-start[0],dy=e.getRawY()-start[1];if(!moved[0]&&Math.hypot(dx,dy)<slop)return true;if(!moved[0]){moved[0]=true;v.removeCallbacks(hold);}
                    // Keep the buddy inside the message area.
                    float x=Math.max(-(stage.getWidth()-v.getWidth()-dp(16)),Math.min(0,start[2]+dx)),y=Math.max(-(stage.getHeight()-v.getHeight()-dp(12)),Math.min(0,start[3]+dy));
                    v.setTranslationX(x);v.setTranslationY(y);buddyBubble.setTranslationX(x);buddyBubble.setTranslationY(y);buddyX=x;buddyY=y;return true;}
                case android.view.MotionEvent.ACTION_UP:v.removeCallbacks(hold);if(!moved[0])greet();return true;
                case android.view.MotionEvent.ACTION_CANCEL:v.removeCallbacks(hold);return true;
                default:return false;
            }
        });
        updateBuddy();
    }
    private int greetings=0;
    private void greet(){
        if(buddy==null||buddyBubble==null)return;String[] lines={"在呢，今天也要好好聊天！","被你戳到啦～","一起把今天的火苗点亮吧"};
        String detail=buddy.days()>0?"连续 "+buddy.days()+" 天 · Lv."+buddy.level()+" · "+FireBuddy.status(buddy.today()):FireBuddy.status(buddy.today());
        buddyBubble.setText(lines[greetings++%lines.length]+"\n"+detail+"\n长按小火人可以换装");buddyBubble.setVisibility(View.VISIBLE);
        buddyBubble.removeCallbacks(hideBubble);buddyBubble.postDelayed(hideBubble,3600);
    }
    private final Runnable hideBubble=()->{if(buddyBubble!=null)buddyBubble.setVisibility(View.GONE);};
    private JSONObject buddyLook(int friend){JSONObject all=workspace.raw.optJSONObject("buddies");JSONObject look=all==null?null:all.optJSONObject(String.valueOf(friend));return look==null?new JSONObject():look;}
    private void updateBuddy(){
        if(buddy==null||workspace==null||chatKind!=0)return;JSONObject look=buddyLook(chatTarget);
        buddy.setLook(FireBuddy.find(FireBuddy.PETS,look.optString("pet")),FireBuddy.find(FireBuddy.OUTFITS,look.optString("outfit")));buddy.setState(FireBuddy.streak(workspace,service,chatTarget));
    }
    private int memberTier(){JSONObject m=workspace.raw.optJSONObject("membership");return m==null?0:m.optInt("tier");}
    /** Wardrobe for the buddy shared with one friend: either side may dress it and both see the change. */
    private void wardrobe(int friend){
        if(workspace==null)return;JSONObject look=buddyLook(friend);final FireBuddy.Style skin=FireBuddy.find(FireBuddy.PETS,look.optString("pet")),wear=FireBuddy.find(FireBuddy.OUTFITS,look.optString("outfit"));final int tier=memberTier();
        LinearLayout content=column();content.setPadding(dp(20),dp(6),dp(20),dp(4));
        FireBuddy preview=new FireBuddy(this,true);preview.setLook(skin,wear);LinearLayout.LayoutParams big=new LinearLayout.LayoutParams(dp(96),dp(110));big.gravity=Gravity.CENTER_HORIZONTAL;content.addView(preview,big);
        TextView note=text("你和 "+workspace.display(friend,service)+" 共用这只小火人，换装后对方会同时看到。会员款需开通相应会员。",12,MUTED,false);note.setGravity(Gravity.CENTER);content.addView(note);gap(content,10);
        final AlertDialog[] dialog=new AlertDialog[1];
        for(int kind=0;kind<2;kind++){
            final boolean pets=kind==0;section(content,pets?"皮肤":"服装",-1);FireBuddy.Style[] all=pets?FireBuddy.PETS:FireBuddy.OUTFITS;LinearLayout line=null;
            for(int i=0;i<all.length;i++){
                if(i%3==0){line=row();line.setGravity(Gravity.TOP);content.addView(line);}final FireBuddy.Style item=all[i];boolean current=item==(pets?skin:wear),locked=item.tier>tier;
                LinearLayout cell=column();cell.setGravity(Gravity.CENTER_HORIZONTAL);cell.setPadding(dp(4),dp(8),dp(4),dp(8));cell.setBackground(UiKit.touch(current?palette.soft:palette.surface,GREEN,14,density()));
                FireBuddy sample=new FireBuddy(this,true);sample.setLook(pets?item:skin,pets?wear:item);cell.addView(sample,new LinearLayout.LayoutParams(dp(56),dp(64)));
                TextView name=line(item.name,12,INK,current);name.setGravity(Gravity.CENTER);cell.addView(name,new LinearLayout.LayoutParams(-1,-2));
                TextView state=line(current?"正在使用":locked?FireBuddy.TIERS[item.tier]+"会员":"免费",10,current?GREEN:MUTED,current);state.setGravity(Gravity.CENTER);cell.addView(state,new LinearLayout.LayoutParams(-1,-2));
                cell.setContentDescription(item.name+"，"+(current?"正在使用":locked?"需要"+FireBuddy.TIERS[item.tier]+"会员":"可以换上"));
                cell.setOnClickListener(v->{
                    if(locked){toast("开通"+FireBuddy.TIERS[item.tier]+"会员后可以换上「"+item.name+"」");return;}if(current){toast("小火人已经穿着「"+item.name+"」");return;}
                    JSONObject profile=Wire.obj(pets?"pet":"outfit",item.id);command(Wire.obj("op","buddy.update","target",friend,"profile",profile),()->toast("小火人换上了「"+item.name+"」，好友那边也同步了"),null);if(dialog[0]!=null)dialog[0].dismiss();
                });
                LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(0,-2,1);lp.setMargins(dp(3),dp(3),dp(3),dp(3));line.addView(cell,lp);
            }
            if(all.length%3!=0)for(int k=all.length%3;k<3;k++)line.addView(new Space(this),new LinearLayout.LayoutParams(0,1,1));
            gap(content,6);
        }
        ScrollView s=new ScrollView(this);s.addView(content);dialog[0]=new AlertDialog.Builder(this).setTitle("小火人衣橱").setView(s).setPositiveButton("完成",null).show();
    }
    /** Pick a friend, then dress the buddy you share. */
    private void chooseWardrobe(){
        JSONArray friends=workspace.account(workspace.viewer,service).optJSONArray("friends");if(friends==null||friends.length()==0){toast("添加好友后，就能和好友一起给小火人换装");return;}
        String[] names=new String[friends.length()];int[] ids=new int[friends.length()];for(int i=0;i<friends.length();i++){ids[i]=friends.optJSONObject(i).optInt("id");names[i]=workspace.display(ids[i],service);}
        new AlertDialog.Builder(this).setTitle("和谁的小火人").setItems(names,(d,w)->wardrobe(ids[w])).show();
    }
    /** 扫一扫: scan the sign-in QR code shown by the desktop client, then confirm on this phone. */
    private void scan(){
        if(!online()){toast("连接服务器后才能扫码登录电脑");return;}
        startActivityForResult(new android.content.Intent(this,ScanActivity.class),SCAN);
    }
    @Override protected void onActivityResult(int request,int result,android.content.Intent data){
        super.onActivityResult(request,result,data);if(request!=SCAN||result!=RESULT_OK||data==null)return;String text=data.getStringExtra(ScanActivity.RESULT);
        if(text==null||!text.matches("TINGLANQR1:[0-9a-f]{32}")){toast("这不是听澜电脑端的登录二维码");return;}
        scannedCode=text.substring(11);try{client.qr("qr_scan",scannedCode,false);}catch(Exception e){toast(e.getMessage());}
    }
    private void qrAnswer(JSONObject r){
        if(!r.optBoolean("ok")){new AlertDialog.Builder(this).setTitle("电脑登录未完成").setMessage(r.optString("detail","二维码已失效，请在电脑上刷新")).setPositiveButton("知道了",null).show();return;}
        String stage=r.optString("stage");final String code=scannedCode;
        if(stage.equals("scan")){
            int svc=Math.max(0,Math.min(2,r.optInt("service")));String address=r.optString("address");
            new AlertDialog.Builder(this).setTitle("登录电脑端听澜？").setMessage("将以「"+workspace.name(workspace.viewer)+"」的"+SERVICES[svc]+"账号登录这台电脑。"+(address.isEmpty()?"":"\n电脑网络地址："+address)+"\n\n手机会保持登录。如果不是你本人在电脑前操作，请点「取消」。").setCancelable(false)
                .setPositiveButton("确认登录",(d,w)->{try{client.qr("qr_confirm",code,true);}catch(Exception e){toast(e.getMessage());}})
                .setNegativeButton("取消",(d,w)->{try{client.qr("qr_confirm",code,false);}catch(Exception e){toast(e.getMessage());}}).show();
        }else if(stage.equals("confirm"))toast("电脑端已登录");
        else if(stage.equals("cancel"))toast("已取消电脑登录");
    }
    /** Desktop composer: emoji button, rounded input and a round send button. */
    private void composer(LinearLayout parent,String hint){
        LinearLayout frame=row();frame.setPadding(dp(6),dp(6),dp(6),dp(6));frame.setBackground(outline(palette.surface,20));
        FrameLayout emoji=iconAction("smile","插入表情",()->{});emoji.setOnClickListener(this::emojiMenu);frame.addView(emoji,new LinearLayout.LayoutParams(dp(40),dp(40)));
        composer=new EditText(this);composer.setHint(hint);composer.setTextSize(15);composer.setTextColor(INK);composer.setHintTextColor(MUTED);composer.setHighlightColor(palette.soft);composer.setBackground(bg(blog()?palette.soft:0xfff0f5fb,17));composer.setPadding(dp(14),dp(9),dp(14),dp(9));composer.setSingleLine(false);composer.setMaxLines(4);
        composer.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_MULTI_LINE|InputType.TYPE_TEXT_FLAG_CAP_SENTENCES);composer.setSaveEnabled(false);composer.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO);
        LinearLayout.LayoutParams field=new LinearLayout.LayoutParams(0,-2,1);field.leftMargin=dp(4);field.rightMargin=dp(8);frame.addView(composer,field);
        FrameLayout send=new FrameLayout(this);send.setContentDescription("发送消息");send.setBackground(UiKit.touch(GREEN,Color.WHITE,22,density()));send.addView(new UiKit.Icon(this,"send",palette.onAccent),new FrameLayout.LayoutParams(dp(21),dp(21),Gravity.CENTER));send.setOnClickListener(v->send());frame.addView(send,new LinearLayout.LayoutParams(dp(44),dp(44)));sendButton=send;
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);if(wide())lp.setMargins(dp(14),dp(8),dp(14),dp(12));else{lp.topMargin=dp(6);lp.bottomMargin=dp(4);}parent.addView(frame,lp);enableSend(online()&&!sending);
    }
    private void emojiMenu(View anchor){PopupMenu menu=new PopupMenu(this,anchor);for(String e:EMOJI)menu.getMenu().add(e);menu.setOnMenuItemClickListener(item->{if(composer!=null){composer.getText().insert(Math.max(0,composer.getSelectionStart()),item.getTitle());composer.requestFocus();}return true;});menu.show();}
    private void enableSend(boolean on){if(sendButton!=null){sendButton.setEnabled(on);sendButton.setAlpha(on?1f:.45f);}}
    private void closeChat(){hideKeyboard();boolean direct=chatKind==2;chatKind=-1;if(direct){tab=FEED;weiboInbox=true;}showHome();}
    private void openChat(int kind,int target){
        JSONObject group=kind==1?workspace.groups.get(target):null;if(kind==1&&group==null){toast("群聊不存在或已解散");showHome();return;}
        chatKind=kind;chatTarget=target;sending=false;lastReadBatch="";boolean split=splitView();if(split)tab=CHATS;base(split);LinearLayout chat=split?splitChats():root;
        String title,subtitle;View face;
        if(kind==0){title=workspace.display(target,service);JSONObject u=workspace.users.get(target);String place=u==null?"":u.optString("location");subtitle=(u!=null&&u.optBoolean("online")?"在线":"离线")+" · 私聊 · "+SERVICES[service]+(place.isEmpty()||place.equals("未公开")?"":" · "+place);face=avatarUser(target,44);}
        else{title=group.optString("name");JSONArray members=group.optJSONArray("members");String notice=group.optString("notice");subtitle=(members==null?0:members.length())+" 位成员 · 群聊 · "+SERVICES[service]+(notice.isEmpty()?"":" · "+notice);face=groupAvatar(target,title,44);}
        chatHeader(chat,face,title,subtitle,!split,kind==1?"群资料":"会话资料",kind==1?this::groupInfo:this::contactInfo);
        if(!split)connectionLabel();messageArea(chat);composer(chat,"输入消息…");renderMessages();
    }
    private int bubbleWidth(){int width=getResources().getConfiguration().screenWidthDp-(splitView()?212+316+150:wide()?212+150:150);return dp(Math.max(160,Math.min(width,460)));}
    private String daySeparator(LinearLayout parent,long created,String last){
        Date date=new Date(created*1000);String day=new SimpleDateFormat("yyyyMMdd",Locale.ROOT).format(date);if(day.equals(last))return last;
        String label=day.equals(new SimpleDateFormat("yyyyMMdd",Locale.ROOT).format(new Date()))?"今天":new SimpleDateFormat("yyyy年M月d日",Locale.CHINA).format(date);
        TextView stamp=text(label,10,blog()?MUTED:0xff6b7f91,false);stamp.setLineSpacing(0,1);stamp.setPadding(dp(12),dp(4),dp(12),dp(4));stamp.setBackground(bg(0xbbffffff,8));LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-2,-2);lp.gravity=Gravity.CENTER_HORIZONTAL;lp.topMargin=dp(4);lp.bottomMargin=dp(12);parent.addView(stamp,lp);return day;
    }
    private void note(LinearLayout parent,String value){TextView t=text(value,12,MUTED,false);t.setGravity(Gravity.CENTER);t.setPadding(0,dp(12),0,dp(12));parent.addView(t,new LinearLayout.LayoutParams(-1,-2));}
    /** One desktop-style message: avatar beside the sender's chosen bubble, name and time underneath. */
    private TextView messageRow(LinearLayout parent,int sender,boolean own,String value,long created,String extra,boolean recalled){
        LinearLayout line=row();line.setGravity((own?Gravity.END:Gravity.START)|Gravity.TOP);int[] style=UiKit.bubble(recalled?"paper":appearance(sender).optString("bubble","jade"));
        LinearLayout stack=column();stack.setGravity(own?Gravity.END:Gravity.START);TextView bubble=text(value,15,recalled?MUTED:style[2],false);bubble.setMaxWidth(bubbleWidth());bubble.setPadding(dp(14),dp(10),dp(14),dp(10));bubble.setBackground(UiKit.bubble(style,own,density()));bubble.setTextIsSelectable(!recalled);stack.addView(bubble);
        TextView meta=text(workspace.name(sender)+"  "+time(created)+extra,10,MUTED,false);meta.setPadding(dp(4),dp(3),dp(4),0);stack.addView(meta);
        View face=avatarUser(sender,36);LinearLayout.LayoutParams fs=new LinearLayout.LayoutParams(dp(36),dp(36));
        if(own){fs.leftMargin=dp(8);line.addView(stack);line.addView(face,fs);}else{fs.rightMargin=dp(8);line.addView(face,fs);line.addView(stack);}
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);lp.bottomMargin=dp(12);parent.addView(line,lp);return bubble;
    }
    private void renderMessages(){
        if(workspace==null||chatKind<0||chatMessages==null)return;if(chatKind==2){renderDirectMessages();return;}List<JSONObject> all=workspace.history(service,chatKind,chatTarget);boolean nearBottom=scroll.getChildAt(0).getHeight()-scroll.getHeight()-scroll.getScrollY()<dp(100);chatMessages.removeAllViews();JSONArray unread=new JSONArray();
        if(all.size()>300)note(chatMessages,"显示最近 300 条消息，完整记录仍保存在服务器");String day="";
        for(int i=Math.max(0,all.size()-300);i<all.size();i++){JSONObject m=all.get(i);int sender=m.optInt("sender");boolean own=sender==workspace.viewer,recalled=m.optBoolean("recalled");day=daySeparator(chatMessages,m.optLong("created"),day);
            String extra=(Workspace.contains(m.optJSONArray("stars"),workspace.viewer)?"  · 收藏":"")+(own&&chatKind==0&&!recalled?(Workspace.contains(m.optJSONArray("read"),chatTarget)?"  · 已读":"  · 未读"):"");
            TextView bubble=messageRow(chatMessages,sender,own,recalled?"消息已撤回":m.optString("text"),m.optLong("created"),extra,recalled);bubble.setOnLongClickListener(v->{messageActions(m);return true;});
            if(!own&&!recalled&&!Workspace.contains(m.optJSONArray("read"),workspace.viewer))unread.put(m.optString("id"));}
        if(all.isEmpty())note(chatMessages,"还没有消息，发送第一句问候吧。");updateBuddy();
        if(nearBottom||all.size()<5)scroll.post(()->scroll.fullScroll(View.FOCUS_DOWN));String batch=unread.toString();if(unread.length()>0&&!batch.equals(lastReadBatch)&&client!=null&&client.isOnline()){lastReadBatch=batch;command(Wire.obj("op","chat.read","messages",unread));}
    }
    private void send(){
        if(composer==null||sending)return;String value=composer.getText().toString();if(value.trim().isEmpty())return;int limit=chatKind==2?2000:5500;if(value.getBytes(StandardCharsets.UTF_8).length>limit){toast("内容过长，请缩短后再发送");return;}
        sending=true;enableSend(false);EditText editor=composer;View action=sendButton;JSONObject data=chatKind==2?Wire.obj("op","weibo.dm.send","target",chatTarget,"post",directPost,"text",value):Wire.obj("op","chat.send","kind",chatKind,"target",chatTarget,"text",value);
        sendRequest=command(data,()->{if(editor.getText().toString().equals(value))editor.setText("");},()->{sending=false;if(action==sendButton)enableSend(online());});
    }
    private void messageActions(JSONObject m){if(m.optBoolean("recalled"))return;List<String> actions=new ArrayList<>();actions.add("复制文字");actions.add(Workspace.contains(m.optJSONArray("stars"),workspace.viewer)?"取消收藏":"收藏消息");if(m.optInt("sender")==workspace.viewer)actions.add("撤回消息");new AlertDialog.Builder(this).setItems(actions.toArray(new String[0]),(d,w)->{if(w==0){((ClipboardManager)getSystemService(CLIPBOARD_SERVICE)).setPrimaryClip(ClipData.newPlainText("听澜消息",m.optString("text")));toast("已复制");}else if(w==1)command(Wire.obj("op","chat.star","message",m.optString("id"),"enabled",!Workspace.contains(m.optJSONArray("stars"),workspace.viewer)));else command(Wire.obj("op","chat.recall","message",m.optString("id")));}).show();}
    /** Desktop 会话资料 panel as a dialog: location, service, remark, group and message statistics. */
    /** Desktop 会话资料 as a profile card: avatar and presence, facts in a card, then messages, favourites and streak. */
    private void contactInfo(){
        final int id=chatTarget;JSONObject u=workspace.users.get(id);String remark="",category="";int contact=service;boolean friend=false;JSONArray friends=workspace.account(workspace.viewer,service).optJSONArray("friends");
        if(friends!=null)for(int i=0;i<friends.length();i++){JSONObject f=friends.optJSONObject(i);if(f!=null&&f.optInt("id")==id){remark=f.optString("remark");category=f.optString("category");contact=Math.max(0,Math.min(2,f.optInt("contact")));friend=true;}}
        List<JSONObject> history=workspace.history(service,0,id);int stars=0;for(JSONObject m:history)if(Workspace.contains(m.optJSONArray("stars"),workspace.viewer))stars++;
        boolean on=u!=null&&u.optBoolean("online");String place=u==null?"未公开":u.optString("location","未公开");
        LinearLayout c=column();c.setPadding(dp(20),dp(20),dp(20),dp(4));
        LinearLayout head=row();head.addView(avatarUser(id,64),new LinearLayout.LayoutParams(dp(64),dp(64)));LinearLayout who=column();who.setPadding(dp(14),0,0,0);
        who.addView(line(workspace.display(id,service),20,INK,true));gap(who,4);LinearLayout state=row();state.addView(dot(on?UiKit.ONLINE:MUTED));
        TextView presence=text((on?"在线":"离线")+(friend?" · 好友 · "+SERVICES[contact]:" · 私聊"),12,MUTED,false);presence.setPadding(dp(6),0,0,0);state.addView(presence);who.addView(state);
        if(!remark.isEmpty())who.addView(line("昵称  "+workspace.name(id),11,MUTED,false));head.addView(who,new LinearLayout.LayoutParams(0,-2,1));c.addView(head);gap(c,16);
        LinearLayout card=column();card.setPadding(dp(14),dp(4),dp(14),dp(4));card.setBackground(outline(palette.surface,14));
        infoRow(card,"地区",place.isEmpty()?"未公开":place);infoRow(card,"服务",SERVICES[service]);infoRow(card,"备注",remark.isEmpty()?"未设置":remark);infoRow(card,"分组",category.isEmpty()?"未分组":category);c.addView(card);gap(c,12);
        LinearLayout tiles=row();statTile(tiles,history.size(),"条消息");statTile(tiles,stars,"条收藏");statTile(tiles,FireBuddy.streak(workspace,service,id)[0],"天连续聊天");c.addView(tiles);
        ScrollView s=new ScrollView(this);s.addView(c);new AlertDialog.Builder(this).setView(s).setNeutralButton("小火人衣橱",(d,w)->wardrobe(id)).setPositiveButton("知道了",null).show();
    }
    private void infoRow(LinearLayout card,String key,String value){
        LinearLayout r=row();r.setPadding(0,dp(9),0,dp(9));r.addView(text(key,13,MUTED,false),new LinearLayout.LayoutParams(dp(58),-2));r.addView(text(value,14,INK,false),new LinearLayout.LayoutParams(0,-2,1));
        if(card.getChildCount()>0){View rule=new View(this);rule.setBackgroundColor(LINE);card.addView(rule,new LinearLayout.LayoutParams(-1,dp(1)));}card.addView(r);
    }
    private void statTile(LinearLayout parent,int value,String caption){
        LinearLayout t=column();t.setGravity(Gravity.CENTER);t.setPadding(dp(4),dp(10),dp(4),dp(10));t.setBackground(outline(palette.surface,14));
        TextView n=text(String.valueOf(value),20,INK,true);n.setGravity(Gravity.CENTER);t.addView(n,new LinearLayout.LayoutParams(-1,-2));TextView label=text(caption,11,MUTED,false);label.setGravity(Gravity.CENTER);t.addView(label,new LinearLayout.LayoutParams(-1,-2));
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(0,-2,1);if(parent.getChildCount()>0)lp.leftMargin=dp(8);parent.addView(t,lp);t.setContentDescription(value+" "+caption);
    }
    private void groupInfo(){
        JSONObject group=workspace.groups.get(chatTarget);if(group==null)return;final int groupId=chatTarget;boolean manager=manages(group);
        LinearLayout content=column();content.setPadding(dp(20),dp(8),dp(20),0);content.addView(text(group.optString("name"),22,INK,true));content.addView(text("群号 "+groupId+"\n"+group.optString("notice","暂无公告"),14,MUTED,false));gap(content,16);
        JSONArray members=group.optJSONArray("members");if(members!=null)for(int i=0;i<members.length();i++){JSONObject member=members.optJSONObject(i);if(member==null)continue;int id=member.optInt("id");LinearLayout r=row();r.setPadding(0,dp(5),0,dp(5));r.addView(avatarUser(id,34),new LinearLayout.LayoutParams(dp(34),dp(34)));TextView name=text(workspace.name(id)+(id==group.optInt("owner")?" · 群主":Workspace.contains(group.optJSONArray("admins"),id)?" · 管理员":""),15,INK,false);name.setPadding(dp(10),0,0,0);r.addView(name);content.addView(r);}
        if(manager){gap(content,16);content.addView(text("待处理的入群申请",14,MUTED,true));JSONArray pending=group.optJSONArray("pending");if(pending!=null)for(int i=0;i<pending.length();i++){JSONArray request=pending.optJSONArray(i);if(request==null)continue;int user=request.optInt(0),contact=request.optInt(1);LinearLayout row=card(content);row.addView(text(workspace.name(user),16,INK,true));row.addView(button("同意加入",true,()->command(Wire.obj("op","group.approve","group",groupId,"target",user,"contact",contact,"enabled",true))));row.addView(button("拒绝",false,()->command(Wire.obj("op","group.approve","group",groupId,"target",user,"contact",contact,"enabled",false))));}}
        ScrollView s=new ScrollView(this);s.addView(content);new AlertDialog.Builder(this).setTitle("群资料").setView(s).setPositiveButton("关闭",null).show();
    }
    private void feed(){
        final boolean wb=channel.equals("weibo");
        if(service!=2){LinearLayout switches=row();switches.addView(button("朋友圈",!wb,()->{channel="moments";weiboInbox=false;showHome();}),new LinearLayout.LayoutParams(0,dp(43),1));LinearLayout.LayoutParams right=new LinearLayout.LayoutParams(0,dp(43),1);right.leftMargin=dp(8);switches.addView(button("微博",wb,()->{channel="weibo";showHome();}),right);body.addView(switches);gap(body,14);}
        body.addView(artwork(wb,wb?218:176));gap(body,16);
        if(wb){LinearLayout tabs=row();String[] labels={"广场","关注","我的"};for(int i=0;i<3;i++){final int filter=i;TextView chip=text(labels[i],14,weiboFilter==i?INK:MUTED,weiboFilter==i);chip.setGravity(Gravity.CENTER);chip.setBackground(UiKit.touch(weiboFilter==i?palette.soft:Color.TRANSPARENT,GREEN,13,density()));tabs.addView(chip,new LinearLayout.LayoutParams(0,dp(39),1));chip.setOnClickListener(v->{weiboFilter=filter;weiboInbox=false;showHome();});}body.addView(tabs);gap(body,12);}
        if(wb&&workspace.account(workspace.viewer,2).length()==0){empty(body,"开启你的微博空间","先在「我的」开通微博服务，再与大家分享灵感。");return;}
        LinearLayout composerCard=card(body);LinearLayout compose=row();compose.addView(avatarUser(workspace.viewer,38),new LinearLayout.LayoutParams(dp(38),dp(38)));TextView prompt=text(wb?"此刻，有什么新灵感？":"记录今天的小事…",14,MUTED,false);prompt.setPadding(dp(12),0,0,0);compose.addView(prompt,new LinearLayout.LayoutParams(0,-2,1));compose.addView(button(wb?"发微博":"发布",true,()->askText(wb?"新的灵感":"发布动态","分享此刻的想法","",value->command(Wire.obj("op","feed.publish","channel",channel,"text",value)))));composerCard.addView(compose);
        JSONArray posts=workspace.array(wb?"weibo":"feed");int shown=0;for(int i=0;i<posts.length();i++){JSONObject post=posts.optJSONObject(i);if(post==null)continue;int author=post.optInt("author");boolean following=Workspace.contains(workspace.array("follows"),author);if(wb&&((weiboFilter==1&&!following)||(weiboFilter==2&&author!=workspace.viewer)))continue;shown++;postCard(post,wb);}
        if(shown==0)empty(body,wb?(weiboFilter==1?"关注的灵感，正在路上":"让第一条灵感从你开始"):"这里还很安静",wb?"真实的公开动态会出现在这里。":"好友发布的生活片段会同步到这里。");
        if(wb){TextView note=text("伊冯灵感同人插画 · 非官方合作",10,MUTED,false);note.setGravity(Gravity.CENTER);body.addView(note);}
    }
    private void postCard(JSONObject post,boolean wb){
        int author=post.optInt("author");String id=post.optString("id");LinearLayout c=card(body);LinearLayout head=row();head.addView(avatarUser(author,42),new LinearLayout.LayoutParams(dp(42),dp(42)));LinearLayout who=column();who.setPadding(dp(11),0,dp(6),0);who.addView(text(workspace.name(author),15,INK,true));who.addView(text(clock(post.optLong("created"))+(wb?" · 公开":" · 朋友可见"),10,MUTED,false));head.addView(who,new LinearLayout.LayoutParams(0,-2,1));
        if(wb&&author!=workspace.viewer){boolean followed=Workspace.contains(workspace.array("follows"),author);Button follow=button(followed?"已关注":"＋ 关注",false,()->command(Wire.obj("op","weibo.follow","target",author,"enabled",!followed)));follow.setTextSize(11);follow.setMinHeight(dp(34));head.addView(follow,new LinearLayout.LayoutParams(-2,dp(35)));}c.addView(head);gap(c,15);TextView content=text(post.optString("text"),16,INK,false);content.setLineSpacing(dp(5),1);content.setTextIsSelectable(true);c.addView(content);
        String image=post.optString("image");if(!image.isEmpty()&&image.length()<180000)try{byte[] bytes=java.util.Base64.getDecoder().decode(image);BitmapFactory.Options options=new BitmapFactory.Options();options.inScaled=false;Bitmap bitmap=BitmapFactory.decodeByteArray(bytes,0,bytes.length,options);if(bitmap!=null){gap(c,13);ImageView view=new ImageView(this);view.setImageBitmap(bitmap);view.setScaleType(ImageView.ScaleType.CENTER_CROP);view.setBackground(bg(palette.soft,15));view.setClipToOutline(true);c.addView(view,new LinearLayout.LayoutParams(-1,dp(208)));}}catch(Exception ignored){}
        gap(c,16);View rule=new View(this);rule.setBackgroundColor(LINE);c.addView(rule,new LinearLayout.LayoutParams(-1,dp(1)));gap(c,7);LinearLayout actions=row();actions.addView(postAction("heart",(post.optBoolean("liked")?"已赞 ":"赞 ")+post.optInt("likes"),post.optBoolean("liked"),()->command(Wire.obj("op","feed.like","post",id,"enabled",!post.optBoolean("liked")))),new LinearLayout.LayoutParams(0,dp(42),1));
        actions.addView(postAction("chat","评论",false,()->askText("回应这条灵感","写下你的回应","",value->command(Wire.obj("op","feed.comment","post",id,"text",value)))),new LinearLayout.LayoutParams(0,dp(42),1));
        if(wb&&author!=workspace.viewer)actions.addView(postAction("send","私信",false,()->openDirectMessage(author,id)),new LinearLayout.LayoutParams(0,dp(42),1));c.addView(actions);
        JSONArray comments=post.optJSONArray("comments");if(comments!=null&&comments.length()>0){LinearLayout replies=column();replies.setPadding(dp(12),dp(10),dp(12),dp(10));replies.setBackground(bg(BG,12));for(int j=0;j<comments.length();j++){JSONObject comment=comments.optJSONObject(j);if(comment!=null)replies.addView(text(workspace.name(comment.optInt("author"))+"："+comment.optString("text"),12,MUTED,false));}c.addView(replies);}
    }
    private View postAction(String icon,String title,boolean active,Runnable tap){LinearLayout r=row();r.setGravity(Gravity.CENTER);r.addView(icon(icon,active?GREEN:MUTED,18));TextView label=text(title,12,active?GREEN:MUTED,active);label.setPadding(dp(6),0,0,0);r.addView(label);r.setContentDescription(title);r.setOnClickListener(v->tap.run());return r;}
    private void renderWeiboInbox(){
        LinearLayout intro=card(body);intro.addView(text("同频的人，会有回音。",20,INK,true));gap(intro,6);intro.addView(text("从公开微博发起对话，无需添加好友。",12,MUTED,false));
        Map<Integer,JSONObject> latest=new LinkedHashMap<>();JSONArray all=workspace.array("directMessages");for(int i=0;i<all.length();i++){JSONObject m=all.optJSONObject(i);if(m==null)continue;int peer=m.optInt("sender")==workspace.viewer?m.optInt("recipient"):m.optInt("sender");if(!latest.containsKey(peer))latest.put(peer,m);}
        if(latest.isEmpty()){empty(body,"还没有私信","在一条公开微博下点击「私信」，开启对话。");return;}
        for(Map.Entry<Integer,JSONObject> entry:latest.entrySet()){int peer=entry.getKey();JSONObject m=entry.getValue();boolean fresh=!m.optBoolean("read")&&m.optInt("recipient")==workspace.viewer;listRow(body,avatarUser(peer,50),workspace.name(peer),when(m.optLong("created")),m.optString("text"),0,fresh?"新私信":"",false,()->openDirectMessage(peer,""));}
    }
    private void openDirectMessage(int target,String post){
        chatKind=2;chatTarget=target;directPost=post;sending=false;lastReadBatch="";base();conversationPane=null;
        chatHeader(root,avatarUser(target,44),workspace.name(target),"微博私信 · 与此刻同频",true,null,null);connectionLabel();messageArea(root);composer(root,"发送一条私信…");renderDirectMessages();
    }
    private void renderDirectMessages(){
        chatMessages.removeAllViews();List<JSONObject> list=new ArrayList<>();JSONArray all=workspace.array("directMessages");boolean unread=false;for(int i=all.length()-1;i>=0;i--){JSONObject m=all.optJSONObject(i);if(m!=null&&(m.optInt("sender")==chatTarget||m.optInt("recipient")==chatTarget)){list.add(m);if(m.optInt("recipient")==workspace.viewer&&!m.optBoolean("read"))unread=true;}}
        String day="";for(JSONObject m:list){int sender=m.optInt("sender");day=daySeparator(chatMessages,m.optLong("created"),day);messageRow(chatMessages,sender,sender==workspace.viewer,m.optString("text"),m.optLong("created"),"",false);}
        if(list.isEmpty())note(chatMessages,"写下第一句问候，开启这段对话。");
        scroll.post(()->scroll.fullScroll(View.FOCUS_DOWN));String token=chatTarget+":"+list.size();if(unread&&!lastReadBatch.equals(token)){lastReadBatch=token;command(Wire.obj("op","weibo.dm.read","target",chatTarget));}
    }
    private void openWeiboService(){
        LinearLayout form=column();form.setPadding(dp(22),dp(8),dp(22),0);form.addView(text("为当前账号设置微博服务密码，开通后可在同一云端切换。",13,MUTED,false));gap(form,12);EditText secret=input("至少 12 字节的微博密码",true);form.addView(secret);AlertDialog d=new AlertDialog.Builder(this).setTitle("开通微博服务").setView(form).setNegativeButton("取消",null).setPositiveButton("开通",null).create();d.setOnShowListener(v->d.getButton(-1).setOnClickListener(b->{String password=secret.getText().toString();if(password.getBytes(StandardCharsets.UTF_8).length<12||password.getBytes(StandardCharsets.UTF_8).length>128){toast("密码需为 12–128 字节");return;}command(Wire.obj("op","account.open","contact",2,"number",String.valueOf(workspace.viewer),"password",password,"shared",false),()->{service=2;tab=FEED;channel="weibo";weiboInbox=false;showHome();},null);secret.setText("");d.dismiss();}));d.show();
    }

    /** Plans of desktop StudioStoreHsx::plan; the server checks tier, price and duration again. */
    private static final String[] PLAN_NAMES={"免费用户","星芽","月汐","星河","云境"};
    private static final int[] PLAN_PRICES={0,1,6,128,648},PLAN_DAYS={0,7,30,365,0};
    private static final String[] PLAN_PERKS={"","8 款气泡 · 青芽头像框 · 薄荷精灵小火人","11 款气泡 · 月汐头像框 · 樱粉小火人","13 款气泡 · 极光动效 · 星焰精灵与星河王冠","15 款全部气泡 · 鎏金动效 · 鎏金守护者"};
    private static final int STORAGE=12;
    private String membershipText(){
        JSONObject m=workspace.raw.optJSONObject("membership");int tier=m==null?0:Math.max(0,Math.min(4,m.optInt("tier")));if(tier==0)return "免费用户";
        long until=m.optLong("expires");return PLAN_NAMES[tier]+"会员 · "+(tier==4||until<=0?"永久权益":new SimpleDateFormat("yyyy-MM-dd",Locale.ROOT).format(new Date(until*1000))+" 到期");
    }
    /** 会员与充值, as on the desktop: choose a plan, pay with the WeChat code, submit the payment reference, follow the order. */
    private void membership(){
        final int tier=memberTier();if(selectedTier<Math.max(1,tier))selectedTier=Math.max(1,tier);
        LinearLayout status=card(body);status.addView(text(membershipText(),18,INK,true));gap(status,4);status.addView(text(tier==0?"免费用户也可以更换主题、头像、背景并养成小火人。":"高档包含低档的全部装扮；同档续费与升级会保留剩余时长。",12,MUTED,false));
        section(body,"选择档位",-1);
        for(int t=1;t<=4;t++){
            final int plan=t;boolean chosen=t==selectedTier,covered=tier==4||t<tier;
            LinearLayout row=row();row.setPadding(dp(14),dp(12),dp(14),dp(12));row.setBackground(UiKit.touch(chosen?palette.soft:palette.surface,GREEN,16,density()));
            LinearLayout labels=column();labels.addView(line(PLAN_NAMES[t]+(covered?" · 已拥有更高权益":""),15,covered?MUTED:INK,true));labels.addView(line(PLAN_PERKS[t],11,MUTED,false));row.addView(labels,new LinearLayout.LayoutParams(0,-2,1));
            LinearLayout price=column();price.setGravity(Gravity.END);price.addView(text("¥"+PLAN_PRICES[t],18,chosen?GREEN:INK,true));price.addView(text(t==4?"永久":PLAN_DAYS[t]+" 天",11,MUTED,false));row.addView(price);
            row.setContentDescription(PLAN_NAMES[t]+"，"+PLAN_PRICES[t]+" 元，"+(t==4?"永久":PLAN_DAYS[t]+" 天")+(chosen?"，已选择":""));
            row.setOnClickListener(v->{if(tier==4||plan<tier){toast("当前权益已覆盖该档位");return;}selectedTier=plan;int y=scroll.getScrollY();showHome();scroll.post(()->scroll.scrollTo(0,y));});
            LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);lp.bottomMargin=dp(8);body.addView(row,lp);
        }
        LinearLayout pay=card(body);pay.addView(text("微信扫码 · 管理员核验开通",16,INK,true));gap(pay,4);pay.addView(text("应付 ¥"+PLAN_PRICES[selectedTier]+" · "+PLAN_NAMES[selectedTier]+(selectedTier==4?" · 永久":" · "+PLAN_DAYS[selectedTier]+" 天"),14,GREEN,true));gap(pay,10);
        // The real code is copied in at build time from the maintainer's own assets and never kept in Git.
        final int qr=getResources().getIdentifier("payment_qr","drawable",getPackageName());
        if(qr!=0){
            ImageView image=new ImageView(this);image.setImageResource(qr);image.setAdjustViewBounds(true);image.setScaleType(ImageView.ScaleType.FIT_CENTER);image.setContentDescription("微信收款码，长按保存到相册");image.setOnLongClickListener(v->{savePaymentQr(qr);return true;});
            pay.addView(image,new LinearLayout.LayoutParams(-1,dp(320)));gap(pay,8);pay.addView(button("保存收款码到相册",false,()->savePaymentQr(qr)),new LinearLayout.LayoutParams(-1,dp(44)));gap(pay,8);
            pay.addView(text("保存后打开微信「扫一扫」，点右上角相册选择这张图片付款。请核对收款人与金额，并在备注里写上你的登录昵称。",12,MUTED,false));
        }else pay.addView(text("此安装包没有内置收款码，请向服务器维护者获取付款方式，或在电脑端「装扮与会员」页扫码付款。",12,MUTED,false));
        gap(pay,12);EditText reference=input("交易单号后 6 位 / 付款昵称与时间（至少 6 字）",false);reference.setSingleLine(true);pay.addView(reference,new LinearLayout.LayoutParams(-1,dp(52)));gap(pay,10);
        pay.addView(button("我已付款，提交核验",true,()->{
            String value=reference.getText().toString().trim();if(value.length()<6||value.length()>120){toast("请填写 6–120 字的交易单号或付款备注");return;}
            if(tier==4||selectedTier<tier){toast("当前权益已覆盖该档位，请选择同档续期或更高档位");return;}
            command(Wire.obj("op","membership.order","tier",selectedTier,"reference",value),()->{reference.setText("");toast("订单已提交，请等待管理员核验到账");},null);
        }),new LinearLayout.LayoutParams(-1,dp(48)));
        gap(pay,8);pay.addView(text("此页面不会自动扣款，也不会自动识别支付结果。付款后提交凭证，管理员核验到账后开通。",11,MUTED,false));
        JSONArray orders=workspace.array("membershipOrders");section(body,"我的订单",orders.length());
        if(orders.length()==0)empty(body,"还没有订单","提交付款凭证后，核验结果与原因会显示在这里。");
        for(int i=0;i<orders.length();i++){
            JSONObject o=orders.optJSONObject(i);if(o==null)continue;final String id=o.optString("id");String state=o.optString("status");int t=Math.max(0,Math.min(4,o.optInt("tier")));
            LinearLayout c=card(body);LinearLayout head=row();head.addView(line(PLAN_NAMES[t]+" · ¥"+o.optInt("price"),15,INK,true),new LinearLayout.LayoutParams(0,-2,1));
            head.addView(pill(state.equals("pending")?"待管理员核验":state.equals("approved")?"已核验开通":state.equals("rejected")?"已驳回":"已取消",state.equals("approved")?palette.soft:BG,state.equals("approved")?GREEN:MUTED));c.addView(head);
            c.addView(text("提交于 "+clock(o.optLong("created"))+" · 订单 "+id.substring(0,Math.min(8,id.length())),11,MUTED,false));
            if(!o.optString("note").isEmpty())c.addView(text("核验备注："+o.optString("note"),12,INK,false));
            if(state.equals("pending")){gap(c,8);c.addView(button("取消申请",false,()->new AlertDialog.Builder(this).setTitle("取消这笔开通申请？").setMessage("取消只撤销开通申请；若已转账，请联系收款人处理款项。").setNegativeButton("保留",null).setPositiveButton("取消申请",(d,w)->command(Wire.obj("op","membership.cancel","id",id),()->toast("开通申请已取消"),null)).show()),new LinearLayout.LayoutParams(-1,dp(42)));}
        }
        gap(body,6);TextView rules=text("权益规则：高档包含低档装扮；同档续费与升级保留剩余时长，云境为永久。到期后自动使用免费气泡和默认头像框，头像与背景继续保留。订单在处理前可取消。",11,MUTED,false);body.addView(rules);
    }
    /** Saves the bundled payment code to the gallery so WeChat can read it from 相册. */
    private void savePaymentQr(int id){
        if(android.os.Build.VERSION.SDK_INT<29&&checkSelfPermission(android.Manifest.permission.WRITE_EXTERNAL_STORAGE)!=android.content.pm.PackageManager.PERMISSION_GRANTED){requestPermissions(new String[]{android.Manifest.permission.WRITE_EXTERNAL_STORAGE},STORAGE);return;}
        try{
            Bitmap bitmap=BitmapFactory.decodeResource(getResources(),id);android.content.ContentValues values=new android.content.ContentValues();
            values.put(android.provider.MediaStore.Images.Media.DISPLAY_NAME,"听澜收款码.jpg");values.put(android.provider.MediaStore.Images.Media.MIME_TYPE,"image/jpeg");
            if(android.os.Build.VERSION.SDK_INT>=29)values.put(android.provider.MediaStore.Images.Media.RELATIVE_PATH,"Pictures/Tinglan");
            android.net.Uri uri=getContentResolver().insert(android.provider.MediaStore.Images.Media.EXTERNAL_CONTENT_URI,values);if(uri==null)throw new IllegalStateException("相册不可用");
            try(java.io.OutputStream out=getContentResolver().openOutputStream(uri)){if(out==null||!bitmap.compress(Bitmap.CompressFormat.JPEG,95,out))throw new IllegalStateException("写入失败");}
            toast("收款码已保存到相册，可在微信扫一扫中从相册选择");
        }catch(Exception e){toast("保存失败："+e.getMessage());}
    }
    @Override public void onRequestPermissionsResult(int code,String[] permissions,int[] results){
        super.onRequestPermissionsResult(code,permissions,results);
        if(code!=STORAGE)return;int qr=getResources().getIdentifier("payment_qr","drawable",getPackageName());
        if(results.length>0&&results[0]==android.content.pm.PackageManager.PERMISSION_GRANTED&&qr!=0)savePaymentQr(qr);else toast("需要存储权限才能保存收款码，也可以直接截屏");
    }
    /** Desktop 账号 page: profile, opened services and settings. */
    private void mine(){
        JSONObject me=workspace.users.get(workspace.viewer);LinearLayout c=card(body);LinearLayout head=row();head.addView(avatarUser(workspace.viewer,64),new LinearLayout.LayoutParams(dp(64),dp(64)));
        LinearLayout who=column();who.setPadding(dp(16),0,0,0);who.addView(line(workspace.name(workspace.viewer),23,INK,true));who.addView(line("云端 ID "+workspace.viewer+" · 当前 "+SERVICES[service],12,MUTED,false));who.addView(line(me.optString("location","未设置"),12,MUTED,false));head.addView(who,new LinearLayout.LayoutParams(0,-2,1));c.addView(head);
        gap(c,16);c.addView(text("已开通的服务",13,MUTED,true));gap(c,8);JSONArray accounts=me.optJSONArray("services");
        if(accounts!=null)for(int i=0;i<accounts.length();i++){JSONObject s=accounts.optJSONObject(i);int type=s==null?-1:s.optInt("type",-1);if(type<0||type>2)continue;boolean current=type==service;String bound=s.optString("bound","0");String relation=s.optBoolean("shared")?"与 QQ 共用":!bound.isEmpty()&&!bound.equals("0")?"绑定 "+bound:"独立账号";
            LinearLayout entry=row();entry.setPadding(dp(12),dp(10),dp(12),dp(10));entry.setBackground(bg(current?palette.soft:BG,12));entry.addView(text(SERVICES[type],14,INK,true),new LinearLayout.LayoutParams(dp(54),-2));entry.addView(line(s.optString("number")+" · "+s.optString("nick"),12,MUTED,false),new LinearLayout.LayoutParams(0,-2,1));entry.addView(text(current?"使用中":relation,11,current?GREEN:MUTED,current));
            LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);lp.bottomMargin=dp(6);c.addView(entry,lp);}
        gap(c,8);c.addView(button("切换已开通服务",false,()->{List<Integer> types=new ArrayList<>();List<String> names=new ArrayList<>();for(int s=0;s<3;s++)if(workspace.account(workspace.viewer,s).length()>0){types.add(s);names.add(SERVICES[s]);}new AlertDialog.Builder(this).setTitle("切换服务视图").setItems(names.toArray(new String[0]),(d,w)->{service=types.get(w);tab=service==2?FEED:CHATS;channel=service==2?"weibo":"moments";weiboInbox=false;weiboFilter=0;prefs.edit().putInt("service",service).apply();showHome();}).show();}));
        if(workspace.account(workspace.viewer,2).length()==0){body.addView(button("开通微博 · 伊冯灵感空间",true,this::openWeiboService));gap(body,12);}
        LinearLayout tools=row();tools.addView(tile("scan","扫一扫","扫码登录电脑端",this::scan),new LinearLayout.LayoutParams(0,-2,1));LinearLayout.LayoutParams second=new LinearLayout.LayoutParams(0,-2,1);second.leftMargin=dp(8);
        tools.addView(tile("flame","小火人衣橱","和好友一起换装",this::chooseWardrobe),second);body.addView(tools);gap(body,8);
        body.addView(tile("crown","会员与充值",membershipText()+" · 开通或续费",()->{chatKind=-1;tab=MEMBER;showHome();}));gap(body,12);
        LinearLayout settings=card(body);settings.addView(button("编辑个人资料",false,()->askText("个人资料","昵称",workspace.name(workspace.viewer),name->askText("个人资料","所在地",workspace.users.get(workspace.viewer).optString("location"),location->command(Wire.obj("op","profile","name",name,"location",location))))));gap(settings,8);settings.addView(button("我的收藏",false,this::favorites));gap(settings,8);settings.addView(button("服务器与连接",false,()->new AlertDialog.Builder(this).setTitle("当前服务器").setMessage(profile.host+":"+profile.port+"\n\nTLS 1.2 / 1.3 · 严格校验证书\n\nCA SHA-256\n"+profile.fingerprint+"\n\n切换服务器请先退出登录。密码不会保存在手机存储中；自动登录只保存可吊销的登录凭证。").setPositiveButton("知道了",null).show()));
        LinearLayout about=card(body);LinearLayout title=row();title.addView(text("听澜 Android "+appVersion(),16,INK,true),new LinearLayout.LayoutParams(0,-2,1));TextView checkNow=text("检查更新",13,GREEN,true);checkNow.setPadding(dp(10),dp(6),0,dp(6));checkNow.setOnClickListener(v->checkUpdate(true));title.addView(checkNow);about.addView(title);gap(about,8);about.addView(text("原生安卓 · 与桌面端共用云端，界面与电脑端统一。\n已支持好友、群聊、通知、动态、收藏、微博关注与私信、聊天小火人与衣橱；平板使用与电脑端相同的侧边栏和双栏会话。\n电脑和手机可以同时登录；同一账号在另一部手机登录时，本机会自动下线。可用「扫一扫」登录电脑端。\n应用在前台时保持实时连接；本版不含系统离线推送、文件传输、管理后台或支付。",13,MUTED,false));gap(body,4);body.addView(button("退出账号",false,()->new AlertDialog.Builder(this).setTitle("退出账号？").setMessage("云端数据会保留。退出后本机的自动登录失效，下次需重新输入密码。").setNegativeButton("取消",null).setPositiveButton("退出",(d,w)->signOut()).show()));
    }
    // In-app update: the server's Android feed on a separate TLS connection, then the system installer.
    private Updater updater;
    private boolean updateOffered=false;
    private java.io.File pendingApk;
    private JSONObject pendingRelease;
    private String appVersion(){try{return getPackageManager().getPackageInfo(getPackageName(),0).versionName;}catch(Exception e){return "0.0.0";}}
    private java.io.File updateFolder(){return new java.io.File(getCacheDir(),"updates");}
    /** After sign-in, at most every 6 hours; only an available update is shown. */
    private void autoUpdateCheck(){long now=System.currentTimeMillis();if(updateOffered||now-prefs.getLong("updateChecked",0)<6*3600_000L)return;prefs.edit().putLong("updateChecked",now).apply();checkUpdate(false);}
    private void checkUpdate(boolean manual){
        if(profile==null){if(manual)toast("尚未设置服务器");return;}
        if(updater!=null){if(manual)toast("正在检查更新…");return;}
        if(manual)toast("正在检查更新…");String current=appVersion();
        final Updater[] holder=new Updater[1];
        holder[0]=new Updater(profile,current,new Updater.Listener(){
            private void ui(Runnable r){runOnUiThread(()->{if(updater==holder[0])updater=null;if(!destroyed)r.run();});}
            public void current(boolean published){ui(()->{if(manual)toast(published?"已是最新版本 "+current:"服务器暂未发布安卓更新，当前版本 "+current);});}
            public void available(JSONObject release){ui(()->{if(manual||!updateOffered)offerUpdate(release);});}
            public void failed(String detail){ui(()->{if(manual)new AlertDialog.Builder(MainActivity.this).setTitle("检查更新失败").setMessage(detail).setPositiveButton("知道了",null).show();});}
        });
        updater=holder[0];updater.check();
    }
    private void offerUpdate(JSONObject release){
        updateOffered=true;String version=release.optString("version"),notes=release.optString("notes").trim();
        String size=String.format(Locale.ROOT,"%.1f MB",release.optLong("size")/1048576.0);
        new AlertDialog.Builder(this).setTitle("发现新版本 "+version).setMessage((notes.isEmpty()?"":notes+"\n\n")+"当前版本 "+appVersion()+" · 安装包 "+size+"\n从听澜服务器加密下载，校验后由系统安装；聊天记录和登录状态会保留。")
            .setNegativeButton("稍后",null).setPositiveButton("下载并安装",(d,w)->downloadUpdate(release)).show();
    }
    private void downloadUpdate(JSONObject release){
        java.io.File cached=new java.io.File(updateFolder(),Updater.apkName(release.optString("version")));
        if(Updater.matches(cached,release)){installUpdate(cached,release);return;}
        if(updater!=null){toast("正在检查更新，请稍候再试");return;}
        LinearLayout c=column();c.setPadding(dp(22),dp(12),dp(22),dp(4));ProgressBar bar=new ProgressBar(this,null,android.R.attr.progressBarStyleHorizontal);bar.setMax(1000);c.addView(bar,new LinearLayout.LayoutParams(-1,-2));
        TextView detail=text("正在连接服务器…",12,MUTED,false);c.addView(detail);
        final Updater[] holder=new Updater[1];
        AlertDialog dialog=new AlertDialog.Builder(this).setTitle("正在下载 "+release.optString("version")).setView(c).setCancelable(false).setNegativeButton("取消",(d,w)->{if(holder[0]!=null)holder[0].cancel();if(updater==holder[0])updater=null;}).show();
        holder[0]=new Updater(profile,appVersion(),new Updater.Listener(){
            private void ui(Runnable r){runOnUiThread(()->{if(!destroyed)r.run();});}
            private void finish(){if(updater==holder[0])updater=null;if(dialog.isShowing())dialog.dismiss();}
            public void progress(long received,long total){ui(()->{bar.setProgress((int)(received*1000/total));detail.setText(String.format(Locale.ROOT,"已下载 %.1f / %.1f MB",received/1048576.0,total/1048576.0));});}
            public void ready(java.io.File apk,JSONObject r){ui(()->{finish();installUpdate(apk,r);});}
            public void failed(String reason){ui(()->{finish();new AlertDialog.Builder(MainActivity.this).setTitle("更新下载失败").setMessage(reason).setPositiveButton("知道了",null).show();});}
        });
        updater=holder[0];updater.download(release,updateFolder());
    }
    private void installUpdate(java.io.File apk,JSONObject release){
        String problem=ApkInstall.verify(this,apk,release);
        if(problem!=null){apk.delete();new AlertDialog.Builder(this).setTitle("更新未安装").setMessage(problem+"。安装包已删除。").setPositiveButton("知道了",null).show();return;}
        if(!getPackageManager().canRequestPackageInstalls()){
            pendingApk=apk;pendingRelease=release;
            new AlertDialog.Builder(this).setTitle("允许听澜安装更新").setMessage("安卓要求先在系统设置里允许听澜「安装未知应用」。打开开关后返回听澜，会继续安装。").setNegativeButton("取消",(d,w)->{pendingApk=null;pendingRelease=null;})
                .setPositiveButton("去设置",(d,w)->{try{startActivity(new android.content.Intent(android.provider.Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,android.net.Uri.parse("package:"+getPackageName())));}catch(Exception e){toast("请在系统设置中允许听澜安装未知应用");}}).show();
            return;
        }
        pendingApk=null;pendingRelease=null;
        try{ApkInstall.install(this,apk);toast("请在系统弹窗中确认安装，安装后听澜会自动重启");}catch(Exception e){toast("无法启动系统安装："+e.getMessage());}
    }
    @Override protected void onResume(){
        super.onResume();
        if(pendingApk!=null&&getPackageManager().canRequestPackageInstalls()){java.io.File apk=pendingApk;JSONObject release=pendingRelease;if(Updater.matches(apk,release))installUpdate(apk,release);else{pendingApk=null;pendingRelease=null;}}
    }
    /** Downloads that are not newer than this app (already installed or abandoned) are removed. */
    private void cleanUpdates(){java.io.File[] files=updateFolder().listFiles();if(files==null)return;String current=appVersion();for(java.io.File f:files){String name=f.getName();String v=name.startsWith("Tinglan-Android-")&&name.endsWith(".apk")?name.substring(16,name.length()-4):"";if(!Updater.newer(v,current))f.delete();}}
    private void favorites(){LinearLayout content=column();content.setPadding(dp(20),0,dp(20),0);JSONArray list=workspace.array("messages");int count=0;for(int i=0;i<list.length();i++){JSONObject m=list.optJSONObject(i);if(m!=null&&!m.optBoolean("recalled")&&Workspace.contains(m.optJSONArray("stars"),workspace.viewer)){count++;LinearLayout c=card(content);c.addView(text(workspace.name(m.optInt("sender"))+" · "+clock(m.optLong("created")),12,MUTED,false));c.addView(text(m.optString("text"),16,INK,false));}}if(count==0)content.addView(text("长按聊天消息即可收藏",15,MUTED,false));ScrollView sc=new ScrollView(this);sc.addView(content);new AlertDialog.Builder(this).setTitle("我的收藏").setView(sc).setPositiveButton("关闭",null).show();}
    private void askText(String title,String hint,String current,TextAction action){LinearLayout c=column();c.setPadding(dp(22),dp(12),dp(22),0);EditText e=input(hint,false);e.setText(current);e.setSingleLine(false);e.setMaxLines(5);e.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_MULTI_LINE);c.addView(e);AlertDialog d=new AlertDialog.Builder(this).setTitle(title).setView(c).setNegativeButton("取消",null).setPositiveButton("提交",null).create();d.setOnShowListener(v->d.getButton(-1).setOnClickListener(b->{String value=e.getText().toString().trim();if(value.isEmpty()){toast("请输入内容");return;}action.accept(value);d.dismiss();}));d.show();}
    private String clock(long seconds){if(seconds<=0)return "";return new SimpleDateFormat("MM-dd HH:mm",Locale.getDefault()).format(new Date(seconds*1000));}
    private String time(long seconds){if(seconds<=0)return "";return new SimpleDateFormat("HH:mm",Locale.getDefault()).format(new Date(seconds*1000));}
    /** Conversation list time: today shows the clock, older days show the date. */
    private String when(long seconds){if(seconds<=0)return "";Date date=new Date(seconds*1000);boolean today=new SimpleDateFormat("yyyyMMdd",Locale.ROOT).format(date).equals(new SimpleDateFormat("yyyyMMdd",Locale.ROOT).format(new Date()));return new SimpleDateFormat(today?"HH:mm":"MM-dd",Locale.getDefault()).format(date);}
    private void hideKeyboard(){View view=getCurrentFocus();if(view!=null)((InputMethodManager)getSystemService(INPUT_METHOD_SERVICE)).hideSoftInputFromWindow(view.getWindowToken(),0);}
    private void toast(String detail){Toast.makeText(this,detail==null?"操作未完成":detail,Toast.LENGTH_LONG).show();}
}
