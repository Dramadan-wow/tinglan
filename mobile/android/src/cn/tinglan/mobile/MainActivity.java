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
    private static final int BG=0xfff4f7f5,INK=0xff173a2f,GREEN=0xff24745d,MUTED=0xff72837c,LINE=0xffe2eae5;
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
    @Override public void onCreate(Bundle saved){super.onCreate(saved);prefs=getSharedPreferences("connection",MODE_PRIVATE);service=prefs.getInt("service",0);try{String code=prefs.getString("code","");if(!code.isEmpty())profile=ConnectionProfile.parse(code);}catch(Exception e){profile=null;}showLogin();}
    @Override protected void onDestroy(){destroyed=true;if(client!=null)client.close();super.onDestroy();}
    @Override public void onBackPressed(){if(chatKind>=0){hideKeyboard();chatKind=-1;showHome();}else if(workspace!=null)new AlertDialog.Builder(this).setTitle("离开听澜？").setMessage("聊天记录保存在服务器，应用回到后台后可能暂停连接。").setPositiveButton("回到桌面",(d,w)->moveTaskToBack(true)).setNegativeButton("取消",null).show();else super.onBackPressed();}
    private int dp(float value){return (int)(value*getResources().getDisplayMetrics().density+.5f);}
    private GradientDrawable bg(int color,float radius){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(radius));return d;}
    private GradientDrawable outline(int color,float radius){GradientDrawable d=bg(color,radius);d.setStroke(dp(1),LINE);return d;}
    private LinearLayout column(){LinearLayout v=new LinearLayout(this);v.setOrientation(LinearLayout.VERTICAL);return v;}
    private LinearLayout row(){LinearLayout v=new LinearLayout(this);v.setOrientation(LinearLayout.HORIZONTAL);v.setGravity(Gravity.CENTER_VERTICAL);return v;}
    private TextView text(String value,int size,int color,boolean bold){TextView t=new TextView(this);t.setText(value);t.setTextSize(size);t.setTextColor(color);if(bold)t.setTypeface(Typeface.DEFAULT,Typeface.BOLD);t.setLineSpacing(dp(3),1);return t;}
    private void gap(LinearLayout v,int height){Space s=new Space(this);v.addView(s,new LinearLayout.LayoutParams(1,dp(height)));}
    private Button button(String title,boolean primary,Runnable action){Button b=new Button(this);b.setAllCaps(false);b.setText(title);b.setTextSize(15);b.setTypeface(Typeface.DEFAULT,Typeface.BOLD);b.setTextColor(primary?Color.WHITE:GREEN);b.setBackground(outline(primary?GREEN:Color.WHITE,16));b.setPadding(dp(12),dp(8),dp(12),dp(8));b.setMinHeight(dp(48));b.setOnClickListener(v->action.run());return b;}
    private EditText input(String hint,boolean secret){EditText e=new EditText(this);e.setHint(hint);e.setTextSize(16);e.setSingleLine(true);e.setTextColor(INK);e.setHintTextColor(MUTED);e.setPadding(dp(16),dp(14),dp(16),dp(14));e.setBackground(outline(BG,14));e.setInputType(InputType.TYPE_CLASS_TEXT|(secret?InputType.TYPE_TEXT_VARIATION_PASSWORD:InputType.TYPE_TEXT_FLAG_CAP_SENTENCES));e.setSaveEnabled(false);e.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO);return e;}
    private void field(LinearLayout parent,String label,EditText e){parent.addView(text(label,13,MUTED,false));gap(parent,8);parent.addView(e,new LinearLayout.LayoutParams(-1,dp(54)));gap(parent,18);}
    private LinearLayout card(LinearLayout parent){LinearLayout c=column();c.setPadding(dp(18),dp(18),dp(18),dp(18));c.setBackground(bg(Color.WHITE,22));LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);lp.bottomMargin=dp(12);parent.addView(c,lp);return c;}
    private void base(){root=column();root.setBackgroundColor(BG);root.setFitsSystemWindows(true);root.setPadding(dp(22),dp(8),dp(22),dp(8));setContentView(root);root.setOnApplyWindowInsetsListener((v,insets)->{v.setPadding(dp(22),dp(8)+insets.getSystemWindowInsetTop(),dp(22),dp(8)+insets.getSystemWindowInsetBottom());return insets.consumeSystemWindowInsets();});}
    private void makeScroll(){scroll=new ScrollView(this);scroll.setFillViewport(true);scroll.setClipToPadding(false);body=column();body.setPadding(0,dp(8),0,dp(16));scroll.addView(body,new ScrollView.LayoutParams(-1,-2));root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));}
    private void top(String title,String sub,String action,Runnable tap){LinearLayout h=row();LinearLayout labels=column();labels.addView(text(title,28,INK,true));if(sub!=null)labels.addView(text(sub,12,MUTED,false));h.addView(labels,new LinearLayout.LayoutParams(0,-2,1));if(action!=null)h.addView(button(action,false,tap),new LinearLayout.LayoutParams(-2,dp(44)));root.addView(h);gap(root,10);}
    private void showLogin(){
        base();top("听澜","TINGLAN  /  随时在你身边","连接",this::connectionDialog);makeScroll();gap(body,24);
        body.addView(text("把想说的话，\n交给此刻。",34,INK,true));gap(body,12);body.addView(text("与电脑端共用同一云端账号\n好友、群聊和消息，实时同步。",15,MUTED,false));gap(body,28);
        LinearLayout c=card(body);LinearLayout choices=row();
        for(int i=0;i<3;i++){final int s=i;Button b=button(SERVICES[i],service==i,()->{if(!connecting){service=s;showLogin();}});LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(0,dp(46),1);lp.setMargins(dp(3),0,dp(3),0);choices.addView(b,lp);}c.addView(choices);gap(c,22);
        nickname=input("输入注册时的昵称",false);nickname.setText(prefs.getString("nickname",""));password=input(register?"设置密码（至少 8 字节）":"输入云端账号密码",true);field(c,"昵称",nickname);field(c,"密码",password);
        loginButton=button(register?"创建账号并登录":"登录听澜",true,()->login());c.addView(loginButton,new LinearLayout.LayoutParams(-1,dp(52)));gap(c,12);
        Button mode=button(register?"已有账号？返回登录":"还没有账号？注册",false,()->{if(!connecting){prefs.edit().putString("nickname",nickname.getText().toString().trim()).apply();register=!register;showLogin();}});c.addView(mode);
        gap(body,6);status=text(connecting?connectionState:profile==null?"先导入服务器连接码，再登录":"加密连接 · "+profile.host+":"+profile.port,12,MUTED,false);body.addView(status);gap(body,14);
        TextView privacy=text("密码仅用于当前服务器认证，保留在本次会话内存中。退出后需重新输入；不上传到其他服务。",12,MUTED,false);body.addView(privacy);
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
            public void snapshot(JSONObject data){ui(()->{try{workspace=new Workspace(data);connecting=false;register=false;String name=workspace.name(workspace.viewer);prefs.edit().putString("nickname",name).apply();if(workspace.account(workspace.viewer,service).length()==0){JSONArray all=workspace.users.get(workspace.viewer).getJSONArray("services");service=all.getJSONObject(0).getInt("type");}if(chatKind>=0)renderMessages();else showHome();}catch(Exception e){logout();toast("同步失败："+e.getMessage());}});}
            public void result(String id,JSONObject result){ui(()->{Runnable done=completions.remove(id);if(done!=null)done.run();Runnable good=successes.remove(id);if(result.optBoolean("ok")){if(good!=null)good.run();}else toast(result.optString("detail","服务器拒绝操作"));});}
            public void failure(String detail,boolean terminal){ui(()->{if(terminal){logout();new AlertDialog.Builder(MainActivity.this).setTitle("连接未完成").setMessage(detail).setPositiveButton("知道了",null).show();}else toast(detail);});}
        });holder[0]=next;client=next;next.start();
    }
    private void logout(){if(client!=null){client.close();client=null;}workspace=null;connecting=false;sending=false;chatKind=-1;sendRequest="";composer=null;sendButton=null;successes.clear();completions.clear();showLogin();}
    private String command(JSONObject data,Runnable good,Runnable done){try{if(client==null)throw new IllegalStateException("请先登录");String id=client.command(data);if(good!=null)successes.put(id,good);if(done!=null)completions.put(id,done);return id;}catch(Exception e){toast(e.getMessage());if(done!=null)done.run();return "";}}
    private void command(JSONObject data){command(data,null,null);}
    private void showHome(){
        if(workspace==null){showLogin();return;}chatKind=-1;composer=null;sendButton=null;base();String[] titles={"消息","通讯录","发现","我的"};top(titles[tab],"听澜  /  "+SERVICES[service],tab==1?"添加":"刷新",()->{if(tab==1)addFriend();else command(Wire.obj("op","sync"));});
        status=text(connectionState,12,MUTED,false);root.addView(status);gap(root,6);makeScroll();if(tab==0)conversations();else if(tab==1)contacts();else if(tab==2)feed();else mine();
        LinearLayout nav=row();nav.setPadding(0,dp(10),0,0);for(int i=0;i<4;i++){final int t=i;Button b=button(new String[]{"◌ 消息","♧ 联系人","☷ 发现","○ 我的"}[i],tab==i,()->{tab=t;showHome();});b.setTextSize(12);LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(0,dp(48),1);lp.setMargins(dp(2),0,dp(2),0);nav.addView(b,lp);}root.addView(nav);
    }
    private TextView avatar(String title,boolean group){TextView a=text(title.isEmpty()?"澜":title.substring(0,1),20,GREEN,true);a.setGravity(Gravity.CENTER);a.setBackground(bg(group?0xffe6eee2:0xffe0eee7,17));return a;}
    private void personRow(LinearLayout parent,String title,String subtitle,boolean group,String badge,Runnable action){LinearLayout r=row();r.setPadding(dp(14),dp(14),dp(14),dp(14));r.setBackground(bg(Color.WHITE,20));r.addView(avatar(title,group),new LinearLayout.LayoutParams(dp(50),dp(50)));LinearLayout labels=column();labels.setPadding(dp(13),0,dp(8),0);labels.addView(text(title,16,INK,true));TextView sub=text(subtitle,12,MUTED,false);sub.setMaxLines(2);sub.setEllipsize(android.text.TextUtils.TruncateAt.END);labels.addView(sub);r.addView(labels,new LinearLayout.LayoutParams(0,-2,1));if(!badge.isEmpty())r.addView(text(badge,11,GREEN,true));LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);lp.bottomMargin=dp(9);parent.addView(r,lp);r.setOnClickListener(v->action.run());}
    private void empty(String title,String detail){gap(body,30);TextView symbol=text("≈",64,GREEN,false);symbol.setGravity(Gravity.CENTER);body.addView(symbol);TextView t=text(title,20,INK,true);t.setGravity(Gravity.CENTER);body.addView(t);gap(body,10);TextView d=text(detail,14,MUTED,false);d.setGravity(Gravity.CENTER);body.addView(d);}
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
        if(workspace==null||chatKind<0||chatMessages==null)return;List<JSONObject> all=workspace.history(service,chatKind,chatTarget);boolean nearBottom=scroll.getChildAt(0).getHeight()-scroll.getHeight()-scroll.getScrollY()<dp(100);chatMessages.removeAllViews();JSONArray unread=new JSONArray();
        if(all.size()>300)chatMessages.addView(text("显示最近 300 条消息，完整记录仍保存在服务器",11,MUTED,false));
        for(int i=Math.max(0,all.size()-300);i<all.size();i++){JSONObject m=all.get(i);boolean own=m.optInt("sender")==workspace.viewer;LinearLayout wrap=column();wrap.setGravity(own?Gravity.RIGHT:Gravity.LEFT);TextView name=text((own?"我":workspace.name(m.optInt("sender")))+" · "+clock(m.optLong("created")),11,MUTED,false);wrap.addView(name);gap(wrap,4);TextView bubble=text(m.optBoolean("recalled")?"消息已撤回":m.optString("text"),16,own?Color.WHITE:INK,false);bubble.setMaxWidth(dp(285));bubble.setPadding(dp(15),dp(11),dp(15),dp(11));bubble.setBackground(bg(own?GREEN:Color.WHITE,17));bubble.setTextIsSelectable(true);wrap.addView(bubble);gap(wrap,14);chatMessages.addView(wrap);bubble.setOnLongClickListener(v->{messageActions(m);return true;});if(!own&&!m.optBoolean("recalled")&&!Workspace.contains(m.optJSONArray("read"),workspace.viewer))unread.put(m.optString("id"));}
        if(all.isEmpty()){TextView t=text("从这一句问候开始",13,MUTED,false);t.setGravity(Gravity.CENTER);chatMessages.addView(t);}
        if(nearBottom||all.size()<5)scroll.post(()->scroll.fullScroll(View.FOCUS_DOWN));String batch=unread.toString();if(unread.length()>0&&!batch.equals(lastReadBatch)&&client!=null&&client.isOnline()){lastReadBatch=batch;command(Wire.obj("op","chat.read","messages",unread));}
    }
    private void send(){if(composer==null||sending)return;String value=composer.getText().toString();if(value.trim().isEmpty())return;if(value.getBytes(StandardCharsets.UTF_8).length>5500){toast("消息过长，请控制在 5500 字节内");return;}sending=true;sendButton.setEnabled(false);EditText editor=composer;Button action=sendButton;sendRequest=command(Wire.obj("op","chat.send","kind",chatKind,"target",chatTarget,"text",value),()->{if(editor.getText().toString().equals(value))editor.setText("");},()->{sending=false;if(action==sendButton)action.setEnabled(client!=null&&client.isOnline());});}
    private void messageActions(JSONObject m){if(m.optBoolean("recalled"))return;List<String> actions=new ArrayList<>();actions.add("复制文字");actions.add(Workspace.contains(m.optJSONArray("stars"),workspace.viewer)?"取消收藏":"收藏消息");if(m.optInt("sender")==workspace.viewer)actions.add("撤回消息");new AlertDialog.Builder(this).setItems(actions.toArray(new String[0]),(d,w)->{if(w==0){((ClipboardManager)getSystemService(CLIPBOARD_SERVICE)).setPrimaryClip(ClipData.newPlainText("听澜消息",m.optString("text")));toast("已复制");}else if(w==1)command(Wire.obj("op","chat.star","message",m.optString("id"),"enabled",!Workspace.contains(m.optJSONArray("stars"),workspace.viewer)));else command(Wire.obj("op","chat.recall","message",m.optString("id")));}).show();}
    private void groupInfo(){
        JSONObject group=workspace.groups.get(chatTarget);if(group==null)return;final int groupId=chatTarget;boolean manager=group.optInt("owner")==workspace.viewer||Workspace.contains(group.optJSONArray("admins"),workspace.viewer);
        LinearLayout content=column();content.setPadding(dp(20),dp(8),dp(20),0);content.addView(text(group.optString("name"),22,INK,true));content.addView(text("群号 "+groupId+"\n"+group.optString("notice","暂无公告"),14,MUTED,false));gap(content,16);
        JSONArray members=group.optJSONArray("members");if(members!=null)for(int i=0;i<members.length();i++){JSONObject member=members.optJSONObject(i);if(member!=null)content.addView(text(workspace.name(member.optInt("id"))+(member.optInt("id")==group.optInt("owner")?" · 群主":""),15,INK,false));}
        if(manager){gap(content,16);content.addView(text("待处理的入群申请",14,MUTED,true));JSONArray pending=group.optJSONArray("pending");if(pending!=null)for(int i=0;i<pending.length();i++){JSONArray request=pending.optJSONArray(i);if(request==null)continue;int user=request.optInt(0),contact=request.optInt(1);LinearLayout row=card(content);row.addView(text(workspace.name(user),16,INK,true));row.addView(button("同意加入",true,()->command(Wire.obj("op","group.approve","group",groupId,"target",user,"contact",contact,"enabled",true))));row.addView(button("拒绝",false,()->command(Wire.obj("op","group.approve","group",groupId,"target",user,"contact",contact,"enabled",false))));}}
        ScrollView s=new ScrollView(this);s.addView(content);new AlertDialog.Builder(this).setTitle("群资料").setView(s).setPositiveButton("关闭",null).show();
    }
    private void feed(){
        LinearLayout switches=row();switches.addView(button("朋友圈",channel.equals("moments"),()->{channel="moments";showHome();}),new LinearLayout.LayoutParams(0,dp(46),1));switches.addView(button("微博",channel.equals("weibo"),()->{channel="weibo";showHome();}),new LinearLayout.LayoutParams(0,dp(46),1));body.addView(switches);gap(body,12);body.addView(button("写一条新动态",true,()->askText("发布动态","分享此刻的想法","",value->command(Wire.obj("op","feed.publish","channel",channel,"text",value)))));gap(body,16);
        JSONArray posts=workspace.array(channel.equals("weibo")?"weibo":"feed");if(posts.length()==0){empty("这里还很安静","好友的新动态会出现在这里。");return;}for(int i=0;i<posts.length();i++){JSONObject p=posts.optJSONObject(i);if(p==null)continue;LinearLayout c=card(body);c.addView(text(workspace.name(p.optInt("author")),16,INK,true));c.addView(text(clock(p.optLong("created")),11,MUTED,false));gap(c,10);c.addView(text(p.optString("text"),16,INK,false));String image=p.optString("image");if(!image.isEmpty()&&image.length()<180000){try{byte[] bytes=java.util.Base64.getDecoder().decode(image);BitmapFactory.Options opts=new BitmapFactory.Options();opts.inSampleSize=2;Bitmap bitmap=BitmapFactory.decodeByteArray(bytes,0,bytes.length,opts);if(bitmap!=null){ImageView view=new ImageView(this);view.setImageBitmap(bitmap);view.setAdjustViewBounds(true);c.addView(view,new LinearLayout.LayoutParams(-1,dp(180)));}}catch(Exception ignored){}}
            gap(c,12);LinearLayout actions=row();String post=p.optString("id");actions.addView(button((p.optBoolean("liked")?"已赞 ":"赞 ")+p.optInt("likes"),false,()->command(Wire.obj("op","feed.like","post",post,"enabled",!p.optBoolean("liked")))),new LinearLayout.LayoutParams(0,dp(42),1));actions.addView(button("评论",false,()->askText("评论","写下评论","",value->command(Wire.obj("op","feed.comment","post",post,"text",value)))),new LinearLayout.LayoutParams(0,dp(42),1));c.addView(actions);
            JSONArray comments=p.optJSONArray("comments");if(comments!=null)for(int j=0;j<comments.length();j++){JSONObject comment=comments.optJSONObject(j);if(comment!=null)c.addView(text(workspace.name(comment.optInt("author"))+"："+comment.optString("text"),13,MUTED,false));}
        }
    }
    private void mine(){
        LinearLayout c=card(body);c.addView(text(workspace.name(workspace.viewer),27,INK,true));c.addView(text("云端 ID "+workspace.viewer+" · "+SERVICES[service],13,MUTED,false));gap(c,12);c.addView(text(workspace.users.get(workspace.viewer).optString("location","未设置"),14,MUTED,false));gap(c,16);c.addView(button("切换已开通服务",false,()->{List<Integer> types=new ArrayList<>();List<String> names=new ArrayList<>();for(int s=0;s<3;s++)if(workspace.account(workspace.viewer,s).length()>0){types.add(s);names.add(SERVICES[s]);}new AlertDialog.Builder(this).setTitle("切换服务视图").setItems(names.toArray(new String[0]),(d,w)->{service=types.get(w);prefs.edit().putInt("service",service).apply();showHome();}).show();}));
        LinearLayout settings=card(body);settings.addView(button("编辑个人资料",false,()->askText("个人资料","昵称",workspace.name(workspace.viewer),name->askText("个人资料","所在地",workspace.users.get(workspace.viewer).optString("location"),location->command(Wire.obj("op","profile","name",name,"location",location))))));gap(settings,8);settings.addView(button("我的收藏",false,this::favorites));gap(settings,8);settings.addView(button("服务器与连接",false,()->new AlertDialog.Builder(this).setTitle("当前服务器").setMessage(profile.host+":"+profile.port+"\n\nTLS 1.2 / 1.3 · 严格校验证书\n\nCA SHA-256\n"+profile.fingerprint+"\n\n切换服务器请先退出登录。密码不会保存在手机存储中。").setPositiveButton("知道了",null).show()));
        LinearLayout about=card(body);about.addView(text("听澜 Android 1.0.0",16,INK,true));gap(about,8);about.addView(text("原生安卓 · 与桌面端共用云端\n已支持好友、私聊、群聊、动态和收藏。\n应用在前台时保持实时连接；本版不含系统离线推送、文件传输、管理后台或支付。\n同一账号当前只能在一台设备登录。",13,MUTED,false));gap(body,4);body.addView(button("退出账号",false,()->new AlertDialog.Builder(this).setTitle("退出账号？").setMessage("云端数据会保留，下次需重新输入密码。").setNegativeButton("取消",null).setPositiveButton("退出",(d,w)->logout()).show()));
    }
    private void favorites(){LinearLayout content=column();content.setPadding(dp(20),0,dp(20),0);JSONArray list=workspace.array("messages");int count=0;for(int i=0;i<list.length();i++){JSONObject m=list.optJSONObject(i);if(m!=null&&!m.optBoolean("recalled")&&Workspace.contains(m.optJSONArray("stars"),workspace.viewer)){count++;LinearLayout c=card(content);c.addView(text(workspace.name(m.optInt("sender"))+" · "+clock(m.optLong("created")),12,MUTED,false));c.addView(text(m.optString("text"),16,INK,false));}}if(count==0)content.addView(text("长按聊天消息即可收藏",15,MUTED,false));ScrollView sc=new ScrollView(this);sc.addView(content);new AlertDialog.Builder(this).setTitle("我的收藏").setView(sc).setPositiveButton("关闭",null).show();}
    private interface TextAction{void accept(String value);}
    private void askText(String title,String hint,String current,TextAction action){LinearLayout c=column();c.setPadding(dp(22),dp(12),dp(22),0);EditText e=input(hint,false);e.setText(current);e.setSingleLine(false);e.setMaxLines(5);e.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_MULTI_LINE);c.addView(e);AlertDialog d=new AlertDialog.Builder(this).setTitle(title).setView(c).setNegativeButton("取消",null).setPositiveButton("提交",null).create();d.setOnShowListener(v->d.getButton(-1).setOnClickListener(b->{String value=e.getText().toString().trim();if(value.isEmpty()){toast("请输入内容");return;}action.accept(value);d.dismiss();}));d.show();}
    private String clock(long seconds){if(seconds<=0)return "";return new SimpleDateFormat("MM-dd HH:mm",Locale.getDefault()).format(new Date(seconds*1000));}
    private void hideKeyboard(){View view=getCurrentFocus();if(view!=null)((InputMethodManager)getSystemService(INPUT_METHOD_SERVICE)).hideSoftInputFromWindow(view.getWindowToken(),0);}
    private void toast(String detail){Toast.makeText(this,detail==null?"操作未完成":detail,Toast.LENGTH_LONG).show();}
}
