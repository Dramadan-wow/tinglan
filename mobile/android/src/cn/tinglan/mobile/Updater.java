package cn.tinglan.mobile;

import org.json.JSONObject;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.SocketTimeoutException;
import java.security.MessageDigest;
import java.util.Base64;
import javax.net.ssl.SSLException;
import javax.net.ssl.SSLParameters;
import javax.net.ssl.SSLSocket;

/**
 * The server's Android update feed, on its own TLS connection like the desktop updater:
 * update_check, then update_chunk requests at strictly increasing offsets. The APK is kept only
 * when its size and SHA-256 match the release the server announced.
 */
public final class Updater {
    public static final long MAX_SIZE=256L*1024*1024;
    public interface Listener {
        default void current(boolean published){}
        default void available(JSONObject release){}
        default void progress(long received,long total){}
        default void ready(File apk,JSONObject release){}
        void failed(String detail);
    }
    private final ConnectionProfile profile;
    private final String version;
    private final Listener listener;
    private volatile boolean active=true;
    private volatile SSLSocket socket;

    public Updater(ConnectionProfile profile,String version,Listener listener){this.profile=profile;this.version=version;this.listener=listener;}
    public void check(){start(null,null);}
    public void download(JSONObject release,File folder){start(release,folder);}
    public void cancel(){active=false;SSLSocket current=socket;if(current!=null)try{current.close();}catch(Exception ignored){}}
    private void start(JSONObject wanted,File folder){Thread t=new Thread(()->run(wanted,folder),"tinglan-update");t.setDaemon(true);t.start();}

    public static boolean validRelease(JSONObject r){
        if(r==null)return false;
        double size=r.optDouble("size",-1);
        return r.optInt("schema")==1&&"client".equals(r.optString("edition"))&&"android".equals(r.optString("platform"))&&validVersion(r.optString("version"))
            &&size>=1024&&size<=MAX_SIZE&&size==Math.floor(size)&&r.optString("sha256").matches("[0-9a-f]{64}")
            &&r.opt("notes") instanceof String&&r.optString("notes").length()<=4000;
    }
    public static boolean validVersion(String v){return v!=null&&v.matches("(0|[1-9][0-9]{0,3})\\.(0|[1-9][0-9]{0,3})\\.(0|[1-9][0-9]{0,3})");}
    public static boolean newer(String candidate,String current){
        if(!validVersion(candidate)||!validVersion(current))return false;
        String[] a=candidate.split("\\."),b=current.split("\\.");
        for(int i=0;i<3;i++){int x=Integer.parseInt(a[i]),y=Integer.parseInt(b[i]);if(x!=y)return x>y;}
        return false;
    }
    public static String apkName(String version){return "Tinglan-Android-"+version+".apk";}
    /** A previously downloaded APK is reused only while it still matches the announced hash. */
    public static boolean matches(File apk,JSONObject release){
        if(apk==null||!apk.isFile()||apk.length()!=release.optLong("size"))return false;
        try(InputStream in=new java.io.FileInputStream(apk)){MessageDigest d=MessageDigest.getInstance("SHA-256");byte[] b=new byte[65536];int n;while((n=in.read(b))>0)d.update(b,0,n);return ConnectionProfile.hex(d.digest()).equals(release.optString("sha256"));}
        catch(Exception e){return false;}
    }

