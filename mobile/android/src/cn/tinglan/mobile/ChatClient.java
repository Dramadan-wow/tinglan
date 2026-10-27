package cn.tinglan.mobile;

import org.json.JSONObject;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.SocketTimeoutException;
import java.nio.charset.StandardCharsets;
import java.util.Base64;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ArrayBlockingQueue;
import javax.net.ssl.SSLException;
import javax.net.ssl.SSLParameters;
import javax.net.ssl.SSLSocket;

/** Dedicated TLS thread; passwords and snapshots remain in memory. Never replay mutations.
 *  With "remember" the server issues a revocable sign-in token instead; only that token is saved. */
public final class ChatClient implements AutoCloseable {
    public interface Listener {
        void state(String detail,boolean online);
        void snapshot(JSONObject snapshot);
        void result(String id,JSONObject result);
        void failure(String detail,boolean terminal);
        /** The server issued an automatic sign-in token for this phone. */
        default void token(String token,int service){}
        /** The saved token no longer works (signed in elsewhere, revoked or expired): forget it. */
        default void signedOut(){}
        /** Answer to scanning or confirming a desktop sign-in QR code. */
        default void qr(JSONObject result){}
    }
    private final ConnectionProfile profile;
    private final Listener listener;
    private volatile int service;
    private volatile String nickname,password,token;
    private final boolean remember;
    private volatile boolean active=true,online=false,register;
    private volatile SSLSocket socket;
    private final ArrayBlockingQueue<JSONObject> outgoing=new ArrayBlockingQueue<>(64);
    private final Map<String,Long> pending=new HashMap<>();
    private String identity="",loginId="",snapshotId="";
    private int part=0,count=0;
    private final ByteArrayOutputStream snapshotBytes=new ByteArrayOutputStream();
    private Thread worker;
    public ChatClient(ConnectionProfile profile,String nickname,String password,int service,boolean register,Listener listener) {
        this(profile,nickname,password,"",service,register,false,listener);
    }
    /** token non-empty: sign in with a saved token; remember: ask for a token after a password sign-in. */
    public ChatClient(ConnectionProfile profile,String nickname,String password,String token,int service,boolean register,boolean remember,Listener listener) {
        this.profile=profile;this.nickname=nickname;this.password=password;this.token=token==null?"":token;this.service=service;this.register=register;this.remember=remember;this.listener=listener;
    }
    public int service(){return service;}
    /** Send a desktop QR sign-in step: "qr_scan", or "qr_confirm" with approve. */
    public void qr(String type,String code,boolean approve) throws Exception {
        ConnectionProfile.require(online&&active,"尚未连接，无法确认电脑登录");
        JSONObject data=Wire.obj("code",code);if(type.equals("qr_confirm"))data.put("approve",approve);
        ConnectionProfile.require(outgoing.offer(Wire.envelope(type,data,identity,Wire.id())),"发送队列已满，请稍后再试");
    }
    /** Sign out on this phone: the server forgets the saved token, then the connection closes. */
    public void signOut(){
        String saved=token;if(!online||saved.isEmpty()||!outgoing.offer(Wire.envelope("logout",Wire.obj("token",saved),identity,Wire.id())))close();
    }
    public void start(){worker=new Thread(this::run,"tinglan-tls");worker.start();}
    public boolean isOnline(){return online;}
    public synchronized String command(JSONObject data) throws Exception {
        ConnectionProfile.require(online&&active,"尚未连接，操作未发送");
        ConnectionProfile.require(!data.optString("op").startsWith("admin."),"手机普通客户端不支持管理操作");
        if(!data.has("service"))data.put("service",service);
        String id=Wire.id(); JSONObject message=Wire.envelope("workspace_command",data,identity,id);Wire.encode(message);
        ConnectionProfile.require(pending.size()<64&&outgoing.offer(message),"发送队列已满，请稍后再试");pending.put(id,System.currentTimeMillis());return id;
    }
    public void close(){active=false;online=false;password="";token="";outgoing.clear();SSLSocket current=socket;if(current!=null)try{current.close();}catch(Exception ignored){} if(worker!=null)worker.interrupt();}
    private void run(){
        int retry=1000;
        while(active){
            boolean terminal=false;
            try {
                listener.state("正在建立加密连接…",false);
                SSLSocket current=(SSLSocket)profile.tls().getSocketFactory().createSocket();socket=current;
                SSLParameters params=current.getSSLParameters(); params.setEndpointIdentificationAlgorithm("HTTPS");current.setSSLParameters(params);
                java.util.ArrayList<String> protocols=new java.util.ArrayList<>();for(String p:current.getSupportedProtocols())if(p.equals("TLSv1.2")||p.equals("TLSv1.3"))protocols.add(p);current.setEnabledProtocols(protocols.toArray(new String[0]));
                current.connect(new InetSocketAddress(profile.host,profile.port),12000);current.setSoTimeout(15000);current.startHandshake();current.setSoTimeout(1000);
                InputStream in=current.getInputStream();OutputStream out=current.getOutputStream();loginId=Wire.id();identity="";part=count=0;snapshotId="";snapshotBytes.reset();
                boolean saved=!token.isEmpty();
                write(out,Wire.envelope("login",saved?Wire.obj("token",token,"workspace",true,"service",service,"device","mobile","fileTransfer",0)
                    :Wire.obj("username",nickname,"password",password,"workspace",true,"register",register,"service",service,"admin",false,"fileTransfer",0,"device","mobile","remember",remember),"",loginId));
                listener.state(register?"正在注册云端账号…":saved?"正在自动登录…":"正在验证账号…",false);
                Wire.Decoder decoder=new Wire.Decoder();byte[] buffer=new byte[32768];long started=System.currentTimeMillis(),lastRead=started,lastHeartbeat=started;
                while(active){
                    JSONObject next;while((next=outgoing.poll())!=null)write(out,next);
                    try{int n=in.read(buffer);if(n<0)throw new java.io.EOFException();for(JSONObject message:decoder.feed(buffer,n)){handle(message);lastRead=System.currentTimeMillis();}}
                    catch(SocketTimeoutException ignored){}
                    long now=System.currentTimeMillis();
                    if((!online&&now-started>30000)||(online&&now-lastRead>45000))throw new java.io.IOException("连接或同步超时");
                    if(online&&now-lastHeartbeat>=15000){write(out,Wire.envelope("heartbeat",new JSONObject(),identity,Wire.id()));lastHeartbeat=now;retry=1000;}
                    expire(now);
                }
            }catch(Quiet e){terminal=true;}
            catch(Terminal e){terminal=true;if(active)listener.failure(e.getMessage(),true);}
            catch(SSLException|java.security.GeneralSecurityException e){terminal=true;if(active)listener.failure("TLS 证书验证失败，请核对连接码和手机时间",true);}
            catch(org.json.JSONException|java.nio.charset.CharacterCodingException e){terminal=true;if(active)listener.failure("服务器协议或同步内容无效",true);}
            catch(Exception e){if(active&&e instanceof IllegalArgumentException){terminal=true;listener.failure("服务器数据无效："+e.getMessage(),true);}}
            finally{
                online=false;SSLSocket old=socket;socket=null;if(old!=null)try{old.close();}catch(Exception ignored){}
                outgoing.clear();synchronized(this){for(String id:pending.keySet())listener.result(id,Wire.obj("ok",false,"detail","连接中断，结果未知；请同步查看，不会自动重发"));pending.clear();}
            }
            if(terminal){active=false;password="";token="";return;}
            if(active){listener.state("连接断开 · "+retry/1000+" 秒后重连",false);try{Thread.sleep(retry);}catch(InterruptedException ignored){}retry=Math.min(retry*2,30000);}
        }
    }
    private void write(OutputStream out,JSONObject value)throws Exception{out.write(Wire.encode(value));out.flush();}
    private void handle(JSONObject m)throws Exception{
        String type=m.getString("type");JSONObject d=m.getJSONObject("data");
        if(type.equals("login_result")){
            if(online||!d.optString("requestId").equals(loginId))throw new Terminal("登录响应不匹配");
            if(!d.optBoolean("ok")){if(d.optString("code").equals("invalid_token")){token="";listener.signedOut();}throw new Terminal(d.optString("detail","账号或密码不正确"));}
            identity=d.optString("userId");if(!identity.matches("u[1-9][0-9]*")||d.optBoolean("admin")||!d.optBoolean("workspace"))throw new Terminal("服务器身份不匹配");
            service=d.optInt("service",service);
            // Reconnects after a network drop use the token, so the password need not stay in memory.
            String issued=d.optString("token");if(issued.matches("[0-9a-f]{64}")){token=issued;password="";listener.token(issued,service);}
            register=false;online=true;listener.state("已连接 · 正在同步",true);
        }else if(type.equals("workspace_sync")){
            ConnectionProfile.require(online&&m.getString("recipientId").equals(identity),"同步身份不匹配");
            String sid=d.getString("snapshot"),encoded=d.getString("payload");byte[] bytes=Base64.getDecoder().decode(encoded);int p=d.getInt("part"),c=d.getInt("count");
            ConnectionProfile.require(sid.matches("[0-9a-f-]{36}")&&c>0&&c<=700&&p>=0&&p<c&&Base64.getEncoder().encodeToString(bytes).equals(encoded),"同步分段无效");
            if(p==0){snapshotBytes.reset();snapshotId=sid;part=0;count=c;}
            ConnectionProfile.require(p==part&&c==count&&sid.equals(snapshotId)&&snapshotBytes.size()+bytes.length<=16*1024*1024,"同步顺序或大小无效");snapshotBytes.write(bytes);part++;
            if(part==count){JSONObject snapshot=new JSONObject(Wire.utf8(snapshotBytes.toByteArray()));snapshotBytes.reset();if(snapshot.optBoolean("admin")||!identity.equals("u"+snapshot.optInt("viewer"))||snapshot.optInt("version")!=1)throw new Terminal("同步身份不匹配");org.json.JSONArray users=snapshot.getJSONArray("users");for(int i=0;i<users.length();i++){JSONObject u=users.getJSONObject(i);if(u.optInt("id")==snapshot.optInt("viewer"))nickname=u.getString("name");}listener.snapshot(snapshot);listener.state("已连接 · 消息实时同步",true);}
        }else if(type.equals("workspace_result")){
            String id=d.optString("requestId");synchronized(this){if(pending.remove(id)!=null)listener.result(id,d);}
        }else if(type.equals("qr_result")&&online){listener.qr(d);
        }else if(type.equals("logout_result")&&online){active=false;throw new Quiet();
        }else if(type.equals("error")){
            if(d.optString("code").equals("session_revoked")){listener.signedOut();throw new Terminal("此会话已结束，请重新登录");}
            if(d.optString("code").equals("session_replaced")){listener.signedOut();throw new Terminal("账号已在另一部手机上登录，本机已下线");}
            String id=d.optString("requestId"),detail=d.optString("detail",d.optString("code","服务器拒绝请求"));
            synchronized(this){if(pending.remove(id)!=null)listener.result(id,Wire.obj("ok",false,"detail",detail));else throw new Terminal(detail);}
        }else if(!type.equals("heartbeat"))throw new Terminal("不支持的服务器消息");
    }
    private synchronized void expire(long now){java.util.ArrayList<String> expired=new java.util.ArrayList<>();for(Map.Entry<String,Long> e:pending.entrySet())if(now-e.getValue()>20000)expired.add(e.getKey());for(String id:expired){pending.remove(id);listener.result(id,Wire.obj("ok",false,"detail","确认超时，结果未知；请刷新查看，不会自动重发"));}}
    private static final class Terminal extends Exception{Terminal(String text){super(text);}}
    private static final class Quiet extends Exception{}
}
