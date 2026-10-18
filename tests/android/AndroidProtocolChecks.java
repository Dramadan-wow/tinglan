package cn.tinglan.mobile;

import org.json.JSONArray;
import org.json.JSONObject;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.BooleanSupplier;

public final class AndroidProtocolChecks {
    private static int checks=0;
    private static void check(boolean value,String label){if(!value)throw new AssertionError(label);checks++;System.out.println("PASS "+label);}
    private interface Throwing{void run()throws Exception;}
    private static void rejects(Throwing f,String label)throws Exception{boolean rejected=false;try{f.run();}catch(Exception e){rejected=true;}check(rejected,label);}
    private static void waitFor(BooleanSupplier condition,String label)throws Exception{long until=System.currentTimeMillis()+20000;while(!condition.getAsBoolean()&&System.currentTimeMillis()<until)Thread.sleep(40);check(condition.getAsBoolean(),label);}
    private static String code(JSONObject data)throws Exception{byte[] bytes=data.toString().getBytes(StandardCharsets.UTF_8);return "TINGLAN1."+Base64.getUrlEncoder().withoutPadding().encodeToString(bytes)+"."+ConnectionProfile.hex(java.security.MessageDigest.getInstance("SHA-256").digest(bytes)).substring(0,16);}
    private static final class Probe implements ChatClient.Listener,AutoCloseable {
        volatile JSONObject snapshot;volatile String problem="";ChatClient client;final Map<String,JSONObject> results=new ConcurrentHashMap<>();
        Probe(ConnectionProfile profile,String name,boolean register){client=new ChatClient(profile,name,"isolated-test-password-2026",0,register,this);client.start();}
        public void state(String text,boolean online){} public void snapshot(JSONObject data){snapshot=data;}public void result(String id,JSONObject result){results.put(id,result);}public void failure(String text,boolean terminal){problem=text;}
        int user(){return snapshot.optInt("viewer");}
        Workspace model(){try{return new Workspace(snapshot);}catch(Exception e){throw new RuntimeException(e);}}
        JSONObject command(JSONObject data)throws Exception{String id=client.command(data);waitFor(()->results.containsKey(id),"command response: "+data.optString("op"));JSONObject r=results.remove(id);check(r.optBoolean("ok"),"server accepted: "+data.optString("op")+" "+r.optString("detail"));return r;}
        public void close(){client.close();}
    }
    public static void main(String[] args)throws Exception{
        String raw=new String(Files.readAllBytes(Paths.get(args[0])),StandardCharsets.UTF_8).trim();ConnectionProfile profile=ConnectionProfile.parse(raw);
        check(profile.port>0&&!profile.ca.isEmpty(),"desktop connection code imported");
        rejects(()->ConnectionProfile.parse(raw+"x"),"checksum corruption rejected");
        JSONObject base=Wire.obj("version",1,"host",profile.host,"port",profile.port,"ca",profile.ca,"mode","cloud");
        rejects(()->ConnectionProfile.parse(code(new JSONObject(base.toString()).put("tls",false))),"TLS bypass field rejected");
        rejects(()->ConnectionProfile.parse(code(new JSONObject(base.toString()).put("password","hidden"))),"credential field rejected");
        rejects(()->ConnectionProfile.parse(code(new JSONObject(base.toString()).put("host","0.0.0.0"))),"wildcard host rejected");
        rejects(()->ConnectionProfile.parse(code(new JSONObject(base.toString()).put("port",1234.5))),"fractional port rejected");
        rejects(()->ConnectionProfile.parse(code(new JSONObject(base.toString()).put("ca","-----BEGIN PRIVATE KEY-----"))),"private key rejected");
        JSONObject packet=Wire.envelope("heartbeat",Wire.obj("text","你好，安卓 ✓"),"",Wire.id());byte[] framed=Wire.encode(packet);Wire.Decoder decoder=new Wire.Decoder();List<JSONObject> decoded=new ArrayList<>();for(byte b:framed)decoded.addAll(decoder.feed(new byte[]{b},1));check(decoded.size()==1&&decoded.get(0).getJSONObject("data").getString("text").equals("你好，安卓 ✓"),"fragmented UTF-8 frame round trip");
        rejects(()->new Wire.Decoder().feed(new byte[]{0,1,0,1},4),"oversized frame rejected");
        byte[] bad=Wire.encode(new JSONObject(packet.toString()).put("v",2));rejects(()->new Wire.Decoder().feed(bad,bad.length),"wrong protocol version rejected");
        try(Probe a=new Probe(profile,"android_alpha",true);Probe b=new Probe(profile,"android_beta",true)){
            waitFor(()->a.snapshot!=null&&b.snapshot!=null,"two Android protocol clients registered and synchronized");int aid=a.user(),bid=b.user();
            a.command(Wire.obj("op","friend.request","target",bid,"contact",0,"reason","安卓互通测试"));
            waitFor(()->b.model().array("requests").length()>0,"friend request synchronized");String request=b.model().array("requests").getJSONObject(0).getString("id");
            b.command(Wire.obj("op","friend.respond","message",request,"enabled",true));
            waitFor(()->a.model().account(aid,0).optJSONArray("friends").length()==1,"accepted friendship synchronized");
            a.command(Wire.obj("op","chat.send","kind",0,"target",bid,"text","来自安卓的实时消息 ✓"));
            waitFor(()->b.model().history(0,0,aid).size()==1,"private chat reaches peer");
            b.command(Wire.obj("op","chat.send","kind",0,"target",aid,"text","原版服务端双向回复"));
            waitFor(()->a.model().history(0,0,bid).size()==2,"bidirectional history synchronized");
            String mid=a.model().history(0,0,bid).get(0).getString("id");
            a.command(Wire.obj("op","chat.star","message",mid,"enabled",true));
            waitFor(()->Workspace.contains(a.model().history(0,0,bid).get(0).optJSONArray("stars"),aid),"message favorite persisted");
            a.command(Wire.obj("op","group.create","name","安卓互通群","mode",0,"parent",0));
            waitFor(()->!a.model().groups.isEmpty(),"group created and synchronized");int gid=a.model().groups.keySet().iterator().next();
            b.command(Wire.obj("op","group.apply","group",gid));
            a.command(Wire.obj("op","group.approve","group",gid,"target",bid,"contact",0,"enabled",true));
            waitFor(()->b.model().member(b.model().groups.get(gid),0),"group application approved");
            b.command(Wire.obj("op","chat.send","kind",1,"target",gid,"text","安卓群聊消息"));
            waitFor(()->a.model().history(0,1,gid).size()==1,"group chat synchronized");
            a.command(Wire.obj("op","feed.publish","channel","moments","text","手机动态互通验证"));
            waitFor(()->b.model().array("feed").length()==1,"moments feed synchronized");
            String post=b.model().array("feed").getJSONObject(0).getString("id");b.command(Wire.obj("op","feed.like","post",post,"enabled",true));
            waitFor(()->a.model().array("feed").getJSONObject(0).optInt("likes")==1,"feed like synchronized");
            a.command(Wire.obj("op","account.open","contact",2,"number",String.valueOf(aid),"password","isolated-weibo-password","shared",false));
            b.command(Wire.obj("op","account.open","contact",2,"number",String.valueOf(bid),"password","isolated-weibo-password","shared",false));
            a.command(Wire.obj("op","feed.publish","service",2,"channel","weibo","text","伊冯灵感 · 手机微博互通验证"));
            waitFor(()->b.model().array("weibo").length()==1,"public Weibo post synchronized");
            String blog=b.model().array("weibo").getJSONObject(0).getString("id");
            b.command(Wire.obj("op","weibo.follow","service",2,"target",aid,"enabled",true));
            waitFor(()->Workspace.contains(b.model().array("follows"),aid),"Weibo following persisted");
            b.command(Wire.obj("op","weibo.dm.send","service",2,"target",aid,"post",blog,"text","来自公开微博的私信"));
            waitFor(()->a.model().array("directMessages").length()==1,"Weibo direct message received");
            a.command(Wire.obj("op","weibo.dm.read","service",2,"target",bid));
            waitFor(()->a.model().array("directMessages").getJSONObject(0).optBoolean("read"),"Weibo inbox read state persisted");
            a.command(Wire.obj("op","weibo.dm.send","service",2,"target",bid,"post","","text","微博私信双向回复"));
            waitFor(()->b.model().array("directMessages").length()==2,"Weibo direct reply synchronized");
            rejects(()->a.client.command(Wire.obj("op","admin.allow","target",bid)),"admin operation blocked by mobile client");
            a.close();Thread.sleep(500);
            try(Probe again=new Probe(profile,"android_alpha",false)){waitFor(()->again.snapshot!=null,"existing account reconnected");check(again.model().history(0,0,bid).size()==2,"history survives client restart");}
        }
        JSONObject wrongHost=new JSONObject(base.toString()).put("host","localhost");
        try(Probe wrong=new Probe(ConnectionProfile.parse(code(wrongHost)),"android_alpha",false)){waitFor(()->!wrong.problem.isEmpty(),"wrong TLS identity rejected");check(wrong.snapshot==null,"no snapshot after TLS identity failure");}
        if(args.length>1){ConnectionProfile cloud=ConnectionProfile.parse(new String(Files.readAllBytes(Paths.get(args[1])),StandardCharsets.UTF_8));try(javax.net.ssl.SSLSocket socket=(javax.net.ssl.SSLSocket)cloud.tls().getSocketFactory().createSocket()){javax.net.ssl.SSLParameters params=socket.getSSLParameters();params.setEndpointIdentificationAlgorithm("HTTPS");socket.setSSLParameters(params);socket.connect(new java.net.InetSocketAddress(cloud.host,cloud.port),12000);socket.setSoTimeout(12000);socket.startHandshake();check(socket.getSession().getProtocol().startsWith("TLS"),"production cloud TLS and IP identity verified without login");}}
        System.out.println("PASS TOTAL "+checks);
    }
}