    private void run(JSONObject wanted,File folder){
        File part=null;
        try{
            SSLSocket current=(SSLSocket)profile.tls().getSocketFactory().createSocket();socket=current;
            SSLParameters params=current.getSSLParameters();params.setEndpointIdentificationAlgorithm("HTTPS");current.setSSLParameters(params);
            java.util.ArrayList<String> protocols=new java.util.ArrayList<>();for(String p:current.getSupportedProtocols())if(p.equals("TLSv1.2")||p.equals("TLSv1.3"))protocols.add(p);current.setEnabledProtocols(protocols.toArray(new String[0]));
            current.connect(new InetSocketAddress(profile.host,profile.port),12000);current.setSoTimeout(20000);current.startHandshake();
            InputStream in=current.getInputStream();OutputStream out=current.getOutputStream();Wire.Decoder decoder=new Wire.Decoder();
            String id=Wire.id();write(out,Wire.envelope("update_check",Wire.obj("edition","client","platform","android","version",version),"",id));
            JSONObject info=await(in,decoder,"update_info",id);JSONObject release=info.optJSONObject("release");
            boolean available=info.optBoolean("available")&&validRelease(release)&&newer(release.optString("version"),version);
            if(wanted==null){if(available)listener.available(release);else listener.current(info.optBoolean("published"));return;}
            if(!available||!release.optString("sha256").equals(wanted.optString("sha256")))throw new IllegalStateException("服务器上的新版本已变更，请重新检查更新");
            long size=release.getLong("size");String sha=release.getString("sha256");
            ConnectionProfile.require(folder.isDirectory()||folder.mkdirs(),"无法创建下载目录");
            File[] old=folder.listFiles();if(old!=null)for(File f:old)f.delete();
            File target=new File(folder,apkName(release.getString("version")));part=new File(folder,target.getName()+".part");
            MessageDigest digest=MessageDigest.getInstance("SHA-256");long received=0;
            try(FileOutputStream file=new FileOutputStream(part)){
                while(received<size){
                    if(!active)throw new InterruptedException();
                    String chunkId=Wire.id();write(out,Wire.envelope("update_chunk",Wire.obj("sha256",sha,"offset",received),"",chunkId));
                    JSONObject chunk=await(in,decoder,"update_chunk",chunkId);
                    ConnectionProfile.require(chunk.optDouble("offset",-1)==received&&chunk.opt("payload") instanceof String,"服务器返回的数据顺序无效");
                    byte[] bytes=Base64.getDecoder().decode(chunk.getString("payload"));
                    ConnectionProfile.require(bytes.length>0&&bytes.length<=32768&&received+bytes.length<=size,"服务器返回的数据长度无效");
                    file.write(bytes);digest.update(bytes);received+=bytes.length;listener.progress(received,size);
                }
                file.getFD().sync();
            }
            ConnectionProfile.require(ConnectionProfile.hex(digest.digest()).equals(sha),"安装包 SHA-256 校验失败，已丢弃");
            ConnectionProfile.require(part.renameTo(target),"无法保存安装包");part=null;
            if(active)listener.ready(target,release);
        }catch(InterruptedException e){/* cancelled by the user */}
        catch(SSLException|java.security.GeneralSecurityException e){if(active)listener.failed("TLS 证书验证失败，请核对连接码和手机时间");}
        catch(SocketTimeoutException e){if(active)listener.failed("连接服务器超时，请稍后再试");}
        catch(IllegalArgumentException|IllegalStateException e){if(active)listener.failed(e.getMessage());}
        catch(Exception e){if(active)listener.failed("无法连接更新服务器，请检查网络后重试");}
        finally{
            SSLSocket current=socket;socket=null;if(current!=null)try{current.close();}catch(Exception ignored){}
            if(part!=null)part.delete();
        }
    }
    private static void write(OutputStream out,JSONObject value)throws Exception{out.write(Wire.encode(value));out.flush();}
    private JSONObject await(InputStream in,Wire.Decoder decoder,String type,String id)throws Exception{
        byte[] buffer=new byte[65536];
        while(active){
            int n=in.read(buffer);if(n<0)throw new IllegalStateException("更新服务器断开了连接");
            for(JSONObject m:decoder.feed(buffer,n)){
                JSONObject d=m.getJSONObject("data");
                if(m.getString("type").equals("error"))throw new IllegalStateException(explain(d.optString("code")));
                if(m.getString("type").equals(type)&&d.optString("requestId").equals(id))return d;
            }
        }
        throw new InterruptedException();
    }
    private static String explain(String code){
        switch(code){
            case "invalid_update_check":return "服务器还没有开启安卓更新，请联系服务器维护者";
            case "invalid_update_chunk":return "服务器上的安装包已变更，请重新检查更新";
            case "update_read_failed":return "服务器读取安装包失败，请稍后再试";
            default:return "服务器拒绝了更新请求（"+code+"）";
        }
    }
}
