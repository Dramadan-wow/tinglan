package cn.tinglan.mobile;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.inputmethod.InputMethodManager;
import android.widget.*;
import org.json.JSONArray;
import org.json.JSONObject;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.*;

/** Native Android client. Account authority and all mutations remain on the existing server. */
public final class MainActivity extends Activity {
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
    private LinearLayout root,body,chatMessages;
    private ScrollView scroll;
    private TextView status;
    private EditText nickname,password,composer;
    private Button loginButton,sendButton;
    private int service=0,tab=0,chatKind=-1,chatTarget=0;
    private String connectionState="未连接",channel="moments";
    private boolean connecting=false,register=false,sending=false,destroyed=false;
    private final Map<String,Runnable> successes=new HashMap<>();
    private final Map<String,Runnable> completions=new HashMap<>();
    private String sendRequest="";
    private String lastReadBatch="";
    @Override public void onCreate(Bundle saved){super.onCreate(saved);prefs=getSharedPreferences("connection",MODE_PRIVATE);service=Math.max(0,Math.min(2,prefs.getInt("service",0)));try{String code=prefs.getString("code","");if(!code.isEmpty())profile=ConnectionProfile.parse(code);}catch(Exception e){profile=null;}showLogin();}
    @Override protected void onDestroy(){destroyed=true;if(client!=null)client.close();super.onDestroy();}
    @Override public void onConfigurationChanged(android.content.res.Configuration configuration){
        super.onConfigurationChanged(configuration);
        if(workspace==null){String nick=nickname==null?prefs.getString("nickname",""):nickname.getText().toString();String secret=password==null?"":password.getText().toString();showLogin();nickname.setText(nick);password.setText(secret);}
        else if(chatKind>=0){int kind=chatKind,target=chatTarget;String draft=composer==null?"":composer.getText().toString();if(kind==2)openDirectMessage(target,directPost);else openChat(kind,target);composer.setText(draft);}
        else showHome();
    }
    @Override public void onBackPressed(){if(chatKind>=0){hideKeyboard();chatKind=-1;showHome();}else if(workspace!=null)new AlertDialog.Builder(this).setTitle("离开听澜？").setMessage("聊天记录保存在服务器，应用回到后台后可能暂停连接。").setPositiveButton("回到桌面",(d,w)->moveTaskToBack(true)).setNegativeButton("取消",null).show();else super.onBackPressed();}
    private int dp(float value){return (int)(value*getResources().getDisplayMetrics().density+.5f);}
    private GradientDrawable bg(int color,float radius){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(radius));return d;}
    private GradientDrawable outline(int color,float radius){GradientDrawable d=bg(color,radius);d.setStroke(dp(1),LINE);return d;}
    private LinearLayout column(){LinearLayout v=new LinearLayout(this);v.setOrientation(LinearLayout.VERTICAL);return v;}
    private LinearLayout row(){LinearLayout v=new LinearLayout(this);v.setOrientation(LinearLayout.HORIZONTAL);v.setGravity(Gravity.CENTER_VERTICAL);return v;}
    private TextView text(String value,int size,int color,boolean bold){TextView t=new TextView(this);t.setText(value);t.setTextSize(size);t.setTextColor(color);if(bold)t.setTypeface(Typeface.DEFAULT,Typeface.BOLD);t.setLineSpacing(dp(3),1);return t;}
    private void gap(LinearLayout v,int height){Space s=new Space(this);v.addView(s,new LinearLayout.LayoutParams(1,dp(height)));}
    private Button button(String title,boolean primary,Runnable action){
        Button b=new Button(this);b.setAllCaps(false);b.setText(title);b.setTextSize(14);b.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));
        b.setTextColor(primary?palette.onAccent:blog()?0xff984570:GREEN);b.setBackground(UiKit.touch(primary?GREEN:palette.soft,GREEN,14,getResources().getDisplayMetrics().density));
        b.setStateListAnimator(null);b.setMinimumWidth(0);b.setMinWidth(0);b.setMinimumHeight(0);b.setMinHeight(dp(44));b.setPadding(dp(13),dp(7),dp(13),dp(7));b.setOnClickListener(v->action.run());return b;
    }
    private boolean blog(){return service==2||(workspace!=null&&tab==2&&channel.equals("weibo"));}
    private void applyPalette(){
        palette=blog()?UiKit.YVONNE:UiKit.RIVER;BG=palette.background;INK=palette.ink;GREEN=palette.accent;MUTED=palette.muted;LINE=palette.line;
        getTheme().applyStyle(blog()?R.style.YvonneTheme:R.style.AppTheme,true);
        getWindow().setStatusBarColor(BG);getWindow().setNavigationBarColor(BG);
    }
    private View icon(String name,int color,int size){UiKit.Icon view=new UiKit.Icon(this,name,color);view.setLayoutParams(new LinearLayout.LayoutParams(dp(size),dp(size)));return view;}
    private View iconAction(String name,String label,Runnable tap){
        FrameLayout hit=new FrameLayout(this);hit.setContentDescription(label);hit.setBackground(UiKit.touch(palette.surface,GREEN,16,getResources().getDisplayMetrics().density));
        UiKit.Icon image=new UiKit.Icon(this,name,INK);FrameLayout.LayoutParams imageSize=new FrameLayout.LayoutParams(dp(21),dp(21),Gravity.CENTER);hit.addView(image,imageSize);hit.setOnClickListener(v->tap.run());hit.setLayoutParams(new LinearLayout.LayoutParams(dp(46),dp(46)));return hit;
    }
    private TextView pill(String label,int fill,int color){TextView t=text(label,11,color,true);t.setPadding(dp(10),dp(4),dp(10),dp(4));t.setBackground(bg(fill,20));return t;}
    private void serviceStrip(LinearLayout parent,boolean login){
        LinearLayout strip=row();strip.setPadding(dp(4),dp(4),dp(4),dp(4));strip.setBackground(bg(palette.soft,17));
        for(int i=0;i<3;i++){final int selected=i;TextView item=text(SERVICES[i],13,service==i?INK:MUTED,service==i);item.setGravity(Gravity.CENTER);item.setBackground(UiKit.touch(service==i?palette.surface:Color.TRANSPARENT,GREEN,13,getResources().getDisplayMetrics().density));item.setContentDescription(SERVICES[i]+(service==i?"，当前服务":""));LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(0,dp(37),1);strip.addView(item,lp);
            item.setOnClickListener(v->{if(connecting)return;if(login){if(nickname!=null)prefs.edit().putString("nickname",nickname.getText().toString().trim()).apply();service=selected;prefs.edit().putInt("service",service).apply();showLogin();}
                else if(workspace.account(workspace.viewer,selected).length()>0){service=selected;weiboInbox=false;weiboFilter=0;channel=selected==2?"weibo":"moments";tab=selected==2?2:0;prefs.edit().putInt("service",service).apply();showHome();}else toast("当前账号尚未开通"+SERVICES[selected]+"，可在「我的」开通微博");});
        }parent.addView(strip,new LinearLayout.LayoutParams(-1,dp(45)));
    }
    private EditText input(String hint,boolean secret){
        EditText e=new EditText(this);e.setHint(hint);e.setTextSize(15);e.setSingleLine(true);e.setTextColor(INK);e.setHintTextColor(MUTED);e.setHighlightColor(palette.soft);e.setPadding(dp(15),dp(13),dp(15),dp(13));e.setBackground(outline(BG,14));
        e.setInputType(InputType.TYPE_CLASS_TEXT|(secret?InputType.TYPE_TEXT_VARIATION_PASSWORD:InputType.TYPE_TEXT_FLAG_CAP_SENTENCES));e.setSaveEnabled(false);e.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO);return e;
    }
    private void field(LinearLayout parent,String label,EditText e){parent.addView(text(label,13,MUTED,false));gap(parent,8);parent.addView(e,new LinearLayout.LayoutParams(-1,dp(54)));gap(parent,18);}
    private LinearLayout card(LinearLayout parent){
        LinearLayout c=column();c.setPadding(dp(19),dp(18),dp(19),dp(18));c.setBackground(outline(palette.surface,22));c.setElevation(dp(.5f));LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);lp.bottomMargin=dp(12);parent.addView(c,lp);return c;
    }
    private void base(){
        applyPalette();pageFrame=new FrameLayout(this);pageFrame.setBackgroundColor(BG);root=column();root.setPadding(dp(20),dp(10),dp(20),dp(8));
        int available=getResources().getDisplayMetrics().widthPixels;FrameLayout.LayoutParams fit=new FrameLayout.LayoutParams(Math.min(available,dp(workspace==null?940:860)),-1,Gravity.TOP|Gravity.CENTER_HORIZONTAL);pageFrame.addView(root,fit);setContentView(pageFrame);
        pageFrame.setOnApplyWindowInsetsListener((v,insets)->{root.setPadding(dp(20),dp(10)+insets.getSystemWindowInsetTop(),dp(20),dp(8)+insets.getSystemWindowInsetBottom());return insets.consumeSystemWindowInsets();});
    }
    private void makeScroll(){scroll=new ScrollView(this);scroll.setFillViewport(true);scroll.setClipToPadding(false);scroll.setVerticalScrollBarEnabled(false);body=column();body.setPadding(0,dp(6),0,dp(18));scroll.addView(body,new ScrollView.LayoutParams(-1,-2));root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));}
    private void top(String title,String sub,String action,Runnable tap){
        LinearLayout h=row();h.setPadding(0,dp(4),0,dp(4));LinearLayout labels=column();TextView small=text(blog()?"YVONNE / TINGLAN":"TINGLAN / STAY CONNECTED",9,MUTED,true);small.setLetterSpacing(.13f);labels.addView(small);labels.addView(text(title,27,INK,true));
        if(sub!=null&&!sub.isEmpty())labels.addView(text(sub,11,MUTED,false));h.addView(labels,new LinearLayout.LayoutParams(0,-2,1));
        if(action!=null){String glyph=action.equals("返回")?"back":action.equals("添加")?"plus":action.contains("连接")||action.equals("服务器")?"globe":action.equals("私信")?"mail":"refresh";h.addView(iconAction(glyph,action,tap));}root.addView(h);gap(root,12);
    }
    private void connectionLabel(){
        status=text(connectionState,11,MUTED,false);status.setPadding(dp(3),0,0,dp(7));root.addView(status);
    }
    private void navigation(){
        LinearLayout nav=row();nav.setPadding(dp(4),dp(8),dp(4),dp(2));nav.setBackground(bg(palette.surface,24));
        String[] names=service==2?new String[]{"广场","关注","私信","我的"}:new String[]{"消息","联系人","发现","我的"};
        String[] glyphs=service==2?new String[]{"compass","heart","mail","person"}:new String[]{"chat","users","compass","person"};
        int current=service==2?(tab==3?3:weiboInbox?2:weiboFilter==1?1:0):tab;
        for(int i=0;i<4;i++){final int index=i;boolean active=current==i;LinearLayout item=column();item.setGravity(Gravity.CENTER);item.setContentDescription(names[i]);
            FrameLayout capsule=new FrameLayout(this);capsule.setBackground(bg(active?palette.soft:Color.TRANSPARENT,14));UiKit.Icon glyph=new UiKit.Icon(this,glyphs[i],active?GREEN:MUTED);capsule.addView(glyph,new FrameLayout.LayoutParams(dp(22),dp(22),Gravity.CENTER));item.addView(capsule,new LinearLayout.LayoutParams(dp(52),dp(31)));TextView label=text(names[i],10,active?INK:MUTED,active);label.setGravity(Gravity.CENTER);item.addView(label);nav.addView(item,new LinearLayout.LayoutParams(0,dp(55),1));
            item.setOnClickListener(v->{if(service==2){tab=index==3?3:2;channel="weibo";weiboInbox=index==2;weiboFilter=index==1?1:0;}else tab=index;showHome();});
        }root.addView(nav,new LinearLayout.LayoutParams(-1,dp(65)));
    }
    private View artwork(boolean yvonne,int height){UiKit.Artwork view=new UiKit.Artwork(this,yvonne);view.setLayoutParams(new LinearLayout.LayoutParams(-1,dp(height)));return view;}

    private void showLogin(){
        base();top("听澜",null,"服务器",this::connectionDialog);makeScroll();boolean wide=getResources().getDisplayMetrics().widthPixels/getResources().getDisplayMetrics().density>=650;
        LinearLayout host=wide?row():column();if(wide)host.setGravity(Gravity.CENTER);body.addView(host,new LinearLayout.LayoutParams(-1,wide?dp(590):-2));
        LinearLayout art=column();art.addView(artwork(service==2,wide?430:202));if(wide){gap(art,18);art.addView(text(service==2?"今天的灵感，值得被看见。":"与你同频，也与你同在。",19,INK,true));}
        LinearLayout.LayoutParams artSize=new LinearLayout.LayoutParams(wide?0:-1,-2,wide?1:0);if(wide)artSize.rightMargin=dp(28);host.addView(art,artSize);
        LinearLayout form=column();if(!wide)gap(form,18);LinearLayout card=card(form);card.addView(text(register?"开启新的相遇":"欢迎回来",23,INK,true));gap(card,5);card.addView(text(service==2?"伊冯灵感 · 记录属于你的频率":"选择服务，继续你的云端会话。",12,MUTED,false));gap(card,20);serviceStrip(card,true);gap(card,21);
        nickname=input("注册时的昵称",false);nickname.setText(prefs.getString("nickname",""));password=input(register?"设置至少 8 字节的密码":"输入账号密码",true);field(card,"昵称",nickname);field(card,"密码",password);
        loginButton=button(register?"创建账号并登录":"进入听澜  →",true,this::login);card.addView(loginButton,new LinearLayout.LayoutParams(-1,dp(50)));gap(card,10);
        TextView mode=text(register?"已有账号？  返回登录":"还没有账号？  创建账号",13,GREEN,true);mode.setGravity(Gravity.CENTER);mode.setPadding(0,dp(10),0,dp(6));mode.setOnClickListener(v->{if(!connecting){prefs.edit().putString("nickname",nickname.getText().toString().trim()).apply();register=!register;showLogin();}});card.addView(mode);
        status=text(connecting?connectionState:profile==null?"尚未设置服务器 · 点击右上角连接":"已保存服务器 · "+profile.host,11,MUTED,false);status.setGravity(Gravity.CENTER);form.addView(status);gap(form,10);TextView privacy=text("密码只用于本次云端认证，不保存到平板。",11,MUTED,false);privacy.setGravity(Gravity.CENTER);form.addView(privacy);host.addView(form,new LinearLayout.LayoutParams(wide?0:-1,-2,wide?1:0));
        loginButton.setEnabled(!connecting);nickname.setEnabled(!connecting);password.setEnabled(!connecting);
    }
    private void connectionDialog(){
        if(connecting||workspace!=null){toast("请先退出账号再更换服务器");return;}
        LinearLayout c=column();c.setPadding(dp(22),dp(12),dp(22),0);c.addView(text("粘贴电脑端或维护者提供的完整连接码。",14,MUTED,false));gap(c,12);
        EditText code=input("TINGLAN1.…",false);code.setSingleLine(false);code.setMinLines(3);code.setMaxLines(6);code.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_MULTI_LINE);c.addView(code);
        gap(c,8);c.addView(button("从剪贴板粘贴",false,()->{ClipboardManager clipboard=(ClipboardManager)getSystemService(CLIPBOARD_SERVICE);if(clipboard.hasPrimaryClip()){ClipData clip=clipboard.getPrimaryClip();if(clip!=null&&clip.getItemCount()>0)code.setText(clip.getItemAt(0).coerceToText(this));}}));
        if(profile!=null){gap(c,10);c.addView(text("当前："+profile.host+":"+profile.port+"\nCA SHA-256："+profile.fingerprint,11,MUTED,false));}
        AlertDialog dialog=new AlertDialog.Builder(this).setTitle("服务器连接").setView(c).setNegativeButton("取消",null).setPositiveButton("检查连接码",null).create();
        dialog.setOnShowListener(d->dialog.getButton(-1).setOnClickListener(v->{try{String raw=code.getText().toString().trim();ConnectionProfile parsed=ConnectionProfile.parse(raw);new AlertDialog.Builder(this).setTitle("确认服务器与证书").setMessage(parsed.host+":"+parsed.port+"\n\nCA SHA-256\n"+parsed.fingerprint+"\n\n请确认连接码来自你的服务器维护者。").setNegativeButton("取消",null).setPositiveButton("信任并保存",(d2,w)->{profile=parsed;prefs.edit().putString("code",raw).apply();dialog.dismiss();showLogin();}).show();}catch(Exception e){toast(e.getMessage());}}));dialog.show();
    }
    private void login(){
        if(profile==null){connectionDialog();return;}String nick=nickname.getText().toString().trim(),secret=password.getText().toString();int bytes=secret.getBytes(StandardCharsets.UTF_8).length;
        if(nick.isEmpty()||nick.getBytes(StandardCharsets.UTF_8).length>60||bytes<1||bytes>128||(register&&bytes<8)){toast(register?"昵称不能为空，密码需为 8–128 字节":"请输入昵称和密码（密码最多 128 字节）");return;}
        prefs.edit().putString("nickname",nick).putInt("service",service).apply();connecting=true;password.setText("");nickname.setEnabled(false);password.setEnabled(false);loginButton.setEnabled(false);
        final ChatClient[] holder=new ChatClient[1];
        ChatClient next=new ChatClient(profile,nick,secret,service,register,new ChatClient.Listener(){
            private void ui(Runnable r){runOnUiThread(()->{if(!destroyed&&client==holder[0])r.run();});}
            public void state(String detail,boolean online){ui(()->{connectionState=detail;if(status!=null)status.setText(detail);if(sendButton!=null)sendButton.setEnabled(online&&!sending);});}
            public void snapshot(JSONObject data){ui(()->{try{boolean first=workspace==null;int position=scroll==null?0:scroll.getScrollY();workspace=new Workspace(data);connecting=false;register=false;if(first){tab=service==2?2:0;channel=service==2?"weibo":"moments";weiboInbox=false;weiboFilter=0;}String name=workspace.name(workspace.viewer);prefs.edit().putString("nickname",name).apply();if(workspace.account(workspace.viewer,service).length()==0){JSONArray all=workspace.users.get(workspace.viewer).getJSONArray("services");service=all.getJSONObject(0).getInt("type");}if(chatKind>=0)renderMessages();else{showHome();if(!first)scroll.post(()->scroll.scrollTo(0,position));}}catch(Exception e){logout();toast("同步失败："+e.getMessage());}});}
            public void result(String id,JSONObject result){ui(()->{Runnable done=completions.remove(id);if(done!=null)done.run();Runnable good=successes.remove(id);if(result.optBoolean("ok")){if(good!=null)good.run();}else toast(result.optString("detail","服务器拒绝操作"));});}
            public void failure(String detail,boolean terminal){ui(()->{if(terminal){logout();new AlertDialog.Builder(MainActivity.this).setTitle("连接未完成").setMessage(detail).setPositiveButton("知道了",null).show();}else toast(detail);});}
        });holder[0]=next;client=next;next.start();
    }
    private void logout(){if(client!=null){client.close();client=null;}workspace=null;connecting=false;sending=false;chatKind=-1;sendRequest="";composer=null;sendButton=null;successes.clear();completions.clear();showLogin();}
    private String command(JSONObject data,Runnable good,Runnable done){try{if(client==null)throw new IllegalStateException("请先登录");String id=client.command(data);if(good!=null)successes.put(id,good);if(done!=null)completions.put(id,done);return id;}catch(Exception e){toast(e.getMessage());if(done!=null)done.run();return "";}}
    private void command(JSONObject data){command(data,null,null);}
    private void showHome(){
        if(workspace==null){showLogin();return;}chatKind=-1;composer=null;sendButton=null;base();
        String[] names={"消息","通讯录","发现","我的"};String title=service==2?(tab==3?"我的空间":weiboInbox?"微博私信":weiboFilter==1?"我的关注":"微博"):names[tab];
        top(title,null,tab==1?"添加":service==2&&!weiboInbox&&tab!=3?"私信":"刷新",()->{if(tab==1)addFriend();else if(service==2&&!weiboInbox&&tab!=3){tab=2;weiboInbox=true;showHome();}else command(Wire.obj("op","sync"));});
        serviceStrip(root,false);gap(root,9);connectionLabel();makeScroll();if(tab==0)conversations();else if(tab==1)contacts();else if(tab==2){if(weiboInbox&&channel.equals("weibo"))renderWeiboInbox();else feed();}else mine();navigation();
    }
    private TextView avatar(String title,boolean group){TextView a=text(title.isEmpty()?"澜":title.substring(0,1),20,GREEN,true);a.setGravity(Gravity.CENTER);a.setBackground(bg(palette.soft,17));return a;}
    private View avatarUser(int id,int size){
        String encoded=workspace==null?"":workspace.raw.optJSONObject("appearances")==null?"":workspace.raw.optJSONObject("appearances").optJSONObject(String.valueOf(id))==null?"":workspace.raw.optJSONObject("appearances").optJSONObject(String.valueOf(id)).optString("avatar");
        if(!encoded.isEmpty()&&encoded.length()<180000)try{byte[] bytes=java.util.Base64.getDecoder().decode(encoded);BitmapFactory.Options options=new BitmapFactory.Options();options.inScaled=false;options.inSampleSize=2;Bitmap bitmap=BitmapFactory.decodeByteArray(bytes,0,bytes.length,options);if(bitmap!=null){ImageView image=new ImageView(this);image.setImageBitmap(bitmap);image.setScaleType(ImageView.ScaleType.CENTER_CROP);image.setBackground(bg(palette.soft,16));image.setClipToOutline(true);return image;}}catch(Exception ignored){}
        return avatar(workspace==null?"澜":workspace.name(id),false);
    }
    private void personRow(LinearLayout parent,String title,String subtitle,boolean group,String badge,Runnable action){
        LinearLayout r=row();r.setPadding(dp(15),dp(15),dp(15),dp(15));r.setBackground(UiKit.touch(palette.surface,GREEN,20,getResources().getDisplayMetrics().density));r.addView(avatar(title,group),new LinearLayout.LayoutParams(dp(48),dp(48)));LinearLayout labels=column();labels.setPadding(dp(13),0,dp(8),0);labels.addView(text(title,16,INK,true));gap(labels,4);TextView sub=text(subtitle,12,MUTED,false);sub.setMaxLines(1);sub.setEllipsize(android.text.TextUtils.TruncateAt.END);labels.addView(sub);r.addView(labels,new LinearLayout.LayoutParams(0,-2,1));
        if(!badge.isEmpty())r.addView(badge.contains("未读")?pill(badge,GREEN,palette.onAccent):text(badge,10,MUTED,false));LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);lp.bottomMargin=dp(8);parent.addView(r,lp);r.setOnClickListener(v->action.run());
    }
    private void empty(String title,String detail){
        LinearLayout c=column();c.setGravity(Gravity.CENTER);c.setPadding(dp(20),dp(30),dp(20),dp(28));FrameLayout circle=new FrameLayout(this);circle.setBackground(bg(palette.soft,32));UiKit.Icon glyph=new UiKit.Icon(this,blog()?"compass":"chat",GREEN);circle.addView(glyph,new FrameLayout.LayoutParams(dp(28),dp(28),Gravity.CENTER));c.addView(circle,new LinearLayout.LayoutParams(dp(64),dp(64)));gap(c,16);c.addView(text(title,18,INK,true));gap(c,8);TextView hint=text(detail,13,MUTED,false);hint.setGravity(Gravity.CENTER);c.addView(hint);body.addView(c,new LinearLayout.LayoutParams(-1,-2));
    }

    private void conversations(){List<Workspace.Conversation> all=workspace.conversations(service);if(all.isEmpty()){empty("还没有会话","去通讯录添加好友，或创建你的第一个群聊。");return;}for(Workspace.Conversation c:all)personRow(body,c.title,c.preview,c.kind==1,c.unread>0?c.unread+" 条未读":clock(c.time),()->openChat(c.kind,c.target));}
    private void contacts(){
        LinearLayout tools=row();tools.addView(button("好友申请",false,this::requests),new LinearLayout.LayoutParams(0,dp(46),1));LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(0,dp(46),1);lp.leftMargin=dp(8);tools.addView(button("创建群聊",false,this::createGroup),lp);body.addView(tools);gap(body,18);body.addView(text("我的好友",14,MUTED,true));gap(body,10);
        JSONArray friends=workspace.account(workspace.viewer,service).optJSONArray("friends");int count=friends==null?0:friends.length();if(count==0)body.addView(text("好友列表为空，点击右上角添加好友。",13,MUTED,false));
        for(int i=0;i<count;i++){JSONObject f=friends.optJSONObject(i);if(f==null)continue;int id=f.optInt("id");JSONObject u=workspace.users.get(id);personRow(body,workspace.display(id,service),"ID "+id+" · "+(u!=null&&u.optBoolean("online")?"在线":"离线")+" · "+SERVICES[Math.max(0,Math.min(2,f.optInt("contact")))],false,"聊天",()->openChat(0,id));}
        gap(body,18);body.addView(text("群聊",14,MUTED,true));gap(body,10);for(JSONObject g:workspace.groups.values()){int id=g.optInt("id");boolean joined=workspace.member(g,service);personRow(body,g.optString("name"),"群号 "+id+" · "+(joined?"已加入":"可申请加入"),true,joined?"聊天":"申请",()->{if(joined)openChat(1,id);else new AlertDialog.Builder(this).setTitle("申请加入此群？").setMessage(g.optString("name")).setNegativeButton("取消",null).setPositiveButton("申请",(d,w)->command(Wire.obj("op","group.apply","group",id))).show();});}
    }
    private void addFriend(){
        List<JSONObject> candidates=new ArrayList<>();for(JSONObject u:workspace.users.values())if(u.optInt("id")!=workspace.viewer)candidates.add(u);
        if(candidates.isEmpty()){toast("当前服务器还没有其他用户");return;}String[] names=new String[candidates.size()];for(int i=0;i<names.length;i++)names[i]=candidates.get(i).optString("name")+"  ·  ID "+candidates.get(i).optInt("id");
        new AlertDialog.Builder(this).setTitle("添加云端用户").setItems(names,(d,w)->{JSONObject u=candidates.get(w);JSONArray accounts=u.optJSONArray("services");List<Integer> types=new ArrayList<>();List<String> labels=new ArrayList<>();if(accounts!=null)for(int i=0;i<accounts.length();i++){JSONObject s=accounts.optJSONObject(i);int t=s==null?-1:s.optInt("type",-1);if(t>=0&&t<3){types.add(t);labels.add(SERVICES[t]+" · "+s.optString("nick"));}}new AlertDialog.Builder(this).setTitle("选择对方的服务").setItems(labels.toArray(new String[0]),(d2,w2)->askText("好友申请","写一句验证信息","你好，我是 "+workspace.name(workspace.viewer),reason->command(Wire.obj("op","friend.request","target",u.optInt("id"),"contact",types.get(w2),"reason",reason)))).show();}).show();
    }
    private void requests(){LinearLayout content=column();content.setPadding(dp(20),0,dp(20),0);JSONArray list=workspace.array("requests");int found=0;
        for(int i=0;i<list.length();i++){JSONObject r=list.optJSONObject(i);if(r==null||r.optInt("status",-1)!=0)continue;boolean incoming=r.optInt("to")==workspace.viewer&&r.optInt("destination")==service;boolean outgoing=r.optInt("from")==workspace.viewer&&r.optInt("source")==service;if(!incoming&&!outgoing)continue;found++;LinearLayout c=card(content);c.addView(text(workspace.name(r.optInt(incoming?"from":"to")),16,INK,true));c.addView(text(r.optString("reason"),13,MUTED,false));String id=r.optString("id");if(incoming){c.addView(button("接受",true,()->{command(Wire.obj("op","friend.respond","message",id,"enabled",true));toast("已提交，请关闭窗口查看同步结果");}));c.addView(button("拒绝",false,()->command(Wire.obj("op","friend.respond","message",id,"enabled",false))));}else c.addView(button("撤销申请",false,()->command(Wire.obj("op","friend.cancel","message",id))));}
        if(found==0)content.addView(text("暂无待处理的好友申请",15,MUTED,false));ScrollView s=new ScrollView(this);s.addView(content);new AlertDialog.Builder(this).setTitle("好友申请").setView(s).setPositiveButton("关闭",null).show();}
    private void createGroup(){askText("创建群聊","输入群名称","",name->command(Wire.obj("op","group.create","name",name,"mode",service==1?1:0,"parent",0)));}
    private void openChat(int kind,int target){
        chatKind=kind;chatTarget=target;sending=false;lastReadBatch="";base();String title=kind==0?workspace.display(target,service):workspace.groups.get(target).optString("name");top(title,kind==0?"私聊 · "+SERVICES[service]:"群聊 · "+SERVICES[service],"返回",()->{hideKeyboard();chatKind=-1;showHome();});status=text(connectionState,12,MUTED,false);root.addView(status);gap(root,8);
        if(kind==1){root.addView(button("群公告、成员与入群申请",false,this::groupInfo));gap(root,8);}
        scroll=new ScrollView(this);scroll.setFillViewport(true);chatMessages=column();chatMessages.setPadding(0,dp(8),0,dp(12));scroll.addView(chatMessages);root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        LinearLayout compose=row();composer=input("说点什么…",false);composer.setSingleLine(false);composer.setMaxLines(4);composer.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_MULTI_LINE|InputType.TYPE_TEXT_FLAG_CAP_SENTENCES);compose.addView(composer,new LinearLayout.LayoutParams(0,-2,1));sendButton=button("发送",true,this::send);LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(dp(68),dp(50));lp.leftMargin=dp(8);compose.addView(sendButton,lp);root.addView(compose);gap(root,4);renderMessages();}
    private void renderMessages(){
        if(workspace==null||chatKind<0||chatMessages==null)return;if(chatKind==2){renderDirectMessages();return;}List<JSONObject> all=workspace.history(service,chatKind,chatTarget);boolean nearBottom=scroll.getChildAt(0).getHeight()-scroll.getHeight()-scroll.getScrollY()<dp(100);chatMessages.removeAllViews();JSONArray unread=new JSONArray();
        if(all.size()>300)chatMessages.addView(text("显示最近 300 条消息，完整记录仍保存在服务器",11,MUTED,false));
        for(int i=Math.max(0,all.size()-300);i<all.size();i++){JSONObject m=all.get(i);boolean own=m.optInt("sender")==workspace.viewer;LinearLayout wrap=column();wrap.setGravity(own?Gravity.RIGHT:Gravity.LEFT);TextView name=text((own?"我":workspace.name(m.optInt("sender")))+" · "+clock(m.optLong("created")),11,MUTED,false);wrap.addView(name);gap(wrap,4);TextView bubble=text(m.optBoolean("recalled")?"消息已撤回":m.optString("text"),16,own?palette.onAccent:INK,false);bubble.setMaxWidth(dp(285));bubble.setPadding(dp(15),dp(11),dp(15),dp(11));bubble.setBackground(bg(own?GREEN:palette.surface,18));bubble.setTextIsSelectable(true);wrap.addView(bubble);gap(wrap,14);chatMessages.addView(wrap);bubble.setOnLongClickListener(v->{messageActions(m);return true;});if(!own&&!m.optBoolean("recalled")&&!Workspace.contains(m.optJSONArray("read"),workspace.viewer))unread.put(m.optString("id"));}
        if(all.isEmpty()){TextView t=text("从这一句问候开始",13,MUTED,false);t.setGravity(Gravity.CENTER);chatMessages.addView(t);}
        if(nearBottom||all.size()<5)scroll.post(()->scroll.fullScroll(View.FOCUS_DOWN));String batch=unread.toString();if(unread.length()>0&&!batch.equals(lastReadBatch)&&client!=null&&client.isOnline()){lastReadBatch=batch;command(Wire.obj("op","chat.read","messages",unread));}
    }
    private void send(){
        if(composer==null||sending)return;String value=composer.getText().toString();if(value.trim().isEmpty())return;int limit=chatKind==2?2000:5500;if(value.getBytes(StandardCharsets.UTF_8).length>limit){toast("内容过长，请缩短后再发送");return;}
        sending=true;sendButton.setEnabled(false);EditText editor=composer;Button action=sendButton;JSONObject data=chatKind==2?Wire.obj("op","weibo.dm.send","target",chatTarget,"post",directPost,"text",value):Wire.obj("op","chat.send","kind",chatKind,"target",chatTarget,"text",value);
        sendRequest=command(data,()->{if(editor.getText().toString().equals(value))editor.setText("");},()->{sending=false;if(action==sendButton)action.setEnabled(client!=null&&client.isOnline());});
    }
    private void messageActions(JSONObject m){if(m.optBoolean("recalled"))return;List<String> actions=new ArrayList<>();actions.add("复制文字");actions.add(Workspace.contains(m.optJSONArray("stars"),workspace.viewer)?"取消收藏":"收藏消息");if(m.optInt("sender")==workspace.viewer)actions.add("撤回消息");new AlertDialog.Builder(this).setItems(actions.toArray(new String[0]),(d,w)->{if(w==0){((ClipboardManager)getSystemService(CLIPBOARD_SERVICE)).setPrimaryClip(ClipData.newPlainText("听澜消息",m.optString("text")));toast("已复制");}else if(w==1)command(Wire.obj("op","chat.star","message",m.optString("id"),"enabled",!Workspace.contains(m.optJSONArray("stars"),workspace.viewer)));else command(Wire.obj("op","chat.recall","message",m.optString("id")));}).show();}
    private void groupInfo(){
        JSONObject group=workspace.groups.get(chatTarget);if(group==null)return;final int groupId=chatTarget;boolean manager=group.optInt("owner")==workspace.viewer||Workspace.contains(group.optJSONArray("admins"),workspace.viewer);
        LinearLayout content=column();content.setPadding(dp(20),dp(8),dp(20),0);content.addView(text(group.optString("name"),22,INK,true));content.addView(text("群号 "+groupId+"\n"+group.optString("notice","暂无公告"),14,MUTED,false));gap(content,16);
        JSONArray members=group.optJSONArray("members");if(members!=null)for(int i=0;i<members.length();i++){JSONObject member=members.optJSONObject(i);if(member!=null)content.addView(text(workspace.name(member.optInt("id"))+(member.optInt("id")==group.optInt("owner")?" · 群主":""),15,INK,false));}
        if(manager){gap(content,16);content.addView(text("待处理的入群申请",14,MUTED,true));JSONArray pending=group.optJSONArray("pending");if(pending!=null)for(int i=0;i<pending.length();i++){JSONArray request=pending.optJSONArray(i);if(request==null)continue;int user=request.optInt(0),contact=request.optInt(1);LinearLayout row=card(content);row.addView(text(workspace.name(user),16,INK,true));row.addView(button("同意加入",true,()->command(Wire.obj("op","group.approve","group",groupId,"target",user,"contact",contact,"enabled",true))));row.addView(button("拒绝",false,()->command(Wire.obj("op","group.approve","group",groupId,"target",user,"contact",contact,"enabled",false))));}}
        ScrollView s=new ScrollView(this);s.addView(content);new AlertDialog.Builder(this).setTitle("群资料").setView(s).setPositiveButton("关闭",null).show();
    }
    private void feed(){
        final boolean wb=channel.equals("weibo");
        if(service!=2){LinearLayout switches=row();switches.addView(button("朋友圈",!wb,()->{channel="moments";weiboInbox=false;showHome();}),new LinearLayout.LayoutParams(0,dp(43),1));LinearLayout.LayoutParams right=new LinearLayout.LayoutParams(0,dp(43),1);right.leftMargin=dp(8);switches.addView(button("微博",wb,()->{channel="weibo";showHome();}),right);body.addView(switches);gap(body,14);}
        body.addView(artwork(wb,wb?218:176));gap(body,16);
        if(wb){LinearLayout tabs=row();String[] labels={"广场","关注","我的"};for(int i=0;i<3;i++){final int filter=i;TextView chip=text(labels[i],14,weiboFilter==i?INK:MUTED,weiboFilter==i);chip.setGravity(Gravity.CENTER);chip.setBackground(UiKit.touch(weiboFilter==i?palette.soft:Color.TRANSPARENT,GREEN,13,getResources().getDisplayMetrics().density));tabs.addView(chip,new LinearLayout.LayoutParams(0,dp(39),1));chip.setOnClickListener(v->{weiboFilter=filter;weiboInbox=false;showHome();});}body.addView(tabs);gap(body,12);}
        if(wb&&workspace.account(workspace.viewer,2).length()==0){empty("开启你的微博空间","先在「我的」开通微博服务，再与大家分享灵感。");return;}
        LinearLayout composerCard=card(body);LinearLayout compose=row();compose.addView(avatarUser(workspace.viewer,38),new LinearLayout.LayoutParams(dp(38),dp(38)));TextView prompt=text(wb?"此刻，有什么新灵感？":"记录今天的小事…",14,MUTED,false);prompt.setPadding(dp(12),0,0,0);compose.addView(prompt,new LinearLayout.LayoutParams(0,-2,1));compose.addView(button(wb?"发微博":"发布",true,()->askText(wb?"新的灵感":"发布动态","分享此刻的想法","",value->command(Wire.obj("op","feed.publish","channel",channel,"text",value)))));composerCard.addView(compose);
        JSONArray posts=workspace.array(wb?"weibo":"feed");int shown=0;for(int i=0;i<posts.length();i++){JSONObject post=posts.optJSONObject(i);if(post==null)continue;int author=post.optInt("author");boolean following=Workspace.contains(workspace.array("follows"),author);if(wb&&((weiboFilter==1&&!following)||(weiboFilter==2&&author!=workspace.viewer)))continue;shown++;postCard(post,wb);}
        if(shown==0)empty(wb?(weiboFilter==1?"关注的灵感，正在路上":"让第一条灵感从你开始"):"这里还很安静",wb?"真实的公开动态会出现在这里。":"好友发布的生活片段会同步到这里。");
        if(wb){TextView note=text("伊冯灵感同人插画 · 非官方合作",10,MUTED,false);note.setGravity(Gravity.CENTER);body.addView(note);}
    }
    private void postCard(JSONObject post,boolean wb){
        int author=post.optInt("author");String id=post.optString("id");LinearLayout c=card(body);LinearLayout head=row();head.addView(avatarUser(author,42),new LinearLayout.LayoutParams(dp(42),dp(42)));LinearLayout who=column();who.setPadding(dp(11),0,dp(6),0);who.addView(text(workspace.name(author),15,INK,true));who.addView(text(clock(post.optLong("created"))+(wb?" · 公开":" · 朋友可见"),10,MUTED,false));head.addView(who,new LinearLayout.LayoutParams(0,-2,1));
        if(wb&&author!=workspace.viewer){boolean followed=Workspace.contains(workspace.array("follows"),author);Button follow=button(followed?"已关注":"＋ 关注",false,()->command(Wire.obj("op","weibo.follow","target",author,"enabled",!followed)));follow.setTextSize(11);follow.setMinHeight(dp(34));head.addView(follow,new LinearLayout.LayoutParams(-2,dp(35)));}c.addView(head);gap(c,15);TextView content=text(post.optString("text"),16,INK,false);content.setLineSpacing(dp(5),1);content.setTextIsSelectable(true);c.addView(content);
        String image=post.optString("image");if(!image.isEmpty()&&image.length()<180000)try{byte[] bytes=java.util.Base64.getDecoder().decode(image);BitmapFactory.Options options=new BitmapFactory.Options();options.inScaled=false;Bitmap bitmap=BitmapFactory.decodeByteArray(bytes,0,bytes.length,options);if(bitmap!=null){gap(c,13);ImageView view=new ImageView(this);view.setImageBitmap(bitmap);view.setScaleType(ImageView.ScaleType.CENTER_CROP);view.setBackground(bg(palette.soft,15));view.setClipToOutline(true);c.addView(view,new LinearLayout.LayoutParams(-1,dp(208)));}}catch(Exception ignored){}
        gap(c,16);View line=new View(this);line.setBackgroundColor(LINE);c.addView(line,new LinearLayout.LayoutParams(-1,dp(1)));gap(c,7);LinearLayout actions=row();actions.addView(postAction("heart",(post.optBoolean("liked")?"已赞 ":"赞 ")+post.optInt("likes"),post.optBoolean("liked"),()->command(Wire.obj("op","feed.like","post",id,"enabled",!post.optBoolean("liked")))),new LinearLayout.LayoutParams(0,dp(42),1));
        actions.addView(postAction("chat","评论",false,()->askText("回应这条灵感","写下你的回应","",value->command(Wire.obj("op","feed.comment","post",id,"text",value)))),new LinearLayout.LayoutParams(0,dp(42),1));
        if(wb&&author!=workspace.viewer)actions.addView(postAction("send","私信",false,()->openDirectMessage(author,id)),new LinearLayout.LayoutParams(0,dp(42),1));c.addView(actions);
        JSONArray comments=post.optJSONArray("comments");if(comments!=null&&comments.length()>0){LinearLayout replies=column();replies.setPadding(dp(12),dp(10),dp(12),dp(10));replies.setBackground(bg(BG,12));for(int j=0;j<comments.length();j++){JSONObject comment=comments.optJSONObject(j);if(comment!=null)replies.addView(text(workspace.name(comment.optInt("author"))+"："+comment.optString("text"),12,MUTED,false));}c.addView(replies);}
    }
    private View postAction(String icon,String title,boolean active,Runnable tap){LinearLayout r=row();r.setGravity(Gravity.CENTER);r.addView(icon(icon,active?GREEN:MUTED,18));TextView label=text(title,12,active?GREEN:MUTED,active);label.setPadding(dp(6),0,0,0);r.addView(label);r.setContentDescription(title);r.setOnClickListener(v->tap.run());return r;}
    private void renderWeiboInbox(){
        LinearLayout intro=card(body);intro.addView(text("同频的人，会有回音。",20,INK,true));gap(intro,6);intro.addView(text("从公开微博发起对话，无需添加好友。",12,MUTED,false));
        Map<Integer,JSONObject> latest=new LinkedHashMap<>();JSONArray all=workspace.array("directMessages");for(int i=0;i<all.length();i++){JSONObject m=all.optJSONObject(i);if(m==null)continue;int peer=m.optInt("sender")==workspace.viewer?m.optInt("recipient"):m.optInt("sender");if(!latest.containsKey(peer))latest.put(peer,m);}
        if(latest.isEmpty()){empty("还没有私信","在一条公开微博下点击「私信」，开启对话。");return;}
        for(Map.Entry<Integer,JSONObject> entry:latest.entrySet()){int peer=entry.getKey();JSONObject m=entry.getValue();personRow(body,workspace.name(peer),m.optString("text"),false,!m.optBoolean("read")&&m.optInt("recipient")==workspace.viewer?"新私信":clock(m.optLong("created")),()->openDirectMessage(peer,""));}
    }
    private void openDirectMessage(int target,String post){
        chatKind=2;chatTarget=target;directPost=post;sending=false;lastReadBatch="";base();top(workspace.name(target),"微博私信 · 与此刻同频","返回",()->{hideKeyboard();chatKind=-1;tab=2;weiboInbox=true;showHome();});connectionLabel();scroll=new ScrollView(this);scroll.setVerticalScrollBarEnabled(false);chatMessages=column();scroll.addView(chatMessages);root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));LinearLayout compose=row();composer=input("发送一条私信…",false);composer.setSingleLine(false);composer.setMaxLines(4);compose.addView(composer,new LinearLayout.LayoutParams(0,-2,1));sendButton=button("发送",true,this::send);LinearLayout.LayoutParams size=new LinearLayout.LayoutParams(dp(67),dp(49));size.leftMargin=dp(8);compose.addView(sendButton,size);root.addView(compose);renderDirectMessages();
    }
    private void renderDirectMessages(){
        chatMessages.removeAllViews();List<JSONObject> list=new ArrayList<>();JSONArray all=workspace.array("directMessages");boolean unread=false;for(int i=all.length()-1;i>=0;i--){JSONObject m=all.optJSONObject(i);if(m!=null&&(m.optInt("sender")==chatTarget||m.optInt("recipient")==chatTarget)){list.add(m);if(m.optInt("recipient")==workspace.viewer&&!m.optBoolean("read"))unread=true;}}
        for(JSONObject m:list){boolean own=m.optInt("sender")==workspace.viewer;LinearLayout wrap=column();wrap.setGravity(own?Gravity.RIGHT:Gravity.LEFT);wrap.addView(text((own?"我":workspace.name(chatTarget))+" · "+clock(m.optLong("created")),10,MUTED,false));gap(wrap,5);TextView bubble=text(m.optString("text"),16,own?palette.onAccent:INK,false);bubble.setMaxWidth(dp(400));bubble.setPadding(dp(15),dp(12),dp(15),dp(12));bubble.setBackground(bg(own?GREEN:palette.surface,18));wrap.addView(bubble);gap(wrap,15);chatMessages.addView(wrap);}
        scroll.post(()->scroll.fullScroll(View.FOCUS_DOWN));String token=chatTarget+":"+list.size();if(unread&&!lastReadBatch.equals(token)){lastReadBatch=token;command(Wire.obj("op","weibo.dm.read","target",chatTarget));}
    }
    private void openWeiboService(){
        LinearLayout form=column();form.setPadding(dp(22),dp(8),dp(22),0);form.addView(text("为当前账号设置微博服务密码，开通后可在同一云端切换。",13,MUTED,false));gap(form,12);EditText secret=input("至少 8 字节的微博密码",true);form.addView(secret);AlertDialog d=new AlertDialog.Builder(this).setTitle("开通微博服务").setView(form).setNegativeButton("取消",null).setPositiveButton("开通",null).create();d.setOnShowListener(v->d.getButton(-1).setOnClickListener(b->{String password=secret.getText().toString();if(password.getBytes(StandardCharsets.UTF_8).length<8||password.getBytes(StandardCharsets.UTF_8).length>128){toast("密码需为 8–128 字节");return;}command(Wire.obj("op","account.open","contact",2,"number",String.valueOf(workspace.viewer),"password",password,"shared",false),()->{service=2;tab=2;channel="weibo";weiboInbox=false;showHome();},null);secret.setText("");d.dismiss();}));d.show();
    }

    private void mine(){
        LinearLayout c=card(body);c.addView(text(workspace.name(workspace.viewer),27,INK,true));c.addView(text("云端 ID "+workspace.viewer+" · "+SERVICES[service],13,MUTED,false));gap(c,12);c.addView(text(workspace.users.get(workspace.viewer).optString("location","未设置"),14,MUTED,false));gap(c,16);c.addView(button("切换已开通服务",false,()->{List<Integer> types=new ArrayList<>();List<String> names=new ArrayList<>();for(int s=0;s<3;s++)if(workspace.account(workspace.viewer,s).length()>0){types.add(s);names.add(SERVICES[s]);}new AlertDialog.Builder(this).setTitle("切换服务视图").setItems(names.toArray(new String[0]),(d,w)->{service=types.get(w);tab=service==2?2:0;channel=service==2?"weibo":"moments";weiboInbox=false;weiboFilter=0;prefs.edit().putInt("service",service).apply();showHome();}).show();}));
        if(workspace.account(workspace.viewer,2).length()==0){body.addView(button("开通微博 · 伊冯灵感空间",true,this::openWeiboService));gap(body,12);}LinearLayout settings=card(body);settings.addView(button("编辑个人资料",false,()->askText("个人资料","昵称",workspace.name(workspace.viewer),name->askText("个人资料","所在地",workspace.users.get(workspace.viewer).optString("location"),location->command(Wire.obj("op","profile","name",name,"location",location))))));gap(settings,8);settings.addView(button("我的收藏",false,this::favorites));gap(settings,8);settings.addView(button("服务器与连接",false,()->new AlertDialog.Builder(this).setTitle("当前服务器").setMessage(profile.host+":"+profile.port+"\n\nTLS 1.2 / 1.3 · 严格校验证书\n\nCA SHA-256\n"+profile.fingerprint+"\n\n切换服务器请先退出登录。密码不会保存在手机存储中。").setPositiveButton("知道了",null).show()));
        LinearLayout about=card(body);about.addView(text("听澜 Android 1.1.0",16,INK,true));gap(about,8);about.addView(text("原生安卓 · 与桌面端共用云端\n已支持好友、群聊、动态、收藏、微博关注与私信。\n应用在前台时保持实时连接；本版不含系统离线推送、文件传输、管理后台或支付。\n同一账号当前只能在一台设备登录。",13,MUTED,false));gap(body,4);body.addView(button("退出账号",false,()->new AlertDialog.Builder(this).setTitle("退出账号？").setMessage("云端数据会保留，下次需重新输入密码。").setNegativeButton("取消",null).setPositiveButton("退出",(d,w)->logout()).show()));
    }
    private void favorites(){LinearLayout content=column();content.setPadding(dp(20),0,dp(20),0);JSONArray list=workspace.array("messages");int count=0;for(int i=0;i<list.length();i++){JSONObject m=list.optJSONObject(i);if(m!=null&&!m.optBoolean("recalled")&&Workspace.contains(m.optJSONArray("stars"),workspace.viewer)){count++;LinearLayout c=card(content);c.addView(text(workspace.name(m.optInt("sender"))+" · "+clock(m.optLong("created")),12,MUTED,false));c.addView(text(m.optString("text"),16,INK,false));}}if(count==0)content.addView(text("长按聊天消息即可收藏",15,MUTED,false));ScrollView sc=new ScrollView(this);sc.addView(content);new AlertDialog.Builder(this).setTitle("我的收藏").setView(sc).setPositiveButton("关闭",null).show();}
    private interface TextAction{void accept(String value);}
    private void askText(String title,String hint,String current,TextAction action){LinearLayout c=column();c.setPadding(dp(22),dp(12),dp(22),0);EditText e=input(hint,false);e.setText(current);e.setSingleLine(false);e.setMaxLines(5);e.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_MULTI_LINE);c.addView(e);AlertDialog d=new AlertDialog.Builder(this).setTitle(title).setView(c).setNegativeButton("取消",null).setPositiveButton("提交",null).create();d.setOnShowListener(v->d.getButton(-1).setOnClickListener(b->{String value=e.getText().toString().trim();if(value.isEmpty()){toast("请输入内容");return;}action.accept(value);d.dismiss();}));d.show();}
    private String clock(long seconds){if(seconds<=0)return "";return new SimpleDateFormat("MM-dd HH:mm",Locale.getDefault()).format(new Date(seconds*1000));}
    private void hideKeyboard(){View view=getCurrentFocus();if(view!=null)((InputMethodManager)getSystemService(INPUT_METHOD_SERVICE)).hideSoftInputFromWindow(view.getWindowToken(),0);}
    private void toast(String detail){Toast.makeText(this,detail==null?"操作未完成":detail,Toast.LENGTH_LONG).show();}
}
